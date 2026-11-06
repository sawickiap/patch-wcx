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

#include "svn_parser.hpp"
#include "unified_parser.hpp"
#include "git_parser.hpp"
#include "../patch_archive.hpp"
#include "../patch_paths.hpp"
#include "../patch_error.hpp"
#include "../text_buffer.hpp"

namespace patch_backend::detail {
namespace {

struct SvnSection {
    ParsedFile file;
    bool properties_only;
};

struct SvnGitHeaders {
    bool present = false;
    bool operation = false;
};

// Separator width is presentation detail; require a nonempty run of the expected byte.
bool IsSeparator(str_view value, char ch) {
    if(value.empty()) return false;
    for(const char byte : value) {
        if(byte != ch) return false;
    }
    return true;
}

// Reads the authoritative Index path and separator. Root is allowed for properties;
// ignored file/property header paths are never used for entry identity or filesystem I/O.
std::wstring ReadSectionHeader(const TextBuffer& text, size_t& line) {
    if(line == text.GetLineCount() || !text.GetLine(line).starts_with(kSvnIndexPrefix)) {
        if(line < text.GetLineCount()) CheckUnsupported(text, line);
        Invalid(text, line, "Expected SVN Index header");
    }
    const str_view path = text.GetLine(line).substr(sizeof(kSvnIndexPrefix) - 1);
    std::wstring result = path == "." ? L"." : DecodePath(path, text.HasUtf8Bom(), text.GetLineOffset(line));
    ++line;
    if(line == text.GetLineCount() || !IsSeparator(text.GetLine(line), '=')) {
        Invalid(text, line, "Missing SVN section separator");
    }
    line = SkipBlankLines(text, line + 1);
    return result;
}

// Git-style annotations are opaque: SVN may use repository-relative paths and
// copy sources with @REV suffixes. Only operation presence matters for empty files.
SvnGitHeaders ReadGitHeaders(const TextBuffer& text, size_t& line) {
    if(line == text.GetLineCount() || !text.GetLine(line).starts_with(kGitSectionPrefix)) return {};
    if(text.GetLine(line).length() == sizeof(kGitSectionPrefix) - 1) {
        Invalid(text, line, "Missing SVN Git header paths");
    }
    SvnGitHeaders headers{true, false};
    bool old_mode = false;
    bool new_mode = false;
    bool rename_from = false;
    bool rename_to = false;
    bool copy_from = false;
    bool copy_to = false;
    for(++line; line < text.GetLineCount(); ++line) {
        const str_view value = text.GetLine(line);
        if(value.empty() || value.starts_with("--- ") || value.starts_with("@@ ") ||
           value.starts_with(kSvnIndexPrefix) || value.starts_with(kSvnPropertyPrefix) ||
           value == kGitBinaryMarker || IsBinaryChangeNotice(value) || value.starts_with("Cannot display:")) break;
        CheckUnsupported(text, line);
        if(value.starts_with("new file mode ") || value.starts_with("deleted file mode ")) headers.operation = true;
        else if(value.starts_with("old mode ")) old_mode = true;
        else if(value.starts_with("new mode ")) new_mode = true;
        else if(value.starts_with("rename from ")) rename_from = true;
        else if(value.starts_with("rename to ")) rename_to = true;
        else if(value.starts_with("copy from ")) copy_from = true;
        else if(value.starts_with("copy to ")) copy_to = true;
        else if(value.front() < 'a' || value.front() > 'z' || value.find(' ') == SIZE_MAX) {
            Invalid(text, line, "Unusable SVN Git extended header");
        }
    }
    headers.operation = headers.operation || (old_mode && new_mode) ||
                        (rename_from && rename_to) || (copy_from && copy_to);
    return headers;
}

// Only the consecutive ---/+++ structure matters; paths and annotations are opaque.
bool ReadFileHeaders(const TextBuffer& text, size_t& line) {
    if(line == text.GetLineCount() || !text.GetLine(line).starts_with("--- ")) return false;
    if(line + 1 == text.GetLineCount() || !text.GetLine(line + 1).starts_with("+++ ")) {
        Invalid(text, line, "Missing SVN new-file header");
    }
    line += 2;
    return true;
}

bool IsBinaryNotice(str_view value) {
    return value.starts_with("Cannot display:") || value.starts_with(kBinaryFilesPrefix);
}

// Requires the property block's underline. Names/values and ## counts are not needed
// for Index boundaries, so preserve them without semantic or numeric validation.
size_t PropertyBody(const TextBuffer& text, size_t line) {
    ++line;
    if(line == text.GetLineCount() || !IsSeparator(text.GetLine(line), '_')) {
        Invalid(text, line, "Missing SVN property separator");
    }
    return line + 1;
}

// Property value lines have diff prefixes, so an unprefixed Index starts a new section.
// Legacy Name: fields and human-readable mergeinfo lines are also retained.
size_t SkipProperties(const TextBuffer& text, size_t line) {
    line = PropertyBody(text, line);
    bool has_change = false;
    while(line < text.GetLineCount()) {
        const str_view value = text.GetLine(line);
        if(value.starts_with(kSvnIndexPrefix) || value.starts_with(kSvnPropertyPrefix)) break;
        if(value.starts_with("Added: ") || value.starts_with("Modified: ") ||
           value.starts_with("Deleted: ") || value.starts_with("Name: ")) {
            has_change = true;
        } else if(!value.empty() && value.front() != '+' && value.front() != '-' &&
                  value.front() != ' ' && value.front() != '\t' && !value.starts_with("##") &&
                  !value.starts_with("\\ No newline at end of ")) {
            CheckUnsupported(text, line);
            Invalid(text, line, "Unusable SVN property block");
        }
        ++line;
    }
    if(!has_change) Invalid(text, line, "Missing SVN property change");
    return line;
}

// Traverses content with shared unified counts; notices and properties remain opaque.
SvnSection ReadSection(const TextBuffer& text, size_t& line) {
    const size_t first = line;
    std::wstring path = ReadSectionHeader(text, line);
    const auto git = ReadGitHeaders(text, line);
    const bool headers = ReadFileHeaders(text, line);
    size_t end = line;
    bool content = git.operation;
    bool properties = false;
    bool binary = false;
    line = SkipBlankLines(text, line);
    while(line < text.GetLineCount() && !text.GetLine(line).starts_with(kSvnIndexPrefix)) {
        const str_view value = text.GetLine(line);
        if(value.starts_with("@@ ")) {
            if(!headers) Invalid(text, line, "SVN hunk has no file headers");
            line = SkipUnifiedHunk(text, line);
            content = true;
        } else if(value.starts_with(kSvnPropertyPrefix)) {
            line = SkipProperties(text, line);
            properties = true;
        } else if(IsBinaryNotice(value)) {
            binary = true;
            ++line;
            while(line < text.GetLineCount() && text.GetLine(line).starts_with("svn:mime-type")) ++line;
        } else if(git.present && value == kGitBinaryMarker) {
            line = SkipGitBinary(text, line);
            binary = true;
        } else {
            CheckUnsupported(text, line);
            Invalid(text, line, "Unusable SVN section");
        }
        end = line;
        line = SkipBlankLines(text, line);
    }
    if(git.present && !headers && !properties && !binary && !git.operation) Invalid(text, first, "Empty SVN section");
    const bool properties_only = properties && !content && !binary;
    if(path == L"." && !properties_only) {
        throw PatchError(ErrorCode::kInvalidPath, "SVN root must be a directory-property section",
                         text.GetLineOffset(first));
    }
    return {{std::move(path), first, end}, properties_only};
}

}  // namespace

void ValidateSvnPrefix(const TextBuffer& text) {
    size_t line = SkipBlankLines(text, 0);
    ReadSectionHeader(text, line);
    // SVN emits just Index/separator for empty files; the operation is ambiguous.
    if(line == text.GetLineCount() || text.GetLine(line).starts_with(kSvnIndexPrefix)) return;
    const auto git = ReadGitHeaders(text, line);
    const bool headers = ReadFileHeaders(text, line);
    line = SkipBlankLines(text, line);
    if(line < text.GetLineCount()) {
        const str_view value = text.GetLine(line);
        if(value.starts_with("@@ ") && headers) {
            ValidateUnifiedHunkHeader(value, text.GetLineOffset(line));
            return;
        }
        if(value.starts_with(kSvnPropertyPrefix)) {
            PropertyBody(text, line);
            return;
        }
        if(IsBinaryNotice(value)) return;
        if(git.present && value == kGitBinaryMarker) {
            if(line + 1 < text.GetLineCount()) ValidateGitBinaryHeader(text, line + 1);
            return;
        }
    }
    // Consecutive file headers can represent empty-file additions/deletions.
    if(!headers && !git.operation) Invalid(text, line, "No SVN change structure in probe");
}

ParsedPatch ParseSvn(const TextBuffer& text) {
    std::vector<SvnSection> sections;
    std::set<std::wstring, PathLess> directories = {L"."};
    size_t line = SkipBlankLines(text, 0);
    while(line < text.GetLineCount()) {
        auto section = ReadSection(text, line);
        for(size_t separator = section.file.path.find(L'/'); separator != std::wstring::npos;
            separator = section.file.path.find(L'/', separator + 1)) {
            directories.emplace(section.file.path.substr(0, separator));
        }
        sections.push_back(std::move(section));
    }
    ParsedPatch result{PatchFormat::kSvn, {}};
    for(auto& section : sections) {
        if(section.properties_only && directories.contains(section.file.path)) {
            result.has_hidden_sections = true;
        } else {
            result.files.push_back(std::move(section.file));
        }
    }
    return result;
}

}  // namespace patch_backend::detail
