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

#include "parser_common.hpp"
#include "../patch_archive.hpp"
#include "../patch_error.hpp"
#include "../patch_paths.hpp"
#include "../text_buffer.hpp"

namespace patch_backend::detail {
namespace {

constexpr char kNoNewlineMarker[] = "\\ No newline at end of file";

// Decodes header filename syntax only; Unicode conversion and path checks follow selection.
std::string ReadHeaderPath(str_view line, size_t byte_offset) {
    const str_view field = line.substr(4);
    if(!field.starts_with('"')) return HeaderPath(line).to_string();
    size_t position = 0;
    std::string path = ReadQuotedPath(field, position, byte_offset + 4);
    if(position != field.length() && field[position] != '\t') {
        throw PatchError(ErrorCode::kInvalidPatch, "Unexpected text after quoted header path",
                         byte_offset + 4 + position);
    }
    return path;
}

}  // namespace

[[noreturn]] void Invalid(const TextBuffer& text, size_t line, str_view message) {
    throw PatchError(ErrorCode::kInvalidPatch, message.to_string(), text.GetLineOffset(line));
}

uint64_t ReadUnsigned(str_view line, size_t& position, size_t byte_offset) {
    const size_t first = position;
    uint64_t value = 0;
    while(position < line.length() && line[position] >= '0' && line[position] <= '9') {
        const uint64_t digit = static_cast<uint64_t>(line[position++] - '0');
        if(value > (std::numeric_limits<uint64_t>::max() - digit) / 10) {
            throw PatchError(ErrorCode::kInvalidPatch, "Patch number overflow", byte_offset);
        }
        value = value * 10 + digit;
    }
    if(position == first) {
        throw PatchError(ErrorCode::kInvalidPatch, "Missing patch number", byte_offset);
    }
    return value;
}

void SkipDigits(str_view line, size_t& position, size_t byte_offset) {
    const size_t first = position;
    while(position < line.length() && line[position] >= '0' && line[position] <= '9') ++position;
    if(position == first) {
        throw PatchError(ErrorCode::kInvalidPatch, "Missing patch number", byte_offset);
    }
}

void Expect(str_view line, size_t& position, str_view token, size_t byte_offset) {
    if(!line.substr(position).starts_with(token)) {
        throw PatchError(ErrorCode::kInvalidPatch, "Malformed patch syntax", byte_offset);
    }
    position += token.length();
}

std::string ReadQuotedPath(str_view value, size_t& position, size_t offset) {
    Expect(value, position, "\"", offset);
    std::string result;
    while(position < value.length()) {
        char ch = value[position++];
        if(ch == '"') return result;
        if(ch == '\\') {
            if(position == value.length()) break;
            ch = value[position++];
            switch(ch) {
            case 'a': ch = '\a'; break;
            case 'b': ch = '\b'; break;
            case 'f': ch = '\f'; break;
            case 'n': ch = '\n'; break;
            case 'r': ch = '\r'; break;
            case 't': ch = '\t'; break;
            case 'v': ch = '\v'; break;
            case '\\': case '"': break;
            default:
                if(ch < '0' || ch > '3' || value.length() - position < 2 ||
                   value[position] < '0' || value[position] > '7' ||
                   value[position + 1] < '0' || value[position + 1] > '7') {
                    throw PatchError(ErrorCode::kInvalidPatch, "Invalid quoted path escape", offset);
                }
                ch = static_cast<char>((ch - '0') * 64 + (value[position] - '0') * 8 +
                                       value[position + 1] - '0');
                position += 2;
                break;
            }
        }
        result += ch;
    }
    throw PatchError(ErrorCode::kInvalidPatch, "Unterminated path quote", offset);
}

size_t SkipBlankLines(const TextBuffer& text, size_t line) {
    while(line < text.GetLineCount() && text.GetLine(line).empty()) ++line;
    return line;
}

bool IsMailSignatureSeparator(str_view line) {
    return line == kMailSignatureSeparator || line == "--";
}

bool IsBinaryChangeNotice(str_view line) {
    return line.starts_with(kBinaryFilesPrefix) || line.starts_with(kBinaryFilePrefix);
}

void ValidateBinaryChangeNotice(const TextBuffer& text, size_t line) {
    const str_view value = text.GetLine(line);
    const bool plural = value.starts_with(kBinaryFilesPrefix);
    const str_view names_and_suffix = value.substr(plural ? sizeof(kBinaryFilesPrefix) - 1 :
                                                           sizeof(kBinaryFilePrefix) - 1);
    const str_view suffix = plural ? " differ" : " has changed";
    if(!names_and_suffix.ends_with(suffix)) Invalid(text, line, "Malformed binary notice");
    const str_view names = names_and_suffix.substr(0, names_and_suffix.length() - suffix.length());
    if(names.empty()) Invalid(text, line, "Missing binary notice name");
    if(plural) {
        const size_t separator = names.find(" and ");
        if(separator == SIZE_MAX || separator == 0 || separator + 5 >= names.length()) {
            Invalid(text, line, "Missing binary notice names");
        }
    }
}

size_t SkipNoNewlineMarkers(const TextBuffer& text, size_t line) {
    while(line < text.GetLineCount() && text.GetLine(line) == kNoNewlineMarker) ++line;
    return line;
}

bool IsDiffPreamble(str_view line, PatchFormat format) {
    if(!line.starts_with("diff ")) return false;
    const bool unified = format == PatchFormat::kUnified;
    size_t begin = 5;
    while(begin < line.length()) {
        while(begin < line.length() && line[begin] == ' ') ++begin;
        size_t end = line.find(' ', begin);
        if(end == SIZE_MAX) end = line.length();
        const str_view option = line.substr(begin, end - begin);
        if(!option.starts_with('-')) break;
        if(option == (unified ? "--unified" : "--context") ||
           option.starts_with(unified ? "--unified=" : "--context=") ||
           (!option.starts_with("--") &&
            (option.find(unified ? 'u' : 'c') != SIZE_MAX ||
             option.find(unified ? 'U' : 'C') != SIZE_MAX))) {
            return true;
        }
        begin = end + 1;
    }
    return false;
}

void CheckUnsupported(const TextBuffer& text, size_t line, bool allow_git) {
    const str_view value = text.GetLine(line);
    if(allow_git && value.starts_with(kGitSectionPrefix)) return;
    if(value.starts_with(kGitSectionPrefix) || value.starts_with("diff --cc ") ||
       value.starts_with("diff --combined ") || value.starts_with(kSvnIndexPrefix) ||
       value.starts_with(kHgExportMarker) || value.starts_with("From ") ||
       IsBinaryChangeNotice(value) || value.starts_with("Only in ") ||
       value.starts_with(kSvnPropertyPrefix) || value.starts_with("Submodule ") ||
       (value.starts_with("diff ") && !IsDiffPreamble(value, PatchFormat::kUnified) &&
        !IsDiffPreamble(value, PatchFormat::kContext))) {
        throw PatchError(ErrorCode::kUnsupportedFeature, "Unsupported patch format or section",
                         text.GetLineOffset(line));
    }
}

size_t FindFileHeader(const TextBuffer& text, size_t line, PatchFormat format, bool first_file) {
    if(line < text.GetLineCount()) {
        CheckUnsupported(text, line);
        if(text.GetLine(line).starts_with("diff ")) {
            if(!IsDiffPreamble(text.GetLine(line), format)) {
                Invalid(text, line, "Diff preamble selects a different format");
            }
            ++line;
        }
    }
    const bool unified = format == PatchFormat::kUnified;
    if(line == text.GetLineCount() || !text.GetLine(line).starts_with(unified ? "--- " : "*** ")) {
        throw PatchError(first_file ? ErrorCode::kUnknownFormat : ErrorCode::kInvalidPatch,
                         "Expected patch file headers", text.GetLineOffset(line));
    }
    if(line + 1 == text.GetLineCount() || !text.GetLine(line + 1).starts_with(unified ? "+++ " : "--- ")) {
        Invalid(text, line, "Missing new-file header");
    }
    return line;
}

str_view HeaderPath(str_view line) {
    const str_view path = line.substr(4);
    const size_t timestamp = path.find('\t');
    return timestamp == SIZE_MAX ? path : path.substr(0, timestamp);
}

std::wstring ReadFilePath(const TextBuffer& text, size_t old_header, size_t new_header) {
    const std::string old_path = ReadHeaderPath(text.GetLine(old_header), text.GetLineOffset(old_header));
    const std::string new_path = ReadHeaderPath(text.GetLine(new_header), text.GetLineOffset(new_header));
    if(old_path.empty() || new_path.empty() || (old_path == kDevNullPath && new_path == kDevNullPath)) {
        Invalid(text, old_header, "Invalid file headers");
    }
    const bool deleted = new_path == kDevNullPath;
    return DecodePathBytes(deleted ? old_path : new_path, text.HasUtf8Bom(),
                           text.GetLineOffset(deleted ? old_header : new_header) + 4);
}

}  // namespace patch_backend::detail
