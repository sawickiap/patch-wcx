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

#include "patch_error.hpp"

namespace patch_backend {

PatchError::PatchError(ErrorCode code, std::string message, std::optional<size_t> byte_offset)
    : code_(code), message_(std::move(message)), byte_offset_(byte_offset) {}

PatchError::PatchError(IoErrorDetails details)
    : code_(ErrorCode::kIoError), message_(details.operation + " failed"),
      io_details_(std::move(details)) {}

const char* PatchError::what() const noexcept {
    return message_.c_str();
}

}  // namespace patch_backend
