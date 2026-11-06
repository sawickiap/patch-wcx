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

#include "error_translation.hpp"
#include <patch_backend/patch_error.hpp>

namespace patch_wcx {

int TranslateException(int io_error) noexcept {
    using namespace patch_backend;
    try {
        throw;
    } catch(const PatchError& error) {
        switch(error.GetCode()) {
        case ErrorCode::kUnknownFormat: return E_UNKNOWN_FORMAT;
        case ErrorCode::kInvalidPatch:
        case ErrorCode::kInvalidPath: return E_BAD_ARCHIVE;
        case ErrorCode::kUnsupportedFeature: return E_NOT_SUPPORTED;
        case ErrorCode::kLimitExceeded: return E_SMALL_BUF;
        case ErrorCode::kCancelled: return E_EABORTED;
        case ErrorCode::kIoError:
            if(error.GetIoDetails()) {
                const auto& operation = error.GetIoDetails()->operation;
                if(operation == "CreateFileW") {
                    return io_error == E_EWRITE ? E_ECREATE : E_EOPEN;
                }
                if(operation == "CreateDirectoryW") return E_ECREATE;
                if(operation == "ReadFile") return E_EREAD;
                if(operation == "WriteFile" || operation == "SetFileTime") return E_EWRITE;
                if(operation == "SetFilePointerEx" || operation == "SetEndOfFile" ||
                   operation == "FlushFileBuffers") return E_EWRITE;
                if(operation == "CloseHandle") return E_ECLOSE;
            }
            return io_error;
        }
        return E_BAD_DATA;
    } catch(const std::bad_alloc&) {
        return E_NO_MEMORY;
    } catch(...) {
        return E_BAD_DATA;
    }
}

}  // namespace patch_wcx
