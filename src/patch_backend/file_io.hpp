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

#include "write_progress.hpp"

namespace patch_backend::detail {

inline constexpr size_t kProbeSize = 32 * 1024;

struct FileContents {
    std::string bytes;
    uint64_t last_write_time;
};

struct FilePrefix {
    std::string bytes;
    bool truncated;
};

// Reads at most kProbeSize bytes; the file size determines whether the prefix is truncated.
FilePrefix ReadPrefix(wstr_view path);
FileContents ReadPatchFile(wstr_view path);

// Opens an existing patch exclusively for reading/writing without truncation. Throws PatchError.
UniqueHandle OpenPatchForUpdate(wstr_view path);
// Reads from a borrowed handle positioned at the beginning; same checks as the path overload.
FileContents ReadPatchFile(HANDLE file, wstr_view path);
// Rewrites a borrowed handle from offset zero and truncates its old tail. Progress cannot cancel.
// Throws PatchError on seek/write/truncate/flush failure; partial output may remain.
void RewritePatchFile(HANDLE file, wstr_view path, str_view bytes, WriteProgress progress);
// Closes and releases the owned handle; reports close failures through PatchError.
void ClosePatchFile(UniqueHandle& file, wstr_view path);

}  // namespace patch_backend::detail
