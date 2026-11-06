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

std::wstring DecodePath(str_view path, bool has_utf8_bom, size_t byte_offset);
// Converts already-unquoted filename bytes and validates the resulting Windows path.
std::wstring DecodePathBytes(str_view path, bool has_utf8_bom, size_t byte_offset);

struct PathLess {
    bool operator()(wstr_view lhs, wstr_view rhs) const;
};

}  // namespace patch_backend::detail
