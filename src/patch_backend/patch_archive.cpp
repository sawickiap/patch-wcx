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

#include "patch_archive.hpp"
#include "patch_error.hpp"
#include "file_io.hpp"
#include "patch_paths.hpp"
#include "formats/patch_parser.hpp"

namespace patch_backend {
namespace {

struct ByteRange {
    size_t begin;
    size_t end;
};

// Shares tentative recognition for disk prefixes and borrowed in-memory input.
bool RecognizePrefix(str_view prefix, bool truncated) {
    try {
        return detail::ValidatePatchPrefix(prefix, truncated) != PatchFormat::kEmpty;
    } catch(const PatchError&) {
        return false;
    }
}

// Removes sorted, non-overlapping byte ranges in place, preserving all intervening bytes.
void RemoveRanges(std::string& bytes, std::span<const ByteRange> ranges) {
    size_t source = 0;
    size_t destination = 0;
    for(const auto& range : ranges) {
        assert(source <= range.begin && range.begin <= range.end && range.end <= bytes.size());
        const size_t count = range.begin - source;
        if(count != 0 && destination != source) {
            std::char_traits<char>::move(bytes.data() + destination, bytes.data() + source, count);
        }
        destination += count;
        source = range.end;
    }
    const size_t tail = bytes.size() - source;
    if(tail != 0 && destination != source) {
        std::char_traits<char>::move(bytes.data() + destination, bytes.data() + source, tail);
    }
    bytes.resize(destination + tail);
}

}  // namespace


PatchArchive PatchArchive::Open(wstr_view path) {
    auto contents = detail::ReadPatchFile(path);
    PatchArchive archive(std::move(contents.bytes));
    archive.last_write_time_ = contents.last_write_time;
    return archive;
}

PatchArchive PatchArchive::Parse(std::string bytes) {
    detail::ValidatePatchPrefix(str_view(bytes).substr(0, detail::kProbeSize), bytes.size() > detail::kProbeSize);
    return PatchArchive(std::move(bytes));
}

bool PatchArchive::CanOpen(wstr_view path) {
    const auto prefix = detail::ReadPrefix(path);
    return RecognizePrefix(prefix.bytes, prefix.truncated);
}

bool PatchArchive::Recognize(str_view prefix) {
    return RecognizePrefix(prefix.substr(0, detail::kProbeSize), prefix.length() > detail::kProbeSize);
}

PatchArchive::PatchArchive(std::string bytes) : text_(std::move(bytes)) {
    const auto parsed = detail::ParsePatch(text_);
    format_ = parsed.format;
    prefix_end_ = parsed.prefix_end;
    patch_start_ = parsed.patch_start;
    suffix_begin_ = parsed.suffix_begin;
    has_hidden_sections_ = parsed.has_hidden_sections;
    std::map<std::wstring, size_t, detail::PathLess> paths;
    for(const auto& file : parsed.files) {
        size_t separator = file.path.find(L'/');
        while(separator != std::wstring::npos) {
            const std::wstring directory = file.path.substr(0, separator);
            const auto found = paths.find(directory);
            if(found == paths.end()) {
                paths.emplace(directory, entries_.size());
                entries_.push_back({directory, true, 0});
                ranges_.push_back({0, 0});
            } else if(!entries_[found->second].is_directory || found->first != directory) {
                throw PatchError(ErrorCode::kInvalidPath, "File/directory or case collision",
                                 text_.GetLineOffset(file.first_line));
            }
            separator = file.path.find(L'/', separator + 1);
        }
        if(paths.find(file.path) != paths.end()) {
            throw PatchError(ErrorCode::kInvalidPath, "Duplicate path or file/directory collision",
                             text_.GetLineOffset(file.first_line));
        }
        paths.emplace(file.path, entries_.size());
        const uint64_t size = CalculateFileSize(file.first_line, file.end_line);
        entries_.push_back({file.path, false, size});
        ranges_.push_back({file.first_line, file.end_line});
    }
}

uint64_t PatchArchive::CalculateFileSize(size_t first_line, size_t end_line) const {
    const uint64_t envelope_size = prefix_end_ != 0 ?
        text_.GetBytes(0, prefix_end_).length() + text_.GetBytes(suffix_begin_, text_.GetLineCount()).length() : 0;
    return envelope_size + text_.GetBytes(first_line, end_line).length() +
           (text_.HasUtf8Bom() ? sizeof(detail::kUtf8Bom) - 1 : 0);
}

std::string PatchArchive::ReadEntry(size_t entry_index) const {
    assert(entry_index < entries_.size() && !entries_[entry_index].is_directory);
    const EntryRange& range = ranges_[entry_index];
    const str_view bytes = text_.GetBytes(range.first_line, range.end_line);
    std::string result = text_.HasUtf8Bom() ? detail::kUtf8Bom : "";
    if(prefix_end_ != 0) {
        const str_view prefix = text_.GetBytes(0, prefix_end_);
        result.append(prefix.data(), prefix.length());
    }
    result.append(bytes.data(), bytes.length());
    if(prefix_end_ != 0) {
        const str_view suffix = text_.GetBytes(suffix_begin_, text_.GetLineCount());
        result.append(suffix.data(), suffix.length());
    }
    return result;
}

void PatchArchive::DeleteEntries(wstr_view archive_path, std::span<const wstr_view> paths,
                                 WriteProgress progress) {
    if(paths.empty()) return;
    auto file = detail::OpenPatchForUpdate(archive_path);
    auto contents = detail::ReadPatchFile(file.get(), archive_path);
    PatchArchive archive(std::move(contents.bytes));

    std::map<std::wstring, size_t, detail::PathLess> indices;
    for(size_t i = 0; i < archive.entries_.size(); ++i) {
        indices.emplace(archive.entries_[i].path, i);
    }
    std::vector<bool> selected(archive.entries_.size(), false);
    for(const auto path : paths) {
        std::wstring name = path.to_string();
        std::replace(name.begin(), name.end(), L'\\', L'/');
        while(!name.empty() && name.back() == L'/') name.pop_back();
        if(name.empty() || name.find(L'\0') != std::wstring::npos) {
            throw PatchError(ErrorCode::kInvalidPath, "Invalid deletion path");
        }
        const auto found = indices.find(name);
        if(found == indices.end()) {
            throw PatchError(ErrorCode::kInvalidPath, "Deletion path does not exist in archive");
        }
        selected[found->second] = true;
    }

    std::vector<ByteRange> ranges;
    if(archive.format_ == PatchFormat::kGitMailbox && archive.prefix_end_ < archive.patch_start_) {
        ranges.push_back({archive.text_.GetLineOffset(archive.prefix_end_),
                          archive.text_.GetLineOffset(archive.patch_start_)});
    }
    size_t remaining_files = 0;
    for(size_t i = 0; i < archive.entries_.size(); ++i) {
        const auto& entry = archive.entries_[i];
        const size_t separator = entry.path.rfind(L'/');
        if(separator != std::wstring::npos) {
            const auto parent = indices.find(entry.path.substr(0, separator));
            assert(parent != indices.end() && parent->second < i);
            selected[i] = selected[i] || selected[parent->second];
        }
        if(entry.is_directory) continue;
        if(selected[i]) {
            const auto& range = archive.ranges_[i];
            ranges.push_back({archive.text_.GetLineOffset(range.first_line),
                              archive.text_.GetLineOffset(range.end_line)});
        } else {
            ++remaining_files;
        }
    }

    const size_t bom_size = archive.HasUtf8Bom() ? sizeof(detail::kUtf8Bom) - 1 : 0;
    std::string bytes = std::move(archive.text_).TakeCharacters();
    if(remaining_files == 0 && !archive.has_hidden_sections_) {
        bytes.resize(bom_size);
    } else {
        RemoveRanges(bytes, ranges);
    }
    detail::RewritePatchFile(file.get(), archive_path, bytes, progress);
    detail::ClosePatchFile(file, archive_path);
    if(progress.callback) progress.callback(progress.context, bytes.size(), bytes.size());
}

}  // namespace patch_backend
