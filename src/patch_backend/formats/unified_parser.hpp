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

#include "parser_common.hpp"

namespace patch_backend::detail {

// Checks initial file headers and hunk counts without requiring the complete body.
void ValidateUnifiedPrefix(const TextBuffer& text);
// Checks a hunk header without reading its body, ignoring unused positions and suffix text.
void ValidateUnifiedHunkHeader(str_view line, size_t byte_offset);
// Consumes one hunk and its no-newline markers using body counts. Throws on unusable structure.
size_t SkipUnifiedHunk(const TextBuffer& text, size_t line);
// Identifies file paths and line ranges using hunk counts, without validating unused details.
std::vector<ParsedFile> ParseUnified(const TextBuffer& text);

}  // namespace patch_backend::detail
