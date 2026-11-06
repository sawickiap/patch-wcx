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

namespace patch_backend::detail {

inline constexpr char kUtf8Bom[] = "\xEF\xBB\xBF";

class TextBuffer {
public:
    explicit TextBuffer(std::string characters);

    bool HasUtf8Bom() const noexcept { return has_utf8_bom_; }
    size_t GetLineCount() const noexcept { return lines_.size(); }
    // The one-past-last line maps to the end of the buffer.
    size_t GetLineOffset(size_t line) const;

    // Returns a line without its trailing CR, LF, or CRLF; excludes the UTF-8 BOM.
    // Requires line < GetLineCount().
    str_view GetLine(size_t line) const;

    // Returns original bytes for lines [first_line, end_line), excluding the UTF-8 BOM.
    // Preserves all line endings, including the last selected line's ending if present.
    // Requires first_line <= end_line <= GetLineCount().
    str_view GetBytes(size_t first_line, size_t end_line) const;

    // Transfers the original buffer, including BOM, for rewriting. Discard this object afterward.
    std::string TakeCharacters() && { return std::move(characters_); }

private:
    std::string characters_;
    std::vector<size_t> lines_;
    bool has_utf8_bom_ = false;
};

}  // namespace patch_backend::detail
