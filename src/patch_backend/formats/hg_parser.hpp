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

#include "patch_parser.hpp"

namespace patch_backend::detail {

// Recognizes a native Hg preamble plus initial unified structure or a binary notice.
void ValidateHgPrefix(const TextBuffer& text);
// Reads one native comparison; preserves preambles and uses their repository paths.
std::vector<ParsedFile> ParseHg(const TextBuffer& text);
// Requires an export marker and User/Date/Node ID/Parent fields, without interpreting
// their values or requiring patch data within the bounded probe.
void ValidateHgExportPrefix(const TextBuffer& text);
// Preserves the complete header/message and selected native or Git file sections.
// Rejects multiple export envelopes, comparisons, and mixed body formats.
ParsedPatch ParseHgExport(const TextBuffer& text);

}  // namespace patch_backend::detail
