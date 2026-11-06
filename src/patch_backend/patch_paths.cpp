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

#include "patch_paths.hpp"
#include "patch_error.hpp"

namespace patch_backend::detail {
namespace {

bool IsDeviceName(wstr_view component) {
    std::wstring stem = component.substr(0, component.find(L'.')).to_string();
    while(!stem.empty() && stem.back() == L' ') {
        stem.pop_back();
    }
    for(wchar_t& ch : stem) {
        if(ch >= L'a' && ch <= L'z') {
            ch -= L'a' - L'A';
        }
    }
    if(stem == L"CON" || stem == L"PRN" || stem == L"AUX" || stem == L"NUL" ||
       stem == L"CONIN$" || stem == L"CONOUT$") {
        return true;
    }
    return stem.size() == 4 && (stem.starts_with(L"COM") || stem.starts_with(L"LPT")) &&
           ((stem[3] >= L'1' && stem[3] <= L'9') || stem[3] == L'\u00B9' ||
            stem[3] == L'\u00B2' || stem[3] == L'\u00B3');
}

}  // namespace

std::wstring DecodePath(str_view path, bool has_utf8_bom, size_t byte_offset) {
    if(!path.empty() && path.front() == '"') {
        throw PatchError(ErrorCode::kUnsupportedFeature, "Quoted header paths are not supported yet",
                         byte_offset);
    }
    return DecodePathBytes(path, has_utf8_bom, byte_offset);
}

std::wstring DecodePathBytes(str_view path, bool has_utf8_bom, size_t byte_offset) {
    if(path.empty()) {
        throw PatchError(ErrorCode::kInvalidPath, "Empty file path", byte_offset);
    }
    if(path.length() > static_cast<size_t>(std::numeric_limits<int>::max())) {
        throw PatchError(ErrorCode::kLimitExceeded, "Path exceeds the conversion limit", byte_offset);
    }
    UINT code_page = CP_UTF8;
    const int byte_count = static_cast<int>(path.length());
    int length = MultiByteToWideChar(code_page, MB_ERR_INVALID_CHARS, path.data(), byte_count,
                                     nullptr, 0);
    if(length == 0 && !has_utf8_bom) {
        code_page = CP_ACP;
        length = MultiByteToWideChar(code_page, MB_ERR_INVALID_CHARS, path.data(), byte_count,
                                     nullptr, 0);
    }
    if(length == 0) {
        throw PatchError(ErrorCode::kInvalidPath, "Path encoding cannot be decoded", byte_offset);
    }
    std::wstring result(static_cast<size_t>(length), L'\0');
    if(MultiByteToWideChar(code_page, MB_ERR_INVALID_CHARS, path.data(), byte_count,
                           result.data(), length) != length) {
        throw PatchError(ErrorCode::kInvalidPath, "Path encoding cannot be decoded", byte_offset);
    }
    std::replace(result.begin(), result.end(), L'\\', L'/');
    size_t prefix = 0;
    while(result.compare(prefix, 2, L"./") == 0) {
        prefix += 2;
    }
    result.erase(0, prefix);
    size_t begin = 0;
    while(begin < result.size()) {
        const size_t separator = result.find(L'/', begin);
        const size_t end = separator == std::wstring::npos ? result.size() : separator;
        const std::wstring component = result.substr(begin, end - begin);
        if(component.empty() || component == L"." || component == L".." ||
           component.back() == L'.' || component.back() == L' ' || IsDeviceName(component)) {
            throw PatchError(ErrorCode::kInvalidPath, "Unsafe or ambiguous file path", byte_offset);
        }
        if(component.size() > 255) {
            throw PatchError(ErrorCode::kLimitExceeded, "Path component exceeds 255 characters",
                             byte_offset);
        }
        for(const wchar_t ch : component) {
            if(ch < 0x20 || wstr_view(L"<>:\"|?*").find(ch) != SIZE_MAX) {
                throw PatchError(ErrorCode::kInvalidPath, "Invalid Windows path character", byte_offset);
            }
        }
        begin = end + 1;
    }
    if(result.empty() || result.back() == L'/') {
        throw PatchError(ErrorCode::kInvalidPath, "Empty file name", byte_offset);
    }
    return result;
}

bool PathLess::operator()(wstr_view lhs, wstr_view rhs) const {
    return CompareStringOrdinal(lhs.c_str(), -1, rhs.c_str(), -1, TRUE) == CSTR_LESS_THAN;
}

}  // namespace patch_backend::detail
