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
#include "error_translation.hpp"

namespace {

struct DeletionProgress {
    std::wstring path;
    tProcessDataProcW callback;
    uint64_t last_report = 0;
    bool reported = false;
};

// Converts write progress to WCX percentages, throttling intermediate reports.
// Cancellation is deliberately ignored because the source is being overwritten.
void ReportDeletionProgress(void* context, uint64_t written, uint64_t total) noexcept {
    auto& progress = *static_cast<DeletionProgress*>(context);
    if(!progress.callback) return;
    const uint64_t now = GetTickCount64();
    if(written != total && progress.reported && now - progress.last_report < 200) return;
    const int percent = total == 0 ? 100 : static_cast<int>(100.0L * written / total);
    progress.callback(progress.path.data(), -std::clamp(percent, 1, 100));
    progress.last_report = now;
    progress.reported = true;
}

// Decodes the WCX double-null-terminated selection, including directory \*.* suffixes.
// Throws backend errors; callback reports percentages and cannot cancel deletion.
void DeleteSelectedFiles(wstr_view archive_path, const wchar_t* delete_list, tProcessDataProcW callback) {
    std::vector<std::wstring> names;
    for(const wchar_t* current = delete_list; *current; ) {
        std::wstring name(current);
        current += name.size() + 1;
        std::replace(name.begin(), name.end(), L'\\', L'/');
        if(name.ends_with(L"/*.*")) name.resize(name.size() - 4);
        names.push_back(std::move(name));
    }
    std::vector<wstr_view> paths;
    paths.reserve(names.size());
    for(const auto& name : names) paths.emplace_back(name);
    DeletionProgress progress{archive_path.to_string(), callback};
    patch_backend::PatchArchive::DeleteEntries(archive_path, paths, {ReportDeletionProgress, &progress});
}

// Standalone deletion registers its callback without an archive handle, on the calling thread.
thread_local tProcessDataProcW deletion_callback = nullptr;

// TC may register a callback with a null or -1 handle for standalone operations.
bool IsArchiveHandle(HANDLE handle) noexcept {
    return handle != nullptr && handle != INVALID_HANDLE_VALUE;
}

}  // namespace

extern "C" __declspec(dllexport)
int __stdcall GetPackerCaps() {
    return PK_CAPS_MULTIPLE | PK_CAPS_SEARCHTEXT | PK_CAPS_DELETE | PK_CAPS_BY_CONTENT;
}

extern "C" __declspec(dllexport)
int __stdcall GetBackgroundFlags() {
    return BACKGROUND_UNPACK;
}

// Returns an owned per-archive context; all failures are reported through OpenResult.
extern "C" __declspec(dllexport)
HANDLE __stdcall OpenArchiveW(tOpenArchiveDataW* data) {
    if(!data) return nullptr;
    data->OpenResult = 0;
    data->CmtSize = data->CmtState = 0;
    if(!data->ArcName) {
        data->OpenResult = E_EOPEN;
        return nullptr;
    }
    if(data->OpenMode != PK_OM_LIST && data->OpenMode != PK_OM_EXTRACT) {
        data->OpenResult = E_NOT_SUPPORTED;
        return nullptr;
    }
    try {
        auto archive = std::make_unique<patch_wcx::ArchiveHandle>(data->ArcName);
        return archive.release();
    } catch(...) {
        data->OpenResult = patch_wcx::TranslateException(E_EREAD);
        return nullptr;
    }
}

extern "C" __declspec(dllexport)
int __stdcall CloseArchive(HANDLE handle) {
    if(!IsArchiveHandle(handle)) return E_BAD_DATA;
    delete static_cast<patch_wcx::ArchiveHandle*>(handle);
    return 0;
}

// WCX calls this alternately with ProcessFileW; names and sizes use the Unicode/64-bit ABI.
extern "C" __declspec(dllexport)
int __stdcall ReadHeaderExW(HANDLE handle, tHeaderDataExW* header) {
    if(!IsArchiveHandle(handle) || !header) return E_BAD_DATA;
    try {
        return static_cast<patch_wcx::ArchiveHandle*>(handle)->ReadHeader(*header);
    } catch(...) {
        return patch_wcx::TranslateException(E_EREAD);
    }
}

// No C++ exception, including callback cancellation, may cross the host ABI.
extern "C" __declspec(dllexport)
int __stdcall ProcessFileW(HANDLE handle, int operation, wchar_t* dest_path, wchar_t* dest_name) {
    if(!IsArchiveHandle(handle)) return E_BAD_DATA;
    try {
        return static_cast<patch_wcx::ArchiveHandle*>(handle)->ProcessFile(operation, dest_path, dest_name);
    } catch(...) {
        return patch_wcx::TranslateException(E_EWRITE);
    }
}

extern "C" __declspec(dllexport)
void __stdcall SetChangeVolProcW(HANDLE, tChangeVolProcW) {
    // Patches are single-volume files.
}

extern "C" __declspec(dllexport)
void __stdcall SetProcessDataProcW(HANDLE handle, tProcessDataProcW callback) {
    if(IsArchiveHandle(handle)) {
        static_cast<patch_wcx::ArchiveHandle*>(handle)->SetProcessDataProc(callback);
    } else {
        deletion_callback = callback;
    }
}

// Deletes the complete WCX selection in one operation; exceptions never cross the ABI.
extern "C" __declspec(dllexport)
int __stdcall DeleteFilesW(wchar_t* path, wchar_t* delete_list) {
    if(!path || !delete_list) return E_BAD_DATA;
    try {
        DeleteSelectedFiles(path, delete_list, deletion_callback);
        return 0;
    } catch(...) {
        return patch_wcx::TranslateException(E_EWRITE);
    }
}

// Bounded content recognition is tentative. Any error means the file is unrecognized.
extern "C" __declspec(dllexport)
BOOL __stdcall CanYouHandleThisFileW(wchar_t* path) {
    if(!path) return FALSE;
    try {
        return patch_backend::PatchArchive::CanOpen(path) ? TRUE : FALSE;
    } catch(...) {
        return FALSE;
    }
}
