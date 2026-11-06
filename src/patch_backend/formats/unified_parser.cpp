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

#include "unified_parser.hpp"
#include "../patch_error.hpp"
#include "../patch_archive.hpp"
#include "../text_buffer.hpp"

namespace patch_backend::detail {
namespace {

struct HunkCounts {
    uint64_t old_count;
    uint64_t new_count;
};

// Skips the unused starting line number and reads the count, defaulting to one.
uint64_t ReadRangeCount(str_view line, size_t& position, size_t byte_offset) {
    SkipDigits(line, position, byte_offset);
    if(position < line.length() && line[position] == ',') {
        ++position;
        return ReadUnsigned(line, position, byte_offset);
    }
    return 1;
}

// Reads only the counts needed for traversal; ignores text after the closing @@.
HunkCounts ReadHunkCounts(str_view line, size_t byte_offset) {
    size_t position = 0;
    Expect(line, position, "@@ -", byte_offset);
    const uint64_t old_count = ReadRangeCount(line, position, byte_offset);
    Expect(line, position, " +", byte_offset);
    const uint64_t new_count = ReadRangeCount(line, position, byte_offset);
    Expect(line, position, " @@", byte_offset);
    return {old_count, new_count};
}

}  // namespace

void ValidateUnifiedHunkHeader(str_view line, size_t byte_offset) {
    ReadHunkCounts(line, byte_offset);
}

size_t SkipUnifiedHunk(const TextBuffer& text, size_t line) {
    auto remaining = ReadHunkCounts(text.GetLine(line), text.GetLineOffset(line));
    ++line;
    for(;;) {
        line = SkipNoNewlineMarkers(text, line);
        if(remaining.old_count == 0 && remaining.new_count == 0) {
            return line;
        }
        if(line == text.GetLineCount() || text.GetLine(line).empty()) {
            Invalid(text, line, "Truncated hunk body");
        }
        const char prefix = text.GetLine(line).front();
        if(prefix != ' ' && prefix != '-' && prefix != '+') {
            Invalid(text, line, "Invalid hunk body line");
        }
        const bool uses_old = prefix != '+';
        const bool uses_new = prefix != '-';
        if((uses_old && remaining.old_count == 0) || (uses_new && remaining.new_count == 0)) {
            Invalid(text, line, "Hunk body exceeds its count");
        }
        remaining.old_count -= uses_old ? 1 : 0;
        remaining.new_count -= uses_new ? 1 : 0;
        ++line;
    }
}

void ValidateUnifiedPrefix(const TextBuffer& text) {
    if(text.GetLineCount() == 0) {
        return;
    }
    const size_t first = SkipBlankLines(text, 0);
    if(first == text.GetLineCount()) {
        throw PatchError(ErrorCode::kUnknownFormat, "No patch structure in probe");
    }
    const size_t header = FindFileHeader(text, first, PatchFormat::kUnified, true);
    if(header + 2 >= text.GetLineCount()) {
        Invalid(text, header, "No hunk header in probe");
    }
    ValidateUnifiedHunkHeader(text.GetLine(header + 2), text.GetLineOffset(header + 2));
}

std::vector<ParsedFile> ParseUnified(const TextBuffer& text) {
    std::vector<ParsedFile> files;
    size_t line = SkipBlankLines(text, 0);
    if(line == text.GetLineCount() && text.GetLineCount() != 0) {
        throw PatchError(ErrorCode::kUnknownFormat, "Input contains no patch");
    }
    while(line < text.GetLineCount()) {
        const size_t first_line = line;
        const size_t header = FindFileHeader(text, line, PatchFormat::kUnified, files.empty());
        std::wstring path = ReadFilePath(text, header, header + 1);
        line = header + 2;
        if(line == text.GetLineCount() || !text.GetLine(line).starts_with("@@ ")) {
            Invalid(text, line, "File section has no unified hunks");
        }
        do {
            line = SkipUnifiedHunk(text, line);
        } while(line < text.GetLineCount() && text.GetLine(line).starts_with("@@ "));
        files.push_back({std::move(path), first_line, line});
        line = SkipBlankLines(text, line);
    }
    return files;
}

}  // namespace patch_backend::detail
