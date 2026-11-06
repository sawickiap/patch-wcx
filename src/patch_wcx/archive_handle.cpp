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

#include "archive_handle.hpp"
#include <patch_backend/patch_error.hpp>

namespace patch_wcx {
namespace {

constexpr uint64_t kProgressIntervalMs = 200;
constexpr size_t kWriteChunkSize = 1024 * 1024;

// Captures the current Win32 error before allocating owned exception details.
[[noreturn]] void ThrowIoError(str_view operation, wstr_view path) {
    const DWORD error = GetLastError();
    throw patch_backend::PatchError(patch_backend::IoErrorDetails{
        operation.to_string(), path.to_string(), error});
}

// Holds a read-only share lock so extraction cannot overwrite the source patch,
// including through another name for the same file. Throws on an open failure.
UniqueHandle OpenSourceFile(wstr_view path) {
    const HANDLE handle = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                                      OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if(handle == INVALID_HANDLE_VALUE) ThrowIoError("CreateFileW", path);
    return UniqueHandle(handle);
}

// Joins the two WCX destination forms and normalizes separators for WinAPI.
// Throws kInvalidPath for an empty destination.
std::wstring ResolveDestination(wstr_view path, wstr_view name) {
    std::wstring result = path.to_string();
    if(!name.empty()) {
        if(!result.empty() && result.back() != L'\\' && result.back() != L'/') {
            result += L'\\';
        }
        result.append(name.data(), name.length());
    }
    std::replace(result.begin(), result.end(), L'/', L'\\');
    while(result.size() > 1 && result.back() == L'\\' && result[result.size() - 2] != L':') {
        result.pop_back();
    }
    if(result.empty()) {
        throw patch_backend::PatchError(patch_backend::ErrorCode::kInvalidPath,
                                        "Missing extraction destination");
    }
    return result;
}

// Borrows the parent portion, preserving a drive/root separator; empty means no parent.
wstr_view ParentDirectory(wstr_view path) {
    const size_t separator = path.rfind(L'\\');
    if(separator == SIZE_MAX) return {};
    const size_t length = separator == 0 || path[separator - 1] == L':' ? separator + 1 : separator;
    return path.substr(0, length);
}

// Creates missing ancestors; returns whether this call created the requested directory.
// Existing directories succeed; files and I/O failures throw.
bool EnsureDirectory(wstr_view path) {
    if(path.empty()) return false;
    const DWORD attributes = GetFileAttributesW(path.c_str());
    if(attributes != INVALID_FILE_ATTRIBUTES) {
        if((attributes & FILE_ATTRIBUTE_DIRECTORY) != 0) return false;
        throw patch_backend::PatchError(patch_backend::IoErrorDetails{
            "CreateDirectoryW", path.to_string(), ERROR_DIRECTORY});
    }
    const DWORD error = GetLastError();
    if(error != ERROR_FILE_NOT_FOUND && error != ERROR_PATH_NOT_FOUND) {
        throw patch_backend::PatchError(patch_backend::IoErrorDetails{
            "CreateDirectoryW", path.to_string(), error});
    }
    const wstr_view parent = ParentDirectory(path);
    if(!parent.empty() && parent != path) {
        EnsureDirectory(parent);
    }
    if(!CreateDirectoryW(path.c_str(), nullptr)) {
        // A concurrent extraction can have created the same parent directory.
        if(GetLastError() == ERROR_ALREADY_EXISTS) {
            const DWORD existing = GetFileAttributesW(path.c_str());
            if(existing != INVALID_FILE_ATTRIBUTES && (existing & FILE_ATTRIBUTE_DIRECTORY) != 0) return false;
            throw patch_backend::PatchError(patch_backend::IoErrorDetails{
                "CreateDirectoryW", path.to_string(), ERROR_DIRECTORY});
        }
        ThrowIoError("CreateDirectoryW", path);
    }
    return true;
}

}  // namespace

ArchiveHandle::ArchiveHandle(wstr_view path)
    : source_file_(OpenSourceFile(path)), archive_(patch_backend::PatchArchive::Open(path)) {
    const uint64_t timestamp = archive_.GetLastWriteTime().value();
    last_write_time_.dwLowDateTime = static_cast<DWORD>(timestamp);
    last_write_time_.dwHighDateTime = static_cast<DWORD>(timestamp >> 32);
    FILETIME local_time{};
    WORD date = 0;
    WORD time = 0;
    if(FileTimeToLocalFileTime(&last_write_time_, &local_time) &&
       FileTimeToDosDateTime(&local_time, &date, &time)) {
        dos_time_ = (static_cast<uint32_t>(date) << 16) | time;
    }
}

int ArchiveHandle::ReadHeader(tHeaderDataExW& header) {
    header = {};
    if(entry_index_ == archive_.GetEntries().size()) {
        ReportProgress(0, true);
        return E_END_ARCHIVE;
    }
    const auto& entry = archive_.GetEntries()[entry_index_];
    constexpr size_t kNameCapacity = sizeof(header.FileName) / sizeof(header.FileName[0]);
    if(entry.path.size() >= kNameCapacity) return E_SMALL_BUF;
    std::copy(entry.path.begin(), entry.path.end(), header.FileName);
    std::replace(header.FileName, header.FileName + entry.path.size(), L'/', L'\\');
    header.PackSize = header.UnpSize = static_cast<uint32_t>(entry.size);
    header.PackSizeHigh = header.UnpSizeHigh = static_cast<uint32_t>(entry.size >> 32);
    header.FileTime = static_cast<int32_t>(dos_time_);
    header.FileAttr = entry.is_directory ? FILE_ATTRIBUTE_DIRECTORY : 0;
    header_pending_ = true;
    return 0;
}

int ArchiveHandle::ProcessFile(int operation, wstr_view dest_path, wstr_view dest_name) {
    if(!header_pending_) return E_BAD_DATA;
    switch(operation) {
    case PK_SKIP:
    case PK_TEST:
        ReportProgress(archive_.GetEntries()[entry_index_].size, false);
        break;  // Opening already checked the structure needed for extraction.
    case PK_EXTRACT:
        ExtractEntry(ResolveDestination(dest_path, dest_name));
        break;
    default:
        return E_NOT_SUPPORTED;
    }
    header_pending_ = false;
    ++entry_index_;
    return 0;
}

void ArchiveHandle::ReportProgress(uint64_t bytes, bool force) {
    pending_bytes_ += bytes;
    if(!callback_) {
        pending_bytes_ = 0;
        return;
    }
    const uint64_t now = GetTickCount64();
    if(!force && now - last_progress_time_ < kProgressIntervalMs) return;
    do {
        const int delta = static_cast<int>(std::min<uint64_t>(pending_bytes_,
                                                            std::numeric_limits<int>::max()));
        if(callback_(nullptr, delta) == 0) {
            throw patch_backend::PatchError(patch_backend::ErrorCode::kCancelled,
                                            "Extraction cancelled");
        }
        pending_bytes_ -= static_cast<uint64_t>(delta);
    } while(pending_bytes_ != 0);
    last_progress_time_ = now;
}

void ArchiveHandle::ExtractEntry(wstr_view destination) {
    const auto& entry = archive_.GetEntries()[entry_index_];
    ReportProgress(0, true);
    if(entry.is_directory) {
        const bool created = EnsureDirectory(destination);
        try {
            const HANDLE handle = CreateFileW(destination.c_str(), FILE_WRITE_ATTRIBUTES,
                FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
                OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
            if(handle == INVALID_HANDLE_VALUE) ThrowIoError("CreateFileW", destination);
            const UniqueHandle directory(handle);
            if(!SetFileTime(directory.get(), nullptr, nullptr, &last_write_time_)) {
                ThrowIoError("SetFileTime", destination);
            }
            ReportProgress(0, true);
        } catch(...) {
            if(created) RemoveDirectoryW(destination.c_str());
            throw;
        }
        return;
    }

    const std::string contents = archive_.ReadEntry(entry_index_);
    EnsureDirectory(ParentDirectory(destination));
    const HANDLE handle = CreateFileW(destination.c_str(), GENERIC_WRITE, 0, nullptr,
                                     CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if(handle == INVALID_HANDLE_VALUE) ThrowIoError("CreateFileW", destination);
    const bool newly_created = GetLastError() != ERROR_ALREADY_EXISTS;
    UniqueHandle file(handle);
    try {
        size_t position = 0;
        while(position < contents.size()) {
            const DWORD count = static_cast<DWORD>(std::min(kWriteChunkSize, contents.size() - position));
            DWORD written = 0;
            if(!WriteFile(file.get(), contents.data() + position, count, &written, nullptr)) {
                ThrowIoError("WriteFile", destination);
            }
            if(written == 0) {
                throw patch_backend::PatchError(patch_backend::IoErrorDetails{
                    "WriteFile", destination.to_string(), ERROR_WRITE_FAULT});
            }
            position += written;
            ReportProgress(written, false);
        }
        if(!SetFileTime(file.get(), nullptr, nullptr, &last_write_time_)) {
            ThrowIoError("SetFileTime", destination);
        }
        ReportProgress(0, true);
        if(!CloseHandle(file.get())) ThrowIoError("CloseHandle", destination);
        file.release();
    } catch(...) {
        file.reset();
        if(newly_created) DeleteFileW(destination.c_str());
        throw;
    }
}

}  // namespace patch_wcx
