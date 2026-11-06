/*
Patch WCX - Total Commander packer plugin that allows .diff and .patch files
to be opened as virtual archives, exposing the hierarchy of affected files.

Copyright (C) 2026 Adam Sawicki

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#include "git_parser.hpp"
#include "unified_parser.hpp"
#include "../patch_archive.hpp"
#include "../patch_error.hpp"
#include "../patch_paths.hpp"
#include "../text_buffer.hpp"

namespace patch_backend::detail {
namespace {

bool IsObjectId(str_view value) {
    if(value.length() != 40 && value.length() != 64) return false;
    for(const char ch : value) {
        if(!((ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f') || (ch >= 'A' && ch <= 'F'))) return false;
    }
    return true;
}

// Skips at most one document-level commit preamble, never arbitrary leading text.
size_t GitPatchStart(const TextBuffer& text) {
    size_t line = SkipBlankLines(text, 0);
    if(line < text.GetLineCount() && IsGitCommitPreamble(text.GetLine(line))) {
        line = SkipBlankLines(text, line + 1);
    }
    if(line == text.GetLineCount()) Invalid(text, line, "Missing Git file section");
    return line;
}

struct GitHeaders {
    str_view names;
    std::optional<std::string> old_file;
    std::optional<std::string> new_file;
    std::optional<std::string> from;
    std::optional<std::string> to;
    bool rename = false;
    bool copy = false;
    bool added = false;
    bool deleted = false;
    bool old_mode = false;
    bool new_mode = false;
    bool metadata = false;
};

struct PathPair {
    std::string old_path;
    std::string new_path;
};

// Decodes a complete filename field; unquoted spaces are part of the name.
std::string ReadPath(str_view value, size_t offset) {
    if(value.empty()) throw PatchError(ErrorCode::kInvalidPatch, "Empty Git path", offset);
    if(!value.starts_with('"')) return value.to_string();
    size_t position = 0;
    std::string result = ReadQuotedPath(value, position, offset);
    if(result.empty()) throw PatchError(ErrorCode::kInvalidPatch, "Empty Git path", offset);
    if(position != value.length()) {
        throw PatchError(ErrorCode::kInvalidPatch, "Unexpected text after Git path", offset);
    }
    return result;
}

// Matches a name of known length, avoiding whitespace splitting of unquoted filenames.
bool MatchName(str_view value, size_t& position, str_view expected, size_t offset) {
    if(position < value.length() && value[position] == '"') {
        return expected == ReadQuotedPath(value, position, offset);
    }
    if(!value.substr(position).starts_with(expected)) return false;
    position += expected.length();
    return true;
}

bool HeaderMatches(str_view names, str_view old_path, str_view new_path, size_t offset) {
    size_t position = 0;
    if(!MatchName(names, position, old_path, offset) || position == names.length() ||
       names[position++] != ' ') return false;
    return MatchName(names, position, new_path, offset) && position == names.length();
}

// Explicit rename/copy paths anchor suffixes; preceding text is a display prefix.
// Quoting or the complete old-path suffix locates the boundary without splitting filenames on spaces.
bool HeaderSuffixesMatch(str_view names, str_view old_path, str_view new_path, size_t offset) {
    if(names.starts_with('"')) {
        size_t position = 0;
        const std::string old_name = ReadQuotedPath(names, position, offset);
        return str_view(old_name).ends_with(old_path) && position < names.length() &&
               names[position] == ' ' &&
               str_view(ReadPath(names.substr(position + 1), offset)).ends_with(new_path);
    }
    const std::string separator = old_path.to_string() + " ";
    size_t position = names.find(separator);
    while(position != SIZE_MAX) {
        const str_view new_name = names.substr(position + separator.length());
        if(!new_name.empty() && str_view(ReadPath(new_name, offset)).ends_with(new_path)) return true;
        position = names.find(separator, position + 1);
    }
    return false;
}

// Ordinary changes repeat one name, with equal-length a/ and b/ prefixes or none.
// Quoted names provide an explicit boundary; rename/copy names are resolved separately.
PathPair ReadRepeatedNames(str_view names, size_t offset) {
    size_t split = SIZE_MAX;
    if(names.starts_with('"')) {
        size_t position = 0;
        ReadQuotedPath(names, position, offset);
        split = position;
    } else {
        const size_t quote = names.find('"');
        if(quote != SIZE_MAX && quote != 0 && names[quote - 1] == ' ') split = quote - 1;
        else if(names.length() % 2 == 1) split = names.length() / 2;
    }
    if(split == SIZE_MAX || split >= names.length() || names[split] != ' ') {
        throw PatchError(ErrorCode::kUnsupportedFeature, "Ambiguous Git header paths or prefixes", offset);
    }
    return {ReadPath(names.substr(0, split), offset), ReadPath(names.substr(split + 1), offset)};
}

// Reads extended headers until a payload, blank line, or next Git section is reached.
// Unused extension fields are preserved rather than semantically validated.
GitHeaders ReadHeaders(const TextBuffer& text, size_t first, size_t& line) {
    if(!text.GetLine(first).starts_with(kGitSectionPrefix)) {
        Invalid(text, first, "Expected Git file section");
    }
    GitHeaders headers;
    headers.names = text.GetLine(first).substr(sizeof(kGitSectionPrefix) - 1);
    line = first + 1;
    while(line < text.GetLineCount()) {
        const str_view value = text.GetLine(line);
        const size_t offset = text.GetLineOffset(line);
        if(value.empty() || IsMailSignatureSeparator(value) || value.starts_with(kGitSectionPrefix) ||
           value == kGitBinaryMarker || IsBinaryChangeNotice(value) ||
           value.starts_with("@@ ")) break;
        CheckUnsupported(text, line, true);
        if(value.starts_with("--- ")) {
            if(line + 1 == text.GetLineCount() || !text.GetLine(line + 1).starts_with("+++ ")) {
                Invalid(text, line, "Missing Git new-file header");
            }
            headers.old_file = ReadPath(HeaderPath(value), offset);
            headers.new_file = ReadPath(HeaderPath(text.GetLine(line + 1)), text.GetLineOffset(line + 1));
            line += 2;
            break;
        }
        if(value.starts_with("rename from ") || value.starts_with("copy from ")) {
            const bool rename = value.starts_with("rename from ");
            headers.rename = headers.rename || rename;
            headers.copy = headers.copy || !rename;
            headers.from = ReadPath(value.substr(rename ? 12 : 10), offset);
        } else if(value.starts_with("rename to ") || value.starts_with("copy to ")) {
            const bool rename = value.starts_with("rename to ");
            headers.rename = headers.rename || rename;
            headers.copy = headers.copy || !rename;
            headers.to = ReadPath(value.substr(rename ? 10 : 8), offset);
        } else if(value.starts_with("new file mode ")) headers.added = true;
        else if(value.starts_with("deleted file mode ")) headers.deleted = true;
        else if(value.starts_with("old mode ")) headers.old_mode = true;
        else if(value.starts_with("new mode ")) headers.new_mode = true;
        else if(!value.starts_with("index ") && !value.starts_with("similarity index ") &&
                !value.starts_with("dissimilarity index ")) {
            if(value.front() < 'a' || value.front() > 'z' || value.find(' ') == SIZE_MAX) {
                Invalid(text, line, "Unusable Git extended header");
            }
            ++line;
            continue;
        }
        headers.metadata = true;
        ++line;
    }
    return headers;
}

// Resolves default prefixes or unambiguous unprefixed names. Explicit rename/copy
// metadata supplies repository paths; custom prefixes need these independent anchors.
std::wstring ResolvePath(const TextBuffer& text, size_t first, const GitHeaders& headers) {
    const size_t offset = text.GetLineOffset(first);
    if((headers.added && headers.deleted) || (headers.rename && headers.copy) ||
       ((headers.rename || headers.copy) && (headers.added || headers.deleted))) {
        Invalid(text, first, "Ambiguous Git change kind");
    }
    PathPair paths;
    bool deleted = headers.deleted;
    if(headers.rename || headers.copy) {
        if(!headers.from || !headers.to) Invalid(text, first, "Incomplete Git rename/copy paths");
        paths = {*headers.from, *headers.to};
        const bool prefixed = HeaderMatches(headers.names, "a/" + paths.old_path, "b/" + paths.new_path, offset);
        const bool bare = HeaderMatches(headers.names, paths.old_path, paths.new_path, offset);
        if(headers.old_file) {
            const bool custom = !prefixed && !bare &&
                str_view(*headers.old_file).ends_with(paths.old_path) &&
                str_view(*headers.new_file).ends_with(paths.new_path);
            if(!HeaderMatches(headers.names, *headers.old_file, *headers.new_file, offset) ||
               (!custom && (*headers.old_file != (prefixed ? "a/" + paths.old_path : paths.old_path) ||
                            *headers.new_file != (prefixed ? "b/" + paths.new_path : paths.new_path)))) {
                Invalid(text, first, "Git path headers disagree");
            }
        } else if(!prefixed && !bare && !HeaderSuffixesMatch(headers.names, paths.old_path, paths.new_path, offset)) {
            throw PatchError(ErrorCode::kUnsupportedFeature, "Unresolved Git prefixes", offset);
        }
    } else if(headers.old_file) {
        const str_view old_path(*headers.old_file);
        const str_view new_path(*headers.new_file);
        if(old_path == kDevNullPath && new_path == kDevNullPath) Invalid(text, first, "Two missing Git paths");
        deleted = deleted || new_path == kDevNullPath;
        if(old_path == kDevNullPath || new_path == kDevNullPath) {
            const str_view existing = old_path == kDevNullPath ? new_path : old_path;
            const str_view prefix = old_path == kDevNullPath ? "b/" : "a/";
            const std::string stripped = existing.starts_with(prefix) ? existing.substr(2).to_string() : "";
            if(!stripped.empty() && HeaderMatches(headers.names, "a/" + stripped, "b/" + stripped, offset)) {
                paths = {stripped, stripped};
            } else if(HeaderMatches(headers.names, existing, existing, offset)) {
                paths = {existing.to_string(), existing.to_string()};
            } else {
                throw PatchError(ErrorCode::kUnsupportedFeature, "Unresolved Git prefixes", offset);
            }
        } else {
            if(!HeaderMatches(headers.names, old_path, new_path, offset)) Invalid(text, first, "Git path headers disagree");
            if(old_path.starts_with("a/") && new_path.starts_with("b/")) {
                paths = {old_path.substr(2).to_string(), new_path.substr(2).to_string()};
            } else if(old_path == new_path) {
                paths = {old_path.to_string(), new_path.to_string()};
            } else {
                throw PatchError(ErrorCode::kUnsupportedFeature, "Unresolved Git prefixes", offset);
            }
        }
    } else {
        paths = ReadRepeatedNames(headers.names, offset);
        if(str_view(paths.old_path).starts_with("a/") && str_view(paths.new_path).starts_with("b/") &&
           str_view(paths.old_path).substr(2) == str_view(paths.new_path).substr(2)) {
            paths.old_path.erase(0, 2);
            paths.new_path.erase(0, 2);
        } else if(paths.old_path != paths.new_path) {
            throw PatchError(ErrorCode::kUnsupportedFeature, "Unresolved Git prefixes", offset);
        }
    }
    const str_view selected(deleted ? paths.old_path : paths.new_path);
    if(selected.find('\\') != SIZE_MAX) {
        throw PatchError(ErrorCode::kInvalidPath, "Literal backslash in Git filename", offset);
    }
    return DecodePathBytes(selected, text.HasUtf8Bom(), offset);
}

// Checks base85 line framing only; no decoding or payload validation.
size_t SkipBinaryRecord(const TextBuffer& text, size_t line) {
    ValidateGitBinaryHeader(text, line);
    const size_t first_payload = ++line;
    while(line < text.GetLineCount() && !text.GetLine(line).empty()) {
        const str_view value = text.GetLine(line);
        const char length_code = value.front();
        const size_t length = length_code >= 'A' && length_code <= 'Z' ? length_code - 'A' + 1 :
                              length_code >= 'a' && length_code <= 'z' ? length_code - 'a' + 27 : 0;
        if(length == 0 || value.length() != 1 + 5 * ((length + 3) / 4)) {
            Invalid(text, line, "Invalid Git binary line length");
        }
        ++line;
    }
    if(line == first_payload || line == text.GetLineCount()) Invalid(text, line, "Truncated Git binary record");
    return line + 1;  // Keep the required empty terminator in the extracted section.
}

}  // namespace

void ValidateGitBinaryHeader(const TextBuffer& text, size_t line) {
    if(line == text.GetLineCount()) Invalid(text, line, "Missing Git binary record");
    const str_view value = text.GetLine(line);
    size_t position = value.starts_with("literal ") ? 8 : value.starts_with("delta ") ? 6 : 0;
    if(position == 0) Invalid(text, line, "Unknown Git binary record method");
    SkipDigits(value, position, text.GetLineOffset(line));
}

size_t SkipGitBinary(const TextBuffer& text, size_t line) {
    line = SkipBinaryRecord(text, line + 1);
    if(line < text.GetLineCount() && (text.GetLine(line).starts_with("literal ") ||
                                    text.GetLine(line).starts_with("delta "))) {
        line = SkipBinaryRecord(text, line);
    }
    return line;
}

bool IsGitCommitPreamble(str_view line) {
    const size_t split = line.find(" (from ");
    if(split == SIZE_MAX) return IsObjectId(line);
    return line.length() == 2 * split + 8 && line.ends_with(")") &&
           IsObjectId(line.substr(0, split)) && IsObjectId(line.substr(split + 7, split));
}

bool IsGitCandidate(const TextBuffer& text, size_t line) {
    if(!text.GetLine(line).starts_with(kGitSectionPrefix)) return false;
    for(++line; line < text.GetLineCount(); ++line) {
        const str_view value = text.GetLine(line);
        if(value.empty() || value.starts_with(kGitSectionPrefix)) return false;
        if(value.starts_with("index ") || value.starts_with("--- ") || value == kGitBinaryMarker ||
           value.starts_with("old mode ") || value.starts_with("new mode ") ||
           value.starts_with("new file mode ") || value.starts_with("deleted file mode ") ||
           value.starts_with("rename from ") || value.starts_with("rename to ") ||
           value.starts_with("copy from ") || value.starts_with("copy to ") ||
           value.starts_with("similarity index ") || value.starts_with("dissimilarity index ") ||
           IsBinaryChangeNotice(value)) return true;
    }
    return false;
}

void ValidateGitPrefix(const TextBuffer& text) {
    const size_t first = GitPatchStart(text);
    size_t line;
    const auto headers = ReadHeaders(text, first, line);
    ResolvePath(text, first, headers);
    if(line < text.GetLineCount() && IsBinaryChangeNotice(text.GetLine(line))) {
        ValidateBinaryChangeNotice(text, line);
    } else if(line < text.GetLineCount() && text.GetLine(line) == kGitBinaryMarker) {
        if(line + 1 < text.GetLineCount()) ValidateGitBinaryHeader(text, line + 1);
    } else if(headers.old_file) {
        if(line == text.GetLineCount()) Invalid(text, line, "Missing Git hunk header in probe");
        ValidateUnifiedHunkHeader(text.GetLine(line), text.GetLineOffset(line));
    } else if(!headers.metadata) {
        Invalid(text, first, "No Git change structure in probe");
    }
}

ParsedFile ParseGitSection(const TextBuffer& text, size_t& line) {
    CheckUnsupported(text, line, true);
    const size_t first = line;
    const auto headers = ReadHeaders(text, first, line);
    std::wstring path = ResolvePath(text, first, headers);
    if(line < text.GetLineCount() && IsBinaryChangeNotice(text.GetLine(line))) {
        ValidateBinaryChangeNotice(text, line);
        ++line;
    } else if(headers.old_file) {
        if(line == text.GetLineCount() || !text.GetLine(line).starts_with("@@ ")) {
            Invalid(text, line, "Git text section has no hunks");
        }
        do {
            line = SkipUnifiedHunk(text, line);
        } while(line < text.GetLineCount() && text.GetLine(line).starts_with("@@ "));
    } else if(line < text.GetLineCount() && text.GetLine(line) == kGitBinaryMarker) {
        line = SkipGitBinary(text, line);
    } else if(!(headers.added || headers.deleted || headers.rename || headers.copy ||
                (headers.old_mode && headers.new_mode))) {
        Invalid(text, first, "Git section has no change payload or metadata operation");
    }
    return {std::move(path), first, line};
}

ParsedPatch ParseGit(const TextBuffer& text) {
    ParsedPatch result{PatchFormat::kGit, {}};
    size_t line = GitPatchStart(text);
    if(IsGitCommitPreamble(text.GetLine(SkipBlankLines(text, 0)))) {
        result.prefix_end = result.patch_start = line;
        result.suffix_begin = text.GetLineCount();
    }
    while(line < text.GetLineCount()) {
        result.files.push_back(ParseGitSection(text, line));
        line = SkipBlankLines(text, line);
    }
    return result;
}

}  // namespace patch_backend::detail
