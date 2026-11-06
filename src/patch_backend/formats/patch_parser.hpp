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

struct ParsedPatch {
    PatchFormat format;
    std::vector<ParsedFile> files;
    // Document envelope ranges: retain [0, prefix_end) and [suffix_begin, EOF),
    // omit [prefix_end, patch_start). Zero prefix_end means no envelope.
    size_t prefix_end = 0;
    size_t patch_start = 0;
    size_t suffix_begin = 0;
    // SVN directory-property sections survive deletion of every visible file.
    bool has_hidden_sections = false;
};

// Checks a bounded prefix and returns its format; empty/BOM-only input returns kEmpty.
// If truncated, discards bytes after the last CR or LF before parsing.
// Throws PatchError for unrecognized, unsupported, or unusable structure.
PatchFormat ValidatePatchPrefix(str_view prefix, bool truncated);
// Selects one format and identifies all file sections, rejecting mixed-format documents.
ParsedPatch ParsePatch(const TextBuffer& text);

}  // namespace patch_backend::detail
