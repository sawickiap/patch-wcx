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

// Recognizes a full 40/64-digit hexadecimal commit ID, optionally followed by
// " (from <parent-ID>)" with the same ID width. IDs remain opaque metadata.
bool IsGitCommitPreamble(str_view line);
// Finds a Git preamble followed by recognizable fields or payload, skipping unused
// extension fields; a quoted command alone does not establish a section.
bool IsGitCandidate(const TextBuffer& text, size_t line);
// Checks a literal/delta record header without interpreting the declared size.
void ValidateGitBinaryHeader(const TextBuffer& text, size_t line);
// Traverses a GIT binary patch marker and one or two records, including terminators.
// Shared with SVN Git-style sections; payloads are preserved without decoding.
size_t SkipGitBinary(const TextBuffer& text, size_t line);
// Requires initial Git path information and convincing metadata or payload structure.
// The bounded probe need not contain complete text hunks or binary records.
void ValidateGitPrefix(const TextBuffer& text);
// Reads one Git file section and advances line to its end, leaving wrappers to the caller.
ParsedFile ParseGitSection(const TextBuffer& text, size_t& line);
// Preserves complete Git text, binary payload/notice, and metadata-only sections as per-file documents.
// Rejects ambiguous names, unsupported wrappers, and unusable section/payload framing.
ParsedPatch ParseGit(const TextBuffer& text);

}  // namespace patch_backend::detail
