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

// Recognizes a Git mailbox envelope without interpreting its timestamp or commit identity.
bool IsMailboxEnvelope(str_view line);
// Requires an envelope and From/Date/Subject headers; no patch need appear in the probe.
// Rejects MIME packaging and encoded bodies. Full parsing also requires a header terminator.
void ValidateMailboxPrefix(const TextBuffer& text);
// Parses exactly one plain-text message, reusing Git section parsing and recording the
// retained prefix/footer and omitted commentary block for extraction and deletion.
ParsedPatch ParseMailbox(const TextBuffer& text);

}  // namespace patch_backend::detail
