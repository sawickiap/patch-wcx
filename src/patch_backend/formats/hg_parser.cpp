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

#include "hg_parser.hpp"
#include "git_parser.hpp"
#include "unified_parser.hpp"
#include "../patch_archive.hpp"
#include "../patch_paths.hpp"
#include "../patch_error.hpp"
#include "../text_buffer.hpp"

namespace patch_backend::detail {
namespace {

struct Revisions {
    str_view old_revision;
    str_view new_revision;
};

struct Preamble {
    Revisions revisions;
    std::wstring path;
};

// Skips an opaque revision token and its following space, preserving the path tail.
str_view ReadRevision(const TextBuffer& text, size_t line, size_t& position) {
    const str_view value = text.GetLine(line);
    const size_t end = value.find(' ', position);
    if(end == SIZE_MAX || end == position) Invalid(text, line, "Missing Mercurial revision or path");
    const str_view revision = value.substr(position, end - position);
    position = end + 1;
    return revision;
}

// One or two -r tokens describe one comparison, not a sequence of changesets.
Preamble ReadPreamble(const TextBuffer& text, size_t line) {
    if(line == text.GetLineCount() || !text.GetLine(line).starts_with(kHgDiffPrefix)) {
        if(line < text.GetLineCount()) CheckUnsupported(text, line);
        Invalid(text, line, "Expected Mercurial diff preamble");
    }
    size_t position = sizeof(kHgDiffPrefix) - 1;
    const str_view old_revision = ReadRevision(text, line, position);
    str_view new_revision;
    const str_view value = text.GetLine(line);
    if(value.substr(position).starts_with("-r ")) {
        position += 3;
        new_revision = ReadRevision(text, line, position);
    }
    return {{old_revision, new_revision}, DecodePath(value.substr(position), text.HasUtf8Bom(),
                                                    text.GetLineOffset(line))};
}

// Ignores header names, artificial prefixes and dates; the preamble supplies the path.
size_t FileBody(const TextBuffer& text, size_t line) {
    if(line + 1 >= text.GetLineCount() || !text.GetLine(line).starts_with("--- ") ||
       !text.GetLine(line + 1).starts_with("+++ ")) {
        Invalid(text, line, "Missing Mercurial file headers");
    }
    return line + 2;
}

// Uses shared unified traversal; binary notices are complete document entries.
ParsedFile ReadSection(const TextBuffer& text, size_t& line, std::optional<Revisions>& comparison) {
    const size_t first = line;
    auto header = ReadPreamble(text, line);
    if(comparison && (comparison->old_revision != header.revisions.old_revision ||
                      comparison->new_revision != header.revisions.new_revision)) {
        throw PatchError(ErrorCode::kUnsupportedFeature, "Multiple Mercurial comparisons are unsupported",
                         text.GetLineOffset(first));
    }
    comparison = header.revisions;
    ++line;
    if(line < text.GetLineCount() && IsBinaryChangeNotice(text.GetLine(line))) {
        ValidateBinaryChangeNotice(text, line);
        ++line;
    } else {
        line = FileBody(text, line);
        if(line == text.GetLineCount() || !text.GetLine(line).starts_with("@@ ")) {
            Invalid(text, line, "Missing Mercurial hunk header");
        }
        do {
            line = SkipUnifiedHunk(text, line);
        } while(line < text.GetLineCount() && text.GetLine(line).starts_with("@@ "));
    }
    return {std::move(header.path), first, line};
}

// Header values are provenance, not a certificate of filtered changeset identity.
size_t ReadExportHeader(const TextBuffer& text) {
    const size_t first = SkipBlankLines(text, 0);
    if(first == text.GetLineCount() || text.GetLine(first) != kHgExportMarker) {
        Invalid(text, first, "Missing Mercurial export marker");
    }
    bool user = false;
    bool date = false;
    bool node = false;
    bool parent = false;
    size_t line = first + 1;
    for(; line < text.GetLineCount(); ++line) {
        const str_view value = text.GetLine(line);
        if(value == kHgExportMarker) {
            throw PatchError(ErrorCode::kUnsupportedFeature, "Multiple Mercurial exports are unsupported",
                             text.GetLineOffset(line));
        }
        if(!value.starts_with('#')) break;
        user = user || value.starts_with("# User ");
        date = date || value.starts_with("# Date ");
        node = node || value.starts_with("# Node ID ");
        parent = parent || value.starts_with("# Parent ");
    }
    if(!user || !date || !node || !parent) Invalid(text, first, "Missing Mercurial export metadata");
    return line;
}

// Avoids treating a quoted diff command alone in a message as the actual patch start.
bool IsPatchStart(const TextBuffer& text, size_t line) {
    const str_view value = text.GetLine(line);
    if(value.starts_with(kGitSectionPrefix)) return IsGitCandidate(text, line);
    if(!value.starts_with(kHgDiffPrefix)) return false;
    if(line + 1 == text.GetLineCount()) return false;
    const str_view next = text.GetLine(line + 1);
    return next.starts_with("--- ") || IsBinaryChangeNotice(next);
}

}  // namespace

void ValidateHgPrefix(const TextBuffer& text) {
    const size_t first = SkipBlankLines(text, 0);
    ReadPreamble(text, first);
    size_t line = first + 1;
    if(line < text.GetLineCount() && IsBinaryChangeNotice(text.GetLine(line))) {
        ValidateBinaryChangeNotice(text, line);
        return;
    }
    line = FileBody(text, line);
    if(line == text.GetLineCount()) Invalid(text, line, "No Mercurial hunk header in probe");
    ValidateUnifiedHunkHeader(text.GetLine(line), text.GetLineOffset(line));
}

std::vector<ParsedFile> ParseHg(const TextBuffer& text) {
    std::vector<ParsedFile> files;
    std::optional<Revisions> comparison;
    size_t line = SkipBlankLines(text, 0);
    while(line < text.GetLineCount()) {
        files.push_back(ReadSection(text, line, comparison));
        line = SkipBlankLines(text, line);
    }
    return files;
}

void ValidateHgExportPrefix(const TextBuffer& text) {
    ReadExportHeader(text);
}

ParsedPatch ParseHgExport(const TextBuffer& text) {
    const size_t body = ReadExportHeader(text);
    const size_t first = SkipBlankLines(text, 0);
    for(size_t line = first + 1; line < text.GetLineCount(); ++line) {
        if(text.GetLine(line) == kHgExportMarker) {
            throw PatchError(ErrorCode::kUnsupportedFeature, "Multiple Mercurial exports are unsupported",
                             text.GetLineOffset(line));
        }
    }
    size_t line = body;
    for(; line < text.GetLineCount(); ++line) {
        if(IsPatchStart(text, line)) break;
    }
    if(line == text.GetLineCount()) Invalid(text, line, "Mercurial export has no patch sections");
    ParsedPatch result{PatchFormat::kHgExport, {}};
    result.prefix_end = result.patch_start = line;
    const bool git = text.GetLine(line).starts_with(kGitSectionPrefix);
    std::optional<Revisions> comparison;
    while(line < text.GetLineCount()) {
        result.files.push_back(git ? ParseGitSection(text, line) : ReadSection(text, line, comparison));
        result.suffix_begin = line;
        line = SkipBlankLines(text, line);
    }
    return result;
}

}  // namespace patch_backend::detail
