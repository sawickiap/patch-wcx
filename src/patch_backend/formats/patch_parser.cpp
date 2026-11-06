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

#include "patch_parser.hpp"
#include "unified_parser.hpp"
#include "context_parser.hpp"
#include "git_parser.hpp"
#include "mailbox_parser.hpp"
#include "svn_parser.hpp"
#include "hg_parser.hpp"
#include "../patch_archive.hpp"
#include "../patch_error.hpp"
#include "../text_buffer.hpp"

namespace patch_backend::detail {
namespace {

// Chooses a parser from initial file headers or a supported mailbox envelope.
PatchFormat DetectFormat(const TextBuffer& text) {
    if(text.GetLineCount() == 0) return PatchFormat::kEmpty;
    size_t line = SkipBlankLines(text, 0);
    if(line == text.GetLineCount()) {
        throw PatchError(ErrorCode::kUnknownFormat, "No patch structure in input");
    }
    if(text.GetLine(line).starts_with(kGitSectionPrefix) || IsGitCommitPreamble(text.GetLine(line))) {
        return PatchFormat::kGit;
    }
    if(IsMailboxEnvelope(text.GetLine(line))) return PatchFormat::kGitMailbox;
    if(text.GetLine(line).starts_with(kSvnIndexPrefix)) return PatchFormat::kSvn;
    if(text.GetLine(line).starts_with(kHgDiffPrefix)) return PatchFormat::kHg;
    if(text.GetLine(line) == kHgExportMarker) return PatchFormat::kHgExport;
    CheckUnsupported(text, line);
    if(text.GetLine(line).starts_with("diff ")) ++line;
    if(line < text.GetLineCount()) {
        if(text.GetLine(line).starts_with("--- ")) return PatchFormat::kUnified;
        if(text.GetLine(line).starts_with("*** ")) return PatchFormat::kContext;
    }
    throw PatchError(ErrorCode::kUnknownFormat, "No supported patch file headers",
                     text.GetLineOffset(line));
}

}  // namespace

PatchFormat ValidatePatchPrefix(str_view prefix, bool truncated) {
    if(truncated) {
        size_t end = prefix.length();
        while(end != 0 && prefix[end - 1] != '\r' && prefix[end - 1] != '\n') --end;
        if(end == 0) {
            throw PatchError(ErrorCode::kUnknownFormat, "No complete line in probe");
        }
        prefix = prefix.substr(0, end);
    }
    const TextBuffer text(prefix.to_string());
    const PatchFormat format = DetectFormat(text);
    switch(format) {
    case PatchFormat::kEmpty: break;
    case PatchFormat::kUnified: ValidateUnifiedPrefix(text); break;
    case PatchFormat::kContext: ValidateContextPrefix(text); break;
    case PatchFormat::kGit: ValidateGitPrefix(text); break;
    case PatchFormat::kGitMailbox: ValidateMailboxPrefix(text); break;
    case PatchFormat::kSvn: ValidateSvnPrefix(text); break;
    case PatchFormat::kHg: ValidateHgPrefix(text); break;
    case PatchFormat::kHgExport: ValidateHgExportPrefix(text); break;
    }
    return format;
}

ParsedPatch ParsePatch(const TextBuffer& text) {
    const PatchFormat format = DetectFormat(text);
    switch(format) {
    case PatchFormat::kEmpty: return {format, {}};
    case PatchFormat::kUnified: return {format, ParseUnified(text)};
    case PatchFormat::kContext: return {format, ParseContext(text)};
    case PatchFormat::kGit: return ParseGit(text);
    case PatchFormat::kGitMailbox: return ParseMailbox(text);
    case PatchFormat::kSvn: return ParseSvn(text);
    case PatchFormat::kHg: return {format, ParseHg(text)};
    case PatchFormat::kHgExport: return ParseHgExport(text);
    }
    assert(false);
    return {PatchFormat::kEmpty, {}};
}

}  // namespace patch_backend::detail
