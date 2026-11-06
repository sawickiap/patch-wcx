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

#include "text_buffer.hpp"
#include "write_progress.hpp"

namespace patch_backend {

enum class PatchFormat { kEmpty, kUnified, kContext, kGit, kGitMailbox, kSvn, kHg, kHgExport };

struct ArchiveEntry {
    std::wstring path;  // Relative Unicode path, using '/' separators.
    bool is_directory;
    uint64_t size;      // Exact byte count returned by ReadEntry; zero for directories.
};

// Immutable after opening. No dependency on the Total Commander API.
class PatchArchive {
public:
    static PatchArchive Open(wstr_view path);
    static PatchArchive Parse(std::string bytes);

    // Deletes files/directories recursively in one disk rewrite; existing objects stay immutable.
    // Paths are case-insensitive archive names with '/' or '\\' separators. All must exist.
    // Empty selection does nothing. Invalid selections fail before writing. Throws PatchError
    // or std::bad_alloc; write failure may leave partial output. Progress cannot cancel.
    // Hidden SVN directory-property sections survive deletion of the last visible file.
    static void DeleteEntries(wstr_view archive_path, std::span<const wstr_view> paths,
                              WriteProgress progress = {});

    // Inspect at most 32 KiB. False includes empty, unsupported, and inconclusive input.
    // File I/O failures throw PatchError; allocation failures propagate as std::bad_alloc.
    static bool CanOpen(wstr_view path);
    static bool Recognize(str_view prefix);

    PatchArchive(const PatchArchive&) = delete;
    PatchArchive& operator=(const PatchArchive&) = delete;
    PatchArchive(PatchArchive&&) noexcept = default;
    PatchArchive& operator=(PatchArchive&&) noexcept = default;

    PatchFormat GetFormat() const noexcept { return format_; }
    bool HasUtf8Bom() const noexcept { return text_.HasUtf8Bom(); }
    // Views remain valid until the archive is moved or destroyed.
    std::span<const ArchiveEntry> GetEntries() const noexcept { return entries_; }
    // Windows FILETIME ticks (100 ns since 1601 UTC); absent for Parse().
    std::optional<uint64_t> GetLastWriteTime() const noexcept { return last_write_time_; }

    // Complete per-file patch, including its preamble and original BOM/line endings.
    // Git commit preambles and mail/Hg envelopes are retained; mailboxes omit commentary/diffstats.
    // Preconditions: entry_index is valid and denotes a file, not a directory.
    std::string ReadEntry(size_t entry_index) const;

private:
    explicit PatchArchive(std::string bytes);

    // Returns the extracted byte count for lines [first_line, end_line), including
    // original line endings, mailbox prefix/footer, and UTF-8 BOM if present.
    // Requires a valid line range.
    uint64_t CalculateFileSize(size_t first_line, size_t end_line) const;

    struct EntryRange {
        size_t first_line;
        size_t end_line;
    };

    detail::TextBuffer text_;
    PatchFormat format_ = PatchFormat::kEmpty;
    std::vector<ArchiveEntry> entries_;
    std::vector<EntryRange> ranges_;
    size_t prefix_end_ = 0;
    size_t patch_start_ = 0;
    size_t suffix_begin_ = 0;
    bool has_hidden_sections_ = false;
    std::optional<uint64_t> last_write_time_;
};

}  // namespace patch_backend
