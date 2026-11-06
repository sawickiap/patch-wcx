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

#include <patch_backend/patch_archive.hpp>

namespace patch_wcx {

// Owns the parsed snapshot, WCX iteration state, and callbacks for one open archive.
class ArchiveHandle {
public:
    // Opens and identifies file sections and paths. Throws PatchError or std::bad_alloc on failure.
    explicit ArchiveHandle(wstr_view path);

    // Reports the current entry without advancing; ProcessFile must follow a successful read.
    // Returns E_END_ARCHIVE at EOF, or E_SMALL_BUF if a name cannot fit the WCX header.
    int ReadHeader(tHeaderDataExW& header);

    // Skips/tests the current entry or extracts to dest_path + dest_name. A null/empty
    // dest_path means dest_name is the full destination. Advances only after success.
    // Extraction throws PatchError on I/O errors or callback cancellation.
    int ProcessFile(int operation, wstr_view dest_path, wstr_view dest_name);

    void SetProcessDataProc(tProcessDataProcW callback) noexcept { callback_ = callback; }

private:
    // Writes a complete per-file patch or creates the selected inferred directory.
    // Newly created incomplete output is removed if extraction fails or is cancelled.
    void ExtractEntry(wstr_view destination);

    // Reports byte deltas at approximately 200 ms intervals, or immediately if forced.
    // Returns normally without a callback; throws kCancelled if the callback returns zero.
    void ReportProgress(uint64_t bytes, bool force);

    UniqueHandle source_file_;
    patch_backend::PatchArchive archive_;
    size_t entry_index_ = 0;
    bool header_pending_ = false;
    FILETIME last_write_time_{};
    uint32_t dos_time_ = 0;
    tProcessDataProcW callback_ = nullptr;
    uint64_t pending_bytes_ = 0;
    uint64_t last_progress_time_ = 0;
};

}  // namespace patch_wcx
