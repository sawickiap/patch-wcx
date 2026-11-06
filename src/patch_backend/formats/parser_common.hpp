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

#pragma once

namespace patch_backend {
enum class PatchFormat;
}

namespace patch_backend::detail {

class TextBuffer;

inline constexpr char kDevNullPath[] = "/dev/null";
inline constexpr char kGitSectionPrefix[] = "diff --git ";
inline constexpr char kGitBinaryMarker[] = "GIT binary patch";
inline constexpr char kBinaryFilesPrefix[] = "Binary files ";
inline constexpr char kBinaryFilePrefix[] = "Binary file ";
inline constexpr char kHgDiffPrefix[] = "diff -r ";
inline constexpr char kHgExportMarker[] = "# HG changeset patch";
inline constexpr char kMailSignatureSeparator[] = "-- ";
inline constexpr char kSvnIndexPrefix[] = "Index: ";
inline constexpr char kSvnPropertyPrefix[] = "Property changes on: ";

struct ParsedFile {
    std::wstring path;
    size_t first_line;
    size_t end_line;
};

// Throws kInvalidPatch at the line's byte offset; the one-past-last line is allowed.
[[noreturn]] void Invalid(const TextBuffer& text, size_t line, str_view message);
// Reads decimal digits with overflow checks, advancing position; throws on missing digits.
uint64_t ReadUnsigned(str_view line, size_t& position, size_t byte_offset);
// Advances over at least one decimal digit without converting its value; throws if absent.
void SkipDigits(str_view line, size_t& position, size_t byte_offset);
// Consumes a required token at position, throwing kInvalidPatch if it does not match.
void Expect(str_view line, size_t& position, str_view token, size_t byte_offset);
// Decodes one C-quoted filename, advancing past its closing quote. Octal escapes
// contain three digits and represent bytes. Throws kInvalidPatch for malformed quoting.
std::string ReadQuotedPath(str_view value, size_t& position, size_t byte_offset);

size_t SkipBlankLines(const TextBuffer& text, size_t line);
// Recognizes the standard mail signature separator and its space-stripped variant.
// Call only at section boundaries; identical text inside a hunk is patch content.
bool IsMailSignatureSeparator(str_view line);
// Recognizes binary-change notice introducers; paths come from their enclosing sections.
bool IsBinaryChangeNotice(str_view line);
// Checks Git/Hg notice framing without interpreting display names or requiring payload bytes.
void ValidateBinaryChangeNotice(const TextBuffer& text, size_t line);
// Skips consecutive no-newline markers without interpreting their semantics.
size_t SkipNoNewlineMarkers(const TextBuffer& text, size_t line);
// Recognizes GNU diff options selecting the format, without fully parsing the command.
bool IsDiffPreamble(str_view line, PatchFormat format);
// Throws kUnsupportedFeature for known unsupported wrappers or comparison notices.
// allow_git permits a Git section boundary when called by that parser.
void CheckUnsupported(const TextBuffer& text, size_t line, bool allow_git = false);
// Skips one matching preamble and requires consecutive file headers for this format.
// Throws kUnknownFormat for an unrecognized first header, otherwise kInvalidPatch.
size_t FindFileHeader(const TextBuffer& text, size_t line, PatchFormat format, bool first_file);
// Strips a four-byte file-header prefix and an optional tab-separated timestamp.
str_view HeaderPath(str_view line);
// Decodes quoted/unquoted unified or context header names before /dev/null selection
// and existing path checks. Optional timestamps follow a tab after the filename.
// Requires both file headers; empty paths or two missing sides throw kInvalidPatch.
std::wstring ReadFilePath(const TextBuffer& text, size_t old_header, size_t new_header);

}  // namespace patch_backend::detail
