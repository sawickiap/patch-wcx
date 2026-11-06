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

// Optional synchronous notification of bytes written / total output bytes.
// Called on the deleting thread; cannot throw or cancel. Context is borrowed.
struct WriteProgress {
    void (*callback)(void* context, uint64_t written, uint64_t total) noexcept = nullptr;
    void* context = nullptr;
};

}  // namespace patch_backend
