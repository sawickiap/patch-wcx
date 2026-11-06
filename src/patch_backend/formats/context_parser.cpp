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

#include "context_parser.hpp"
#include "../patch_archive.hpp"
#include "../text_buffer.hpp"

namespace patch_backend::detail {
namespace {

bool IsHunkSeparator(str_view line) {
    return line.starts_with("***************");
}

// Reads an inclusive range's body count; positions are used only for this subtraction.
// A single zero is empty. Description text after the required suffix is ignored.
uint64_t ReadRangeCount(const TextBuffer& text, size_t line, bool old_side) {
    if(line == text.GetLineCount()) Invalid(text, line, "Missing context range header");
    const str_view value = text.GetLine(line);
    const size_t offset = text.GetLineOffset(line);
    size_t position = 0;
    Expect(value, position, old_side ? "*** " : "--- ", offset);
    const uint64_t first = ReadUnsigned(value, position, offset);
    uint64_t last = first;
    if(position < value.length() && value[position] == ',') {
        ++position;
        last = ReadUnsigned(value, position, offset);
    }
    Expect(value, position, old_side ? " ****" : " ----", offset);
    if(first == 0 && last == 0) return 0;
    if(last < first || last - first == std::numeric_limits<uint64_t>::max()) {
        Invalid(text, line, "Unusable context range count");
    }
    return last - first + 1;
}

bool IsBodyLine(str_view line, bool old_side) {
    return line.starts_with("  ") || line.starts_with("! ") ||
           line.starts_with(old_side ? "- " : "+ ");
}

// A whole half can be omitted even with a nonzero declared range. Once a body starts,
// consume its complete count so truncated bodies cannot be mistaken for new sections.
size_t SkipBody(const TextBuffer& text, size_t line, uint64_t count, bool old_side) {
    line = SkipNoNewlineMarkers(text, line);
    if(line == text.GetLineCount() || !IsBodyLine(text.GetLine(line), old_side)) return line;
    while(count != 0) {
        line = SkipNoNewlineMarkers(text, line);
        if(line == text.GetLineCount() || !IsBodyLine(text.GetLine(line), old_side)) {
            Invalid(text, line, "Truncated or unusable context body");
        }
        --count;
        ++line;
    }
    return SkipNoNewlineMarkers(text, line);
}

// Requires a separator and both range headers, returning the first line after the hunk.
size_t SkipHunk(const TextBuffer& text, size_t line) {
    ++line;  // The caller identified the separator.
    const uint64_t old_count = ReadRangeCount(text, line, true);
    line = SkipBody(text, line + 1, old_count, true);
    const uint64_t new_count = ReadRangeCount(text, line, false);
    return SkipBody(text, line + 1, new_count, false);
}

}  // namespace

void ValidateContextPrefix(const TextBuffer& text) {
    const size_t first = SkipBlankLines(text, 0);
    const size_t header = FindFileHeader(text, first, PatchFormat::kContext, true);
    if(header + 3 >= text.GetLineCount() || !IsHunkSeparator(text.GetLine(header + 2))) {
        Invalid(text, header, "No context hunk header in probe");
    }
    ReadRangeCount(text, header + 3, true);
}

std::vector<ParsedFile> ParseContext(const TextBuffer& text) {
    std::vector<ParsedFile> files;
    size_t line = SkipBlankLines(text, 0);
    while(line < text.GetLineCount()) {
        const size_t first_line = line;
        const size_t header = FindFileHeader(text, line, PatchFormat::kContext, files.empty());
        std::wstring path = ReadFilePath(text, header, header + 1);
        line = header + 2;
        if(line == text.GetLineCount() || !IsHunkSeparator(text.GetLine(line))) {
            Invalid(text, line, "File section has no context hunks");
        }
        do {
            line = SkipHunk(text, line);
        } while(line < text.GetLineCount() && IsHunkSeparator(text.GetLine(line)));
        files.push_back({std::move(path), first_line, line});
        line = SkipBlankLines(text, line);
    }
    return files;
}

}  // namespace patch_backend::detail
