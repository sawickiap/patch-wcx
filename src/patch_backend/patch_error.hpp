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

namespace patch_backend {

enum class ErrorCode {
    kUnknownFormat,
    kInvalidPatch,
    kUnsupportedFeature,
    kInvalidPath,
    kLimitExceeded,
    kIoError,
    kCancelled,
};

struct IoErrorDetails {
    std::string operation;
    std::wstring path;
    uint32_t win32_error;
};

class PatchError : public std::exception {
public:
    PatchError(ErrorCode code, std::string message,
               std::optional<size_t> byte_offset = std::nullopt);
    explicit PatchError(IoErrorDetails details);

    const char* what() const noexcept override;
    ErrorCode GetCode() const noexcept { return code_; }
    std::optional<size_t> GetByteOffset() const noexcept { return byte_offset_; }
    const std::optional<IoErrorDetails>& GetIoDetails() const noexcept { return io_details_; }

private:
    ErrorCode code_;
    std::string message_;
    std::optional<size_t> byte_offset_;
    std::optional<IoErrorDetails> io_details_;
};

}  // namespace patch_backend
