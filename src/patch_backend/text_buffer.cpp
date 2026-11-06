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

#include "text_buffer.hpp"
#include "patch_error.hpp"

namespace patch_backend::detail {
namespace {

// Checks a prefix or full file without validating UTF-8. Throws kUnsupportedFeature
// for UTF-16/32 BOMs, or kUnknownFormat for disallowed controls, with a byte offset.
// Form feed is a text page separator and may appear in source lines.
void ValidateTextBytes(str_view bytes) {
    if(bytes.starts_with("\xFF\xFE") || bytes.starts_with("\xFE\xFF") ||
       bytes.starts_with(str_view("\0\0\xFE\xFF", 4))) {
        throw PatchError(ErrorCode::kUnsupportedFeature, "UTF-16 and UTF-32 are unsupported", 0);
    }
    for(size_t i = 0; i < bytes.length(); ++i) {
        const auto ch = static_cast<unsigned char>(bytes[i]);
        if(ch < 0x20 && ch != '\t' && ch != '\r' && ch != '\n' && ch != '\f') {
            throw PatchError(ErrorCode::kUnknownFormat, "Input is not a supported text file", i);
        }
    }
}

}  // namespace

TextBuffer::TextBuffer(std::string characters) : characters_(std::move(characters)) {
    const str_view bytes(characters_);
    ValidateTextBytes(bytes);
    has_utf8_bom_ = bytes.starts_with(kUtf8Bom);
    size_t position = has_utf8_bom_ ? sizeof(kUtf8Bom) - 1 : 0;
    while(position < characters_.size()) {
        lines_.push_back(position);
        while(position < characters_.size() && characters_[position] != '\r' &&
              characters_[position] != '\n') {
            ++position;
        }
        if(position < characters_.size()) {
            const char ending = characters_[position++];
            if(ending == '\r' && position < characters_.size() && characters_[position] == '\n') {
                ++position;
            }
        }
    }
}

size_t TextBuffer::GetLineOffset(size_t line) const {
    assert(line <= lines_.size());
    return line == lines_.size() ? characters_.size() : lines_[line];
}

str_view TextBuffer::GetLine(size_t line) const {
    assert(line < lines_.size());
    const size_t begin = lines_[line];
    size_t end = GetLineOffset(line + 1);
    if(end > begin && characters_[end - 1] == '\n') {
        --end;
    }
    if(end > begin && characters_[end - 1] == '\r') {
        --end;
    }
    return str_view(characters_.data() + begin, end - begin);
}

str_view TextBuffer::GetBytes(size_t first_line, size_t end_line) const {
    assert(first_line <= end_line);
    const size_t begin = GetLineOffset(first_line);
    return str_view(characters_.data() + begin, GetLineOffset(end_line) - begin);
}

}  // namespace patch_backend::detail
