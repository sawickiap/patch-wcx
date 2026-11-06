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

#include "mailbox_parser.hpp"
#include "git_parser.hpp"
#include "../patch_archive.hpp"
#include "../patch_error.hpp"
#include "../text_buffer.hpp"

namespace patch_backend::detail {
namespace {

// Mail field names and MIME tokens are ASCII case-insensitive; contents stay untouched.
std::string LowerAscii(str_view value) {
    std::string result = value.to_string();
    for(char& ch : result) {
        if(ch >= 'A' && ch <= 'Z') ch += 'a' - 'A';
    }
    return result;
}

// Strips horizontal header whitespace without changing the original message bytes.
str_view TrimSpace(str_view value) {
    while(!value.empty() && (value.front() == ' ' || value.front() == '\t')) value.remove_prefix(1);
    while(!value.empty() && (value.back() == ' ' || value.back() == '\t')) value.remove_suffix(1);
    return value;
}

struct MailHeaders {
    bool from = false;
    bool date = false;
    bool subject = false;
};

// Inspects only required identity fields and MIME controls, not message/header semantics.
void ReadField(const TextBuffer& text, size_t line, str_view name, str_view value, MailHeaders& headers) {
    if(name == "from") headers.from = !TrimSpace(value).empty();
    else if(name == "date") headers.date = !TrimSpace(value).empty();
    else if(name == "subject") headers.subject = true;
    else if(name == "content-type" || name == "content-transfer-encoding" || name == "content-disposition") {
        const std::string lower = LowerAscii(value);
        const size_t separator = lower.find(';');
        const str_view token = TrimSpace(str_view(lower).substr(0, separator));
        if((name == "content-type" && token != "text/plain") ||
           (name == "content-transfer-encoding" && token != "7bit" && token != "8bit" && token != "binary") ||
           (name == "content-disposition" && token != "inline")) {
            throw PatchError(ErrorCode::kUnsupportedFeature, "Only unencoded plain-text mail is supported",
                             text.GetLineOffset(line));
        }
    }
}

// Unfolds headers only for recognition/MIME checks. Original bytes are never rewritten.
size_t ReadMailHeaders(const TextBuffer& text, bool probe) {
    const size_t first = SkipBlankLines(text, 0);
    if(first == text.GetLineCount() || !IsMailboxEnvelope(text.GetLine(first))) {
        Invalid(text, first, "Missing Git mailbox envelope");
    }
    MailHeaders headers;
    std::string name;
    std::string value;
    size_t field_line = first;
    size_t line = first + 1;
    for(; line < text.GetLineCount() && !text.GetLine(line).empty(); ++line) {
        const str_view field = text.GetLine(line);
        if(field.front() == ' ' || field.front() == '\t') {
            if(name.empty()) Invalid(text, line, "Mail continuation has no header");
            value += ' ';
            value += TrimSpace(field).to_string();
            continue;
        }
        if(!name.empty()) ReadField(text, field_line, name, value, headers);
        const size_t colon = field.find(':');
        if(colon == SIZE_MAX || colon == 0) Invalid(text, line, "Malformed mail header");
        for(size_t i = 0; i < colon; ++i) {
            if(field[i] < '!' || field[i] > '~') Invalid(text, line, "Malformed mail header name");
        }
        name = LowerAscii(field.substr(0, colon));
        value = TrimSpace(field.substr(colon + 1)).to_string();
        field_line = line;
    }
    if(!name.empty()) ReadField(text, field_line, name, value, headers);
    if(!headers.from || !headers.date || !headers.subject) {
        Invalid(text, first, "Missing From, Date, or Subject mail header");
    }
    if(line == text.GetLineCount()) {
        if(!probe) Invalid(text, line, "Missing blank line after mail headers");
        return line;
    }
    return line + 1;
}

}  // namespace

bool IsMailboxEnvelope(str_view line) {
    if(!line.starts_with("From ")) return false;
    const size_t end = line.find(' ', 5);
    if(end == SIZE_MAX || (end - 5 != 40 && end - 5 != 64) || end + 1 == line.length()) return false;
    for(size_t i = 5; i < end; ++i) {
        const char ch = line[i];
        if(!((ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f') || (ch >= 'A' && ch <= 'F'))) return false;
    }
    return true;
}

void ValidateMailboxPrefix(const TextBuffer& text) {
    ReadMailHeaders(text, true);
}

ParsedPatch ParseMailbox(const TextBuffer& text) {
    const size_t body = ReadMailHeaders(text, false);
    // A later envelope is unsupported even if it would otherwise be hidden in a footer.
    for(size_t line = body; line < text.GetLineCount(); ++line) {
        if(IsMailboxEnvelope(text.GetLine(line))) {
            throw PatchError(ErrorCode::kUnsupportedFeature, "Multiple mailbox messages are unsupported",
                             text.GetLineOffset(line));
        }
    }
    size_t separator = SIZE_MAX;
    size_t line = body;
    for(; line < text.GetLineCount(); ++line) {
        if(IsGitCandidate(text, line)) break;
        if(separator == SIZE_MAX && text.GetLine(line) == "---") separator = line;
    }
    if(line == text.GetLineCount()) Invalid(text, line, "Mail message has no Git patch sections");
    ParsedPatch result{PatchFormat::kGitMailbox, {}};
    result.patch_start = line;
    result.prefix_end = separator == SIZE_MAX ? line : separator + 1;
    for(;;) {
        result.files.push_back(ParseGitSection(text, line));
        result.suffix_begin = line;
        line = SkipBlankLines(text, line);
        if(line == text.GetLineCount() || IsMailSignatureSeparator(text.GetLine(line))) break;
    }
    return result;
}

}  // namespace patch_backend::detail
