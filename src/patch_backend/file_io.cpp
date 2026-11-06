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

#include "file_io.hpp"
#include "formats/patch_parser.hpp"
#include "patch_error.hpp"

namespace patch_backend::detail {
namespace {

[[noreturn]] void ThrowIoError(str_view operation, wstr_view path) {
    const DWORD error = GetLastError();
    throw PatchError(IoErrorDetails{operation.to_string(), path.to_string(), error});
}

UniqueHandle OpenFile(wstr_view path) {
    const HANDLE handle = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                                     OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
    if(handle == INVALID_HANDLE_VALUE) {
        ThrowIoError("CreateFileW", path);
    }
    return UniqueHandle(handle);
}

size_t ReadBytes(HANDLE handle, char* destination, size_t size, wstr_view path) {
    size_t total = 0;
    while(total < size) {
        const DWORD chunk = static_cast<DWORD>(std::min<size_t>(size - total, 1024 * 1024));
        DWORD read = 0;
        if(!ReadFile(handle, destination + total, chunk, &read, nullptr)) {
            ThrowIoError("ReadFile", path);
        }
        if(read == 0) {
            break;
        }
        total += read;
    }
    return total;
}

}  // namespace

FilePrefix ReadPrefix(wstr_view path) {
    const auto file = OpenFile(path);
    LARGE_INTEGER file_size{};
    if(!GetFileSizeEx(file.get(), &file_size)) ThrowIoError("GetFileSizeEx", path);
    FilePrefix result;
    result.bytes.resize(static_cast<size_t>(std::min<uint64_t>(file_size.QuadPart, kProbeSize)));
    if(ReadBytes(file.get(), result.bytes.data(), result.bytes.size(), path) != result.bytes.size()) {
        throw PatchError(IoErrorDetails{"ReadFile", path.to_string(), ERROR_HANDLE_EOF});
    }
    result.truncated = static_cast<uint64_t>(file_size.QuadPart) > result.bytes.size();
    return result;
}

FileContents ReadPatchFile(wstr_view path) {
    const auto file = OpenFile(path);
    return ReadPatchFile(file.get(), path);
}

UniqueHandle OpenPatchForUpdate(wstr_view path) {
    const HANDLE file = CreateFileW(path.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr,
                                    OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
    if(file == INVALID_HANDLE_VALUE) ThrowIoError("CreateFileW", path);
    return UniqueHandle(file);
}

FileContents ReadPatchFile(HANDLE file, wstr_view path) {
    LARGE_INTEGER file_size{};
    FILETIME last_write{};
    if(!GetFileSizeEx(file, &file_size)) {
        ThrowIoError("GetFileSizeEx", path);
    }
    if(!GetFileTime(file, nullptr, nullptr, &last_write)) {
        ThrowIoError("GetFileTime", path);
    }
    FileContents result;
    result.last_write_time = (static_cast<uint64_t>(last_write.dwHighDateTime) << 32) |
                             last_write.dwLowDateTime;
    result.bytes.resize(static_cast<size_t>(std::min<uint64_t>(file_size.QuadPart, kProbeSize)));
    if(ReadBytes(file, result.bytes.data(), result.bytes.size(), path) != result.bytes.size()) {
        throw PatchError(IoErrorDetails{"ReadFile", path.to_string(), ERROR_HANDLE_EOF});
    }
    ValidatePatchPrefix(result.bytes, static_cast<uint64_t>(file_size.QuadPart) > result.bytes.size());
    if(static_cast<uint64_t>(file_size.QuadPart) > result.bytes.max_size()) {
        throw PatchError(ErrorCode::kLimitExceeded, "Patch exceeds the string storage limit");
    }
    const size_t prefix_size = result.bytes.size();
    result.bytes.resize(static_cast<size_t>(file_size.QuadPart));
    if(ReadBytes(file, result.bytes.data() + prefix_size, result.bytes.size() - prefix_size,
                 path) != result.bytes.size() - prefix_size) {
        throw PatchError(IoErrorDetails{"ReadFile", path.to_string(), ERROR_HANDLE_EOF});
    }
    return result;
}

void RewritePatchFile(HANDLE file, wstr_view path, str_view bytes, WriteProgress progress) {
    if(progress.callback && !bytes.empty()) progress.callback(progress.context, 0, bytes.length());
    LARGE_INTEGER beginning{};
    if(!SetFilePointerEx(file, beginning, nullptr, FILE_BEGIN)) ThrowIoError("SetFilePointerEx", path);
    size_t position = 0;
    while(position < bytes.length()) {
        const DWORD chunk = static_cast<DWORD>(std::min<size_t>(bytes.length() - position, 1024 * 1024));
        DWORD written = 0;
        if(!WriteFile(file, bytes.data() + position, chunk, &written, nullptr)) {
            ThrowIoError("WriteFile", path);
        }
        if(written == 0) {
            throw PatchError(IoErrorDetails{"WriteFile", path.to_string(), ERROR_WRITE_FAULT});
        }
        position += written;
        if(progress.callback && position < bytes.length()) {
            progress.callback(progress.context, position, bytes.length());
        }
    }
    if(!SetEndOfFile(file)) ThrowIoError("SetEndOfFile", path);
    if(!FlushFileBuffers(file)) ThrowIoError("FlushFileBuffers", path);
}

void ClosePatchFile(UniqueHandle& file, wstr_view path) {
    if(!CloseHandle(file.get())) ThrowIoError("CloseHandle", path);
    file.release();
}

}  // namespace patch_backend::detail
