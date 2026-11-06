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

#include <patch_backend/patch_archive.hpp>
#include <patch_backend/patch_error.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_predicate.hpp>

using namespace patch_backend;

namespace {

const std::string kPatch =
    "--- old/hello.txt\t2026-11-02 12:00:00 +0100\n"
    "+++ new/hello.txt\t2026-11-02 12:00:01 +0100\n"
    "@@ -1,2 +1,2 @@ greeting\n"
    " Hello\n"
    "-world\n"
    "+Patch WCX\n";

std::string MakePatch(str_view path) {
    std::string patch = "--- ";
    patch.append(path.begin(), path.end());
    patch += "\n+++ ";
    patch.append(path.begin(), path.end());
    patch += "\n@@ -1 +1 @@\n-old\n+new\n";
    return patch;
}

std::string MakeContextPatch(str_view path) {
    std::string patch = "*** ";
    patch.append(path.begin(), path.end());
    patch += "\n--- ";
    patch.append(path.begin(), path.end());
    patch += "\n***************\n*** 1 ****\n! old\n--- 1 ----\n! new\n";
    return patch;
}

// Builds either supported GNU-style text format with independently spelled header fields.
std::string MakeHeaderPatch(bool context, str_view old_field, str_view new_field) {
    return std::string(context ? "*** " : "--- ") + old_field.to_string() + "\n" +
           (context ? "--- " : "+++ ") + new_field.to_string() + "\n" +
           (context ? "***************\n*** 1 ****\n! old\n--- 1 ----\n! new\n" :
                      "@@ -1 +1 @@\n-old\n+new\n");
}

std::string MakeGitPatch(str_view path, bool prefixes = true) {
    const std::string old_path = (prefixes ? "a/" : "") + path.to_string();
    const std::string new_path = (prefixes ? "b/" : "") + path.to_string();
    return "diff --git " + old_path + " " + new_path + "\nindex 1234567..89abcde 100644\n--- " +
           old_path + "\n+++ " + new_path + "\n@@ -1 +1 @@\n-old\n+new\n";
}

std::string MakeGitNotice(str_view path, bool prefixes = true) {
    const std::string old_path = (prefixes ? "a/" : "") + path.to_string();
    const std::string new_path = (prefixes ? "b/" : "") + path.to_string();
    return "diff --git " + old_path + " " + new_path + "\nBinary files " +
           old_path + " and " + new_path + " differ\n";
}

std::string MakeHgPatch(str_view path, bool two_revisions = true) {
    return "diff -r 0123456789ab " + std::string(two_revisions ? "-r abcdef012345 " : "") +
           path.to_string() + "\n--- a/" + path.to_string() + "\n+++ b/" + path.to_string() +
           "\n@@ -1 +1 @@\n-old\n+new\n";
}

std::string MakeHgNotice(str_view path) {
    return "diff -r 0123456789ab -r abcdef012345 " + path.to_string() +
           "\nBinary file " + path.to_string() + " has changed\n";
}

std::string MakeSvnHeader(str_view path) {
    return "Index: " + path.to_string() + "\n" + std::string(67, '=') + "\n";
}

std::string MakeSvnPatch(str_view path) {
    return MakeSvnHeader(path) + MakePatch(path);
}

std::string MakeSvnProperties(str_view path) {
    return MakeSvnHeader(path) + "\nProperty changes on: " + path.to_string() + "\n" +
           std::string(67, '_') + "\nModified: custom\n## -1 +1 ##\n-old\n+new\n";
}

const std::string kGitLiteral =
    "diff --git a/asset.bin b/asset.bin\n"
    "index 82a60be24cca2564698a3dcb697f3a4e6ee59a39..843f4dc3361ee83756fe8121154cd4b9b69b9b5a 100644\n"
    "GIT binary patch\nliteral 5\nMcmZR`OD*RD00ZO!RR910\n\nliteral 5\nMcmZR`&q?6|00Y+nN&o-=\n\n";

// Generated with Git from two 8192-byte blobs differing in a 16-byte region.
const std::string kGitDelta =
    "diff --git a/blob.bin b/blob.bin\n"
    "index 26160dd0db34047da15429b82dc27784321bbd08..6ce1726b3a5b32bd17f23c6a4fe1a98da0ca05df 100644\n"
    "GIT binary patch\ndelta 29\nkcmZp0XmHrDfL|alwOk=7GcU2IQX#3bB(-?+2L1p60H$*a_y7O^\n\n"
    "delta 29\nlcmZp0XmHrDfM1}}+GqZp;5{8bU7xd6*Xq33yn#PJ006z_49frj\n\n";

const std::string kMailHeaders =
    "From 1111111111111111111111111111111111111111 Mon Sep 17 00:00:00 2001\n"
    "From: Example Author <author@example.com>\n"
    "Date: Mon, 2 Nov 2026 12:00:00 +0100\n"
    "Subject: [PATCH] Update files\n\n";
const std::string kMailMessage = "Explain changes to several files.\n\n";
const std::string kMailSummary = "Review-only commentary.\n src/a | 2 +-\n 2 files changed\n\n";
const std::string kMailFooter = "-- \nCustom signature\n";

const std::string kHgHeaders =
    "# HG changeset patch\n"
    "# User Example Author <author@example.com>\n"
    "# Date 1 0\n"
    "# Branch feature\n"
    "# Node ID 1111111111111111111111111111111111111111\n"
    "# Parent  2222222222222222222222222222222222222222\n"
    "# Parent  3333333333333333333333333333333333333333\n"
    "# Unknown metadata retained verbatim\n";
const std::string kHgMessage = "Explain the changes.\n\nMore details, including --- and diffstats.\n\n";

void RequireParseError(str_view bytes, ErrorCode code) {
    REQUIRE_THROWS_MATCHES(
        PatchArchive::Parse(bytes.to_string()), PatchError,
        Catch::Matchers::Predicate<PatchError>(
            [code](const PatchError& error) { return error.GetCode() == code; },
            "has error code " + std::to_string(static_cast<int>(code))));
}

// CTest runs in the repository's build directory; no files go to the system temp folder.
class TestFile {
public:
    explicit TestFile(str_view bytes) {
        std::wstring directory(GetCurrentDirectoryW(0, nullptr), L'\0');
        const DWORD length = GetCurrentDirectoryW(static_cast<DWORD>(directory.size()), directory.data());
        REQUIRE(length != 0);
        directory.resize(length);
        static uint32_t sequence = 0;
        path_ = directory + L"/patch_test_\u017C_" + std::to_wstring(GetCurrentProcessId()) +
                L"_" + std::to_wstring(sequence++) + L".diff";
        const HANDLE file = CreateFileW(path_.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW,
                                        FILE_ATTRIBUTE_NORMAL, nullptr);
        REQUIRE(file != INVALID_HANDLE_VALUE);
        DWORD written = 0;
        const BOOL success = WriteFile(file, bytes.c_str(), static_cast<DWORD>(bytes.size()),
                                        &written, nullptr);
        CloseHandle(file);
        if(!success || written != bytes.size()) {
            DeleteFileW(path_.c_str());
            FAIL("Could not write test fixture");
        }
    }

    ~TestFile() { DeleteFileW(path_.c_str()); }
    TestFile(const TestFile&) = delete;
    TestFile& operator=(const TestFile&) = delete;
    const std::wstring& GetPath() const { return path_; }

    // Reads the disk bytes without parsing, to check exact rewrite and failure behavior.
    std::string ReadBytes() const {
        const HANDLE handle = CreateFileW(path_.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                                          OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        REQUIRE(handle != INVALID_HANDLE_VALUE);
        const UniqueHandle file(handle);
        LARGE_INTEGER size{};
        REQUIRE(GetFileSizeEx(file.get(), &size));
        std::string bytes(static_cast<size_t>(size.QuadPart), '\0');
        if(!bytes.empty()) {
            DWORD read = 0;
            REQUIRE(ReadFile(file.get(), bytes.data(), static_cast<DWORD>(bytes.size()), &read, nullptr));
            REQUIRE(read == bytes.size());
        }
        return bytes;
    }

private:
    std::wstring path_;
};

struct RecordedWriteProgress {
    uint64_t expected_total;
    uint64_t last_written = 0;
    size_t calls = 0;
    bool invalid = false;

    // Records progress without allocating or throwing from the callback.
    static void Report(void* context, uint64_t written, uint64_t total) noexcept {
        auto& state = *static_cast<RecordedWriteProgress*>(context);
        state.invalid = state.invalid || total != state.expected_total || written > total ||
                        written < state.last_written;
        state.last_written = written;
        ++state.calls;
    }
};

}  // namespace

TEST_CASE("A unified patch exposes the destination path and exact document bytes", "[unified]") {
    const auto archive = PatchArchive::Parse(kPatch);
    REQUIRE(archive.GetFormat() == PatchFormat::kUnified);
    REQUIRE_FALSE(archive.HasUtf8Bom());
    REQUIRE_FALSE(archive.GetLastWriteTime().has_value());
    const auto entries = archive.GetEntries();
    REQUIRE(entries.size() == 2);
    REQUIRE(entries[0].path == L"new");
    REQUIRE(entries[0].is_directory);
    REQUIRE(entries[0].size == 0);
    REQUIRE(entries[1].path == L"new/hello.txt");
    REQUIRE_FALSE(entries[1].is_directory);
    REQUIRE(entries[1].size == kPatch.size());
    REQUIRE(archive.ReadEntry(1) == kPatch);
}

TEST_CASE("File order is preserved and directories precede their first descendant", "[entries]") {
    const std::array<std::string, 4> patches = {
        MakePatch("src/lib/b.cpp"), MakePatch("readme.txt"),
        MakePatch("src/a.cpp"), MakePatch("src/lib/c.cpp")};
    const auto archive = PatchArchive::Parse(patches[0] + patches[1] + patches[2] + patches[3]);
    const auto entries = archive.GetEntries();
    REQUIRE(entries.size() == 6);
    REQUIRE(entries[0].path == L"src");
    REQUIRE(entries[1].path == L"src/lib");
    REQUIRE(entries[2].path == L"src/lib/b.cpp");
    REQUIRE(entries[3].path == L"readme.txt");
    REQUIRE(entries[4].path == L"src/a.cpp");
    REQUIRE(entries[5].path == L"src/lib/c.cpp");
    for(size_t i = 0; i < patches.size(); ++i) {
        REQUIRE(archive.ReadEntry(i + 2) == patches[i]);
        REQUIRE(entries[i + 2].size == patches[i].size());
    }
}

TEST_CASE("Line endings, BOMs and a missing final newline survive per-file reads", "[bytes]") {
    for(const std::string ending : {"\n", "\r", "\r\n"}) {
        DYNAMIC_SECTION("Line ending size " << ending.size() << " first byte " << int(ending[0])) {
            for(const bool bom : {false, true}) {
                const std::string first = "--- a" + ending + "+++ a" + ending +
                    "@@ -1 +1 @@" + ending + "-x" + ending + "+y" + ending;
                const std::string second = "--- b" + ending + "+++ b" + ending +
                    "@@ -1 +1 @@" + ending + "-x" + ending + "+z";
                const std::string prefix = bom ? detail::kUtf8Bom : "";
                const auto archive = PatchArchive::Parse(prefix + first + second);
                REQUIRE(archive.HasUtf8Bom() == bom);
                REQUIRE(archive.ReadEntry(0) == prefix + first);
                REQUIRE(archive.ReadEntry(1) == prefix + second);
                REQUIRE(archive.GetEntries()[1].size == prefix.size() + second.size());
            }
        }
    }
    const std::string mixed = "--- a\r\n+++ a\r@@ -1 +1 @@\n-x\r+y";
    REQUIRE(PatchArchive::Parse(mixed).ReadEntry(0) == mixed);
}

TEST_CASE("No-newline markers and source lines resembling headers are preserved", "[unified]") {
    const std::string bytes =
        "--- a\n+++ a\n@@ -1 +1 @@\n--- old\n\\ No newline at end of file\n"
        "+++ new\n\\ No newline at end of file\n";
    const auto archive = PatchArchive::Parse(bytes);
    REQUIRE(archive.GetEntries().size() == 1);
    REQUIRE(archive.ReadEntry(0) == bytes);
}

TEST_CASE("Hunk counts distinguish header-like body lines from file boundaries", "[unified]") {
    const std::string first =
        "--- file.txt\n+++ file.txt\n@@ -1 +1 @@\n--- old-title\n+++ new-title\n"
        "@@ -10 +10 @@\n-before\n+after\n";
    const std::string second = MakePatch("next.txt");
    const auto archive = PatchArchive::Parse(first + second);
    REQUIRE(archive.GetEntries().size() == 2);
    REQUIRE(archive.GetEntries()[0].path == L"file.txt");
    REQUIRE(archive.GetEntries()[1].path == L"next.txt");
    REQUIRE(archive.ReadEntry(0) == first);
    REQUIRE(archive.ReadEntry(1) == second);
}

TEST_CASE("Additions and deletions use zero ranges and the existing side's path", "[unified]") {
    const std::string added = "--- /dev/null\n+++ added.txt\n@@ -0,0 +1,2 @@\n+a\n+b\n";
    const std::string removed = "--- removed.txt\n+++ /dev/null\n@@ -1,2 +0,0 @@\n-a\n-b\n";
    const auto archive = PatchArchive::Parse(added + removed);
    REQUIRE(archive.GetEntries().size() == 2);
    REQUIRE(archive.GetEntries()[0].path == L"added.txt");
    REQUIRE(archive.GetEntries()[1].path == L"removed.txt");
    REQUIRE(archive.ReadEntry(0) == added);
    REQUIRE(archive.ReadEntry(1) == removed);
}

TEST_CASE("Multiple hunks and recursive GNU preambles belong to their file", "[unified]") {
    const std::string first =
        "diff -ruN old/a new/a\n--- old/a\n+++ new/a\n@@ -1 +1 @@\n-a\n+b\n"
        "@@ -9,2 +9,3 @@ function\n c\n-d\n+e\n+f\n";
    const std::string second = "diff -u old/b new/b\n" + MakePatch("b");
    const auto archive = PatchArchive::Parse(first + second);
    REQUIRE(archive.GetEntries().size() == 3);
    REQUIRE(archive.ReadEntry(1) == first);
    REQUIRE(archive.ReadEntry(2) == second);
}

TEST_CASE("Empty and BOM-only patches open but are not recognized by content", "[empty]") {
    for(const std::string bytes : {std::string(), std::string(detail::kUtf8Bom)}) {
        const auto archive = PatchArchive::Parse(bytes);
        REQUIRE(archive.GetEntries().empty());
        REQUIRE(archive.GetFormat() == PatchFormat::kEmpty);
        REQUIRE_FALSE(PatchArchive::Recognize(bytes));
        const TestFile file(bytes);
        REQUIRE(PatchArchive::Open(file.GetPath()).GetEntries().empty());
        REQUIRE_FALSE(PatchArchive::CanOpen(file.GetPath()));
    }
}

TEST_CASE("Recognition is bounded, tentative and based on content", "[probe]") {
    REQUIRE_FALSE(PatchArchive::Recognize(str_view()));
    REQUIRE_FALSE(PatchArchive::Recognize(""));
    REQUIRE(PatchArchive::Recognize(kPatch));
    REQUIRE_FALSE(PatchArchive::Recognize("Ordinary text\n"));
    REQUIRE_FALSE(PatchArchive::Recognize("--- a\n+++ a\n"));
    REQUIRE_FALSE(PatchArchive::Recognize(std::string(32 * 1024, '\n') + kPatch));
    RequireParseError(std::string(32 * 1024, '\n') + kPatch, ErrorCode::kUnknownFormat);
    const std::string truncated = "--- a\n+++ a\n@@ -1,2 +1,2 @@\n x\n";
    REQUIRE(PatchArchive::Recognize(truncated));
    RequireParseError(truncated, ErrorCode::kInvalidPatch);
    // Bytes beyond the probe do not participate in recognition, but opening validates them.
    const std::string late_binary = kPatch + std::string(32 * 1024, '\n') + std::string(1, '\0');
    const TestFile file(late_binary);
    REQUIRE(PatchArchive::CanOpen(file.GetPath()));
    REQUIRE_THROWS_AS(PatchArchive::Open(file.GetPath()), PatchError);
}

TEST_CASE("The 32 KiB probe recognizes structure beyond the former 4 KiB limit", "[probe][io]") {
    for(const std::string patch : {MakePatch("a"), MakeContextPatch("a"), MakeGitPatch("a")}) {
        DYNAMIC_SECTION("Patch " << patch.substr(0, patch.find('\n'))) {
            // Place the complete patch at the end of the probe.
            const std::string bytes = std::string(32 * 1024 - patch.size(), '\n') + patch;
            REQUIRE(PatchArchive::Recognize(bytes));
            REQUIRE(PatchArchive::Parse(bytes).ReadEntry(0) == patch);
            const TestFile file(bytes);
            REQUIRE(PatchArchive::CanOpen(file.GetPath()));
            REQUIRE(PatchArchive::Open(file.GetPath()).ReadEntry(0) == patch);
        }
    }
    // A disallowed control after 4 KiB but within 32 KiB must affect recognition.
    const std::string binary = kPatch + std::string(32 * 1024 - kPatch.size() - 1, '\n') + '\0';
    REQUIRE_FALSE(PatchArchive::Recognize(binary));
    const TestFile file(binary);
    REQUIRE_FALSE(PatchArchive::CanOpen(file.GetPath()));
}

TEST_CASE("Truncated probes discard partial mail fields at CR LF and CRLF boundaries", "[probe][mailbox][io]") {
    const std::string field = "Content-Type: text/plain";
    for(const std::string ending : {"\n", "\r", "\r\n"}) {
        for(const size_t cut : {size_t(10), size_t(20)}) {
            DYNAMIC_SECTION("Ending " << int(ending[0]) << " length " << ending.size() << " cut " << cut) {
                std::string headers;
                for(const char ch : kMailHeaders.substr(0, kMailHeaders.size() - 1)) {
                    headers += ch == '\n' ? ending : std::string(1, ch);
                }
                headers += "X-Padding: ";
                headers += std::string(32 * 1024 - headers.size() - ending.size() - cut, 'x');
                headers += ending;
                // The probe stops in a header name or MIME value, after sufficient complete headers.
                const std::string bytes = headers + field + ending + ending + MakeGitPatch("a");
                REQUIRE(PatchArchive::Recognize(bytes));
                REQUIRE(PatchArchive::Parse(bytes).ReadEntry(0) == bytes);
                const TestFile file(bytes);
                REQUIRE(PatchArchive::CanOpen(file.GetPath()));
                REQUIRE(PatchArchive::Open(file.GetPath()).ReadEntry(0) == bytes);
            }
        }
    }
    // A CR at the limit is a usable ending even when its LF lies outside the probe.
    const std::string headers = kMailHeaders.substr(0, kMailHeaders.size() - 1) + "X-Padding: ";
    const std::string bytes = headers + std::string(32 * 1024 - headers.size() - 1, 'x') +
                              "\r\n\r\n" + MakeGitPatch("a");
    REQUIRE(PatchArchive::Recognize(bytes));
    const TestFile file(bytes);
    REQUIRE(PatchArchive::CanOpen(file.GetPath()));
    REQUIRE(PatchArchive::Open(file.GetPath()).ReadEntry(0) == bytes);
}

TEST_CASE("Probe trimming retains an unterminated line at actual EOF including exactly 32 KiB", "[probe][io]") {
    const std::string patch = "--- a\n+++ a\n@@ -1,0 +1,0 @@";
    for(const size_t size : {patch.size(), size_t(32 * 1024)}) {
        DYNAMIC_SECTION("File size " << size) {
            const std::string bytes = std::string(size - patch.size(), '\n') + patch;
            REQUIRE(PatchArchive::Recognize(bytes));
            REQUIRE(PatchArchive::Parse(bytes).ReadEntry(0) == patch);
            const TestFile file(bytes);
            REQUIRE(PatchArchive::CanOpen(file.GetPath()));
            REQUIRE(PatchArchive::Open(file.GetPath()).ReadEntry(0) == patch);
        }
    }
    REQUIRE_FALSE(PatchArchive::Recognize(std::string(32 * 1024 + 1, 'x')));
}

TEST_CASE("Form-feed page separators in source lines remain text", "[git][bytes][probe]") {
    const std::string patch =
        "diff --git a/entry.S b/entry.S\n--- a/entry.S\n+++ b/entry.S\n"
        "@@ -1,3 +1,3 @@\n-\f\n+/* page */\n \f\n code\n";
    REQUIRE(PatchArchive::Recognize(patch));
    const TestFile file(patch);
    REQUIRE(PatchArchive::CanOpen(file.GetPath()));
    const auto archive = PatchArchive::Open(file.GetPath());
    REQUIRE(archive.GetEntries().size() == 1);
    REQUIRE(archive.GetEntries()[0].path == L"entry.S");
    REQUIRE(archive.ReadEntry(0) == patch);
    REQUIRE(PatchArchive::Parse(archive.ReadEntry(0)).ReadEntry(0) == patch);
}

TEST_CASE("Unusable unified structure fails with structured parsing errors", "[invalid]") {
    const std::array<std::string, 14> invalid = {
        "--- a\n+++ a\n\n",  // Missing hunk header.
        "--- a\n+++ a\n@@ -1",
        "--- a\n+++ a\n@@ -1 +1",
        "--- a\n+++ a\n@@ -1 +1 @",
        "--- a\n+++ a\n@@ -1,2 +1,2 @@\n-a\n+b\n",  // Truncated.
        "--- a\n+++ a\n@@ -1 +1 @@\n-a\n+b\n+extra\n",
        "--- a\n+++ a\n@@ -,1 +1 @@\n-a\n+b\n",  // Missing start digits.
        "--- a\n+++ a\n@@ -1,x +1 @@\n-a\n+b\n",
        "--- a\n+++ a\n@@ -1,18446744073709551616 +1 @@\n-a\n+b\n",  // Count overflow.
        "--- a\n+++ a\n@@ -1 +1, @@\n-a\n+b\n",
        "--- a\n+++ a\n@@ -1 +1,18446744073709551616 @@\n-a\n+b\n",
        "--- a\n+++ a\n@@ -1 +1 @@\n?x\n",
        "--- a\n+++ a\n@@ -1,0 +1 @@\n a\n",  // Would underflow the old count.
        "--- /dev/null\n+++ /dev/null\n@@ -1 +1 @@\n-a\n+b\n"};
    for(size_t i = 0; i < invalid.size(); ++i) {
        DYNAMIC_SECTION("Malformed case " << i) {
            RequireParseError(invalid[i], ErrorCode::kInvalidPatch);
        }
    }
    try {
        PatchArchive::Parse("--- a\n+++ a\n@@ -1,2 +1,2 @@\n-a\n+b\n");
        FAIL("Expected a parsing error");
    } catch(const PatchError& error) {
        REQUIRE(error.GetByteOffset().has_value());
        REQUIRE_FALSE(error.GetIoDetails().has_value());
    }
}

TEST_CASE("Unused hunk details do not prevent listing and extracting file sections", "[unified]") {
    const std::array<std::string, 11> patches = {
        "--- a\n+++ a\n@@ -0 +0 @@\n-a\n+b\n",  // Zero starting line numbers.
        "--- a\n+++ a\n@@ -18446744073709551616 +18446744073709551617 @@\n-a\n+b\n",
        "--- a\n+++ a\n@@ -1 +1 @@description without separator\n-a\n+b\n",
        "--- a\n+++ a\n@@ -1 +1 @@\n a\n",  // Context only.
        "--- a\n+++ a\n@@ -1 +1 @@\n-a\n+b\n@@ -1 +1 @@\n-c\n+d\n",  // Overlap.
        "--- a\n+++ a\n@@ -10 +10 @@\n-a\n+b\n@@ -1 +1 @@\n-c\n+d\n",  // Out of order.
        "--- a\n+++ a\n@@ -1,2 +1 @@\n-a\n\\ No newline at end of file\n-b\n+c\n",
        "--- a\n+++ a\n@@ -1 +1 @@\n-a\n+b\n\\ No newline at end of file\n"
        "\\ No newline at end of file\n",  // Repeated markers.
        "--- /dev/null\n+++ a\n@@ -1 +1 @@\n-a\n+b\n",  // Nonempty missing side.
        "--- a\n+++ /dev/null\n@@ -1 +1 @@\n-a\n+b\n",
        "--- a\n+++ a\n@@ -1,0 +1,0 @@\n"};  // Two empty ranges.
    const std::string next = MakePatch("next.txt");
    for(size_t i = 0; i < patches.size(); ++i) {
        DYNAMIC_SECTION("Tolerated case " << i) {
            const auto& patch = patches[i];
            REQUIRE(PatchArchive::Recognize(patch));
            const auto archive = PatchArchive::Parse(patch + next);
            REQUIRE(archive.GetEntries().size() == 2);
            REQUIRE(archive.GetEntries()[0].path == L"a");
            REQUIRE(archive.GetEntries()[0].size == patch.size());
            REQUIRE(archive.GetEntries()[1].path == L"next.txt");
            REQUIRE(archive.ReadEntry(0) == patch);
            REQUIRE(archive.ReadEntry(1) == next);
            REQUIRE(PatchArchive::Parse(archive.ReadEntry(0)).ReadEntry(0) == patch);
        }
    }
}

TEST_CASE("Unsupported formats and incomplete change notices are rejected", "[scope]") {
    const std::array<std::string, 3> prefixes = {
        "From 123 Mon Sep 17 00:00:00 2001\n",
        "Binary files a and b differ\n", "Only in old: a\n"};
    for(const auto& prefix : prefixes) {
        DYNAMIC_SECTION(prefix) {
            REQUIRE_FALSE(PatchArchive::Recognize(prefix + kPatch));
            RequireParseError(prefix + kPatch, ErrorCode::kUnsupportedFeature);
            RequireParseError(kPatch + prefix, ErrorCode::kUnsupportedFeature);
        }
    }
    RequireParseError("unrelated text\n" + kPatch, ErrorCode::kUnknownFormat);
    RequireParseError(kPatch + "unexpected trailer\n", ErrorCode::kInvalidPatch);
}

TEST_CASE("Binary input and unsupported encodings are rejected", "[encoding]") {
    for(const std::string prefix : {std::string("\xFF\xFE"), std::string("\xFE\xFF"),
                                   std::string("\xFF\xFE\0\0", 4),
                                   std::string("\0\0\xFE\xFF", 4)}) {
        REQUIRE_FALSE(PatchArchive::Recognize(prefix + kPatch));
        RequireParseError(prefix + kPatch, ErrorCode::kUnsupportedFeature);
    }
    RequireParseError(kPatch + std::string(1, '\0'), ErrorCode::kUnknownFormat);
    RequireParseError(detail::kUtf8Bom + MakePatch(std::string("bad_\xFF")), ErrorCode::kInvalidPath);
}

TEST_CASE("Unicode and spaces in paths are supported without altering patch bytes", "[paths]") {
    const std::string bytes = MakePatch("źródła/żółć file.cpp");
    const auto archive = PatchArchive::Parse(bytes);
    REQUIRE(archive.GetEntries()[1].path == L"źródła/żółć file.cpp");
    REQUIRE(archive.ReadEntry(1) == bytes);
    REQUIRE(PatchArchive::Parse(MakePatch("./src/file")).GetEntries()[1].path == L"src/file");
    REQUIRE(PatchArchive::Parse(MakePatch("src\\file")).GetEntries()[1].path == L"src/file");
}

TEST_CASE("Unified and context headers decode C-quoted filenames and optional timestamps",
          "[unified][context][paths][quoted]") {
    const std::array<std::pair<std::string, std::wstring>, 7> names = {{
        {R"("src/my file.cpp")", L"src/my file.cpp"},
        {R"("src/\305\274.cpp")", L"src/ż.cpp"},
        {R"("src\\my file.cpp")", L"src/my file.cpp"},
        {R"("src\057my file.cpp")", L"src/my file.cpp"},
        {R"("src/\156ame.cpp")", L"src/name.cpp"},
        {"src/plain name.cpp", L"src/plain name.cpp"},
        {R"("src/żółć file.cpp")", L"src/żółć file.cpp"}}};
    for(const bool context : {false, true}) {
        for(const auto& [field, expected] : names) {
            for(const std::string old_field : {"old file.cpp", "\"old file.cpp\""}) {
                for(const std::string timestamp : {"", "\t2026-11-05 12:00:00 +0100"}) {
                    DYNAMIC_SECTION(context << " " << field << " " << old_field << timestamp) {
                        // Quoted names followed by tab-separated dates match GNU diff 3.12 output.
                        const std::string bytes = MakeHeaderPatch(context, old_field + timestamp, field + timestamp);
                        REQUIRE(PatchArchive::Recognize(bytes));
                        const auto archive = PatchArchive::Parse(bytes);
                        REQUIRE(archive.GetFormat() == (context ? PatchFormat::kContext : PatchFormat::kUnified));
                        REQUIRE(archive.GetEntries().back().path == expected);
                        REQUIRE(archive.GetEntries().back().size == bytes.size());
                        REQUIRE(archive.ReadEntry(archive.GetEntries().size() - 1) == bytes);
                    }
                }
            }
        }
    }
}

TEST_CASE("Quoted unified and context paths preserve preambles BOM endings and exact extracted bytes",
          "[unified][context][bytes][quoted]") {
    for(const bool context : {false, true}) {
        const std::string patch = std::string(context ? "diff -c" : "diff -u") +
            " \"old file\" \"src/ż.cpp\"\n" +
            MakeHeaderPatch(context, R"("old file")", R"("src/\305\274.cpp")");
        for(const std::string ending : {"\n", "\r\n", "\r"}) {
            for(const bool bom : {false, true}) {
                for(const bool final_ending : {false, true}) {
                    DYNAMIC_SECTION(context << " " << ending.size() << " " << bom << " " << final_ending) {
                        std::string bytes = bom ? detail::kUtf8Bom : "";
                        for(const char ch : patch) {
                            if(ch == '\n') bytes += ending;
                            else bytes += ch;
                        }
                        if(!final_ending) bytes.resize(bytes.size() - ending.size());
                        REQUIRE(PatchArchive::Recognize(bytes));
                        const auto archive = PatchArchive::Parse(bytes);
                        REQUIRE(archive.GetEntries().back().path == L"src/ż.cpp");
                        REQUIRE(archive.GetEntries().back().size == bytes.size());
                        const std::string extracted = archive.ReadEntry(archive.GetEntries().size() - 1);
                        REQUIRE(extracted == bytes);
                        const auto reparsed = PatchArchive::Parse(extracted);
                        REQUIRE(reparsed.GetEntries().back().path == L"src/ż.cpp");
                        REQUIRE(reparsed.ReadEntry(reparsed.GetEntries().size() - 1) == bytes);
                    }
                }
            }
        }
    }
}

TEST_CASE("Unified and context missing sides are selected after unquoting",
          "[unified][context][paths][quoted]") {
    for(const bool context : {false, true}) {
        for(const std::string missing : {"/dev/null", R"("/dev/null")", R"("\057dev/null")"}) {
            DYNAMIC_SECTION(context << " " << missing) {
                const std::string added = context ?
                    "*** " + missing + "\n--- \"added file\"\n***************\n*** 0 ****\n--- 1 ----\n+ new\n" :
                    "--- " + missing + "\n+++ \"added file\"\n@@ -0,0 +1 @@\n+new\n";
                const std::string removed = context ?
                    "*** \"removed file\"\n--- " + missing + "\n***************\n*** 1 ****\n- old\n--- 0 ----\n" :
                    "--- \"removed file\"\n+++ " + missing + "\n@@ -1 +0,0 @@\n-old\n";
                const auto archive = PatchArchive::Parse(added + removed);
                REQUIRE(archive.GetEntries().size() == 2);
                REQUIRE(archive.GetEntries()[0].path == L"added file");
                REQUIRE(archive.GetEntries()[1].path == L"removed file");
                REQUIRE(archive.ReadEntry(0) == added);
                REQUIRE(archive.ReadEntry(1) == removed);
                RequireParseError(MakeHeaderPatch(context, missing, R"("/dev/null")"),
                                  ErrorCode::kInvalidPatch);
            }
        }
    }
}

TEST_CASE("Malformed quoting and unsafe decoded unified and context paths fail before deletion",
          "[unified][context][paths][invalid][delete][quoted]") {
    const std::array<std::string, 9> malformed = {
        "\"unterminated", "\"trailing\\", R"("bad\q")", R"("bad\400")", R"("bad\40")",
        R"("bad\378")", R"("file"suffix)", R"("file" extra)", R"("")"};
    const std::array<std::string, 16> unsafe = {
        R"("\056\056/outside")", R"("\057absolute")", R"("C\072/drive")", R"("\103ON.txt")",
        R"("bad\"name")", R"("bad\000name")", R"("bad\tname")", R"("bad\nname")",
        R"("bad\rname")", R"("bad\aname")", R"("bad\bname")", R"("bad\fname")",
        R"("bad\vname")", R"("trailing\040")", R"("trailing\056")", R"("\134outside")"};
    for(const bool context : {false, true}) {
        for(const auto& field : malformed) {
            for(const bool old_side : {false, true}) {
                DYNAMIC_SECTION(context << " " << field << " " << old_side) {
                    const std::string bad = MakeHeaderPatch(context, old_side ? field : "safe",
                                                            old_side ? "safe" : field);
                    RequireParseError(bad, ErrorCode::kInvalidPatch);
                    const std::string bytes = MakeHeaderPatch(context, "keep", "keep") + bad;
                    const TestFile file(bytes);
                    const std::array<wstr_view, 1> selected = {L"keep"};
                    REQUIRE_THROWS_AS(PatchArchive::DeleteEntries(file.GetPath(), selected), PatchError);
                    REQUIRE(file.ReadBytes() == bytes);
                }
            }
        }
        for(const auto& field : unsafe) {
            DYNAMIC_SECTION(context << " unsafe " << field) {
                RequireParseError(MakeHeaderPatch(context, "safe", field), ErrorCode::kInvalidPath);
            }
        }
        RequireParseError(MakeHeaderPatch(context, "safe", "\"" + std::string(256, 'a') + "\""),
                          ErrorCode::kLimitExceeded);
        RequireParseError(detail::kUtf8Bom + MakeHeaderPatch(context, "safe", R"("bad\377")"),
                          ErrorCode::kInvalidPath);
        RequireParseError(MakeHeaderPatch(context, "safe", R"("src/\141")") +
                          MakeHeaderPatch(context, "safe", "src/a"), ErrorCode::kInvalidPath);
        RequireParseError(MakeHeaderPatch(context, "safe", R"("\123RC/a")") +
                          MakeHeaderPatch(context, "safe", "src/b"), ErrorCode::kInvalidPath);
        RequireParseError(MakeHeaderPatch(context, "safe", R"("\163rc")") +
                          MakeHeaderPatch(context, "safe", "src/a"), ErrorCode::kInvalidPath);
    }
}

TEST_CASE("Quoted unified and context sections delete by decoded file and directory names",
          "[unified][context][delete][quoted]") {
    for(const bool context : {false, true}) {
        DYNAMIC_SECTION(context) {
            const std::string first = MakeHeaderPatch(context, R"("old file")", R"("src/my file.cpp")");
            const std::string keep = MakeHeaderPatch(context, "keep", "keep");
            const std::string last = MakeHeaderPatch(context, "old", R"("src/sub/\305\274.cpp")");
            const TestFile file(detail::kUtf8Bom + first + "\r\n" + keep + "\r\n" + last);
            REQUIRE(PatchArchive::CanOpen(file.GetPath()));
            const std::array<wstr_view, 1> selected = {L"SRC\\MY FILE.CPP"};
            PatchArchive::DeleteEntries(file.GetPath(), selected);
            REQUIRE(file.ReadBytes() == detail::kUtf8Bom + std::string("\r\n") + keep + "\r\n" + last);
            const auto remaining = PatchArchive::Open(file.GetPath());
            REQUIRE(remaining.GetEntries().back().path == L"src/sub/ż.cpp");
            REQUIRE(remaining.ReadEntry(remaining.GetEntries().size() - 1) == detail::kUtf8Bom + last);
            const std::array<wstr_view, 2> overlapping = {L"src", L"src/sub/ż.cpp"};
            PatchArchive::DeleteEntries(file.GetPath(), overlapping);
            REQUIRE(file.ReadBytes() == detail::kUtf8Bom + std::string("\r\n") + keep + "\r\n");
            const auto final = PatchArchive::Open(file.GetPath());
            REQUIRE(final.GetEntries().size() == 1);
            REQUIRE(final.ReadEntry(0) == detail::kUtf8Bom + keep);
        }
    }
}

TEST_CASE("Native SVN and Mercurial authoritative paths remain literal",
          "[svn][hg][paths][quoted]") {
    RequireParseError(MakeSvnHeader(R"("file name")") + MakePatch("ignored"),
                      ErrorCode::kUnsupportedFeature);
    RequireParseError(MakeHgPatch(R"("file name")"), ErrorCode::kUnsupportedFeature);
}

TEST_CASE("BOM-less paths can fall back to the system ANSI code page", "[encoding]") {
    std::array<char, 16> bytes{};
    const int count = WideCharToMultiByte(CP_ACP, 0, L"é", 1, bytes.data(),
                                         static_cast<int>(bytes.size()), nullptr, nullptr);
    REQUIRE(count > 0);
    std::array<wchar_t, 16> expected{};
    const int length = MultiByteToWideChar(CP_ACP, 0, bytes.data(), count, expected.data(),
                                          static_cast<int>(expected.size()));
    REQUIRE(length > 0);
    const auto archive = PatchArchive::Parse(MakePatch(std::string(bytes.data(), count) + ".txt"));
    REQUIRE(archive.GetEntries()[0].path == std::wstring(expected.data(), length) + L".txt");
}

TEST_CASE("Unsafe and ambiguous virtual paths are rejected", "[paths]") {
    const std::array<std::string, 15> paths = {
        "../outside", "src/../../outside", "/absolute", "C:/drive", "src//file",
        "src/./file", "src/trailing.", "src/trailing ", "CON.txt", "dir/LPT1",
        "file:stream", "file*", "dir/", "./", "NUL .txt"};
    for(const auto& path : paths) {
        DYNAMIC_SECTION(path) {
            RequireParseError(MakePatch(path), ErrorCode::kInvalidPath);
        }
    }
    RequireParseError(MakePatch(std::string(256, 'a')), ErrorCode::kLimitExceeded);
}

TEST_CASE("Duplicates, case aliases and file-directory collisions cannot hide entries", "[paths]") {
    const std::array<std::pair<std::string, std::string>, 5> pairs = {{
        {"a", "a"}, {"A", "a"}, {"src", "src/file"},
        {"src/file", "src"}, {"src/a", "SRC/b"}}};
    for(const auto& pair : pairs) {
        DYNAMIC_SECTION(pair.first << " then " << pair.second) {
            RequireParseError(MakePatch(pair.first) + MakePatch(pair.second), ErrorCode::kInvalidPath);
        }
    }
}

TEST_CASE("Moving an archive preserves its byte ranges", "[archive]") {
    auto source = PatchArchive::Parse(MakePatch("a") + MakePatch("b"));
    auto moved = std::move(source);
    REQUIRE(moved.ReadEntry(0) == MakePatch("a"));
    auto assigned = PatchArchive::Parse("");
    assigned = std::move(moved);
    REQUIRE(assigned.ReadEntry(1) == MakePatch("b"));
}

TEST_CASE("WinAPI opening captures metadata and supports Unicode disk paths", "[io]") {
    const TestFile file(kPatch);
    REQUIRE(PatchArchive::CanOpen(file.GetPath()));
    const auto archive = PatchArchive::Open(file.GetPath());
    REQUIRE(archive.ReadEntry(1) == kPatch);
    REQUIRE(archive.GetLastWriteTime().has_value());
    WIN32_FILE_ATTRIBUTE_DATA attributes{};
    REQUIRE(GetFileAttributesExW(file.GetPath().c_str(), GetFileExInfoStandard, &attributes));
    const uint64_t expected = (static_cast<uint64_t>(attributes.ftLastWriteTime.dwHighDateTime) << 32) |
                              attributes.ftLastWriteTime.dwLowDateTime;
    REQUIRE(archive.GetLastWriteTime() == expected);

    const std::wstring padded_path = L"prefix" + file.GetPath() + L".outside";
    const wstr_view path(padded_path, 6, file.GetPath().size());
    REQUIRE(PatchArchive::CanOpen(path));
    REQUIRE(PatchArchive::Open(path).ReadEntry(1) == kPatch);
}

TEST_CASE("WinAPI reads beyond the probe and across multiple I/O chunks", "[io]") {
    const std::string bytes = "--- large\n+++ large\n@@ -1 +1 @@\n-old\n+" +
                              std::string(2 * 1024 * 1024, 'x') + "\n";
    const TestFile file(bytes);
    const auto archive = PatchArchive::Open(file.GetPath());
    REQUIRE(archive.GetEntries().size() == 1);
    REQUIRE(archive.GetEntries()[0].size == bytes.size());
    REQUIRE(archive.ReadEntry(0) == bytes);
}

TEST_CASE("WinAPI failures retain operation, path and system error code", "[io]") {
    std::wstring path;
    {
        const TestFile file(kPatch);
        path = file.GetPath();
    }
    const std::wstring padded_path = path + L".outside";
    try {
        PatchArchive::Open(wstr_view(padded_path, 0, path.size()));
        FAIL("Expected an I/O error");
    } catch(const PatchError& error) {
        REQUIRE(error.GetCode() == ErrorCode::kIoError);
        REQUIRE(error.GetIoDetails().has_value());
        REQUIRE(error.GetIoDetails()->path == path);
        REQUIRE(error.GetIoDetails()->operation == "CreateFileW");
        REQUIRE(error.GetIoDetails()->win32_error == ERROR_FILE_NOT_FOUND);
    }
}

TEST_CASE("RegEngine examples recognize and expose the expected archive contents", "[fixture]") {
    struct Example {
        str_view name;
        PatchFormat format;
        size_t files;
        size_t directories;
    };
    const std::array<Example, 10> examples = {{
        {"regengine_unified.diff", PatchFormat::kUnified, 31, 7},
        {"regengine_context.diff", PatchFormat::kContext, 31, 7},
        {"regengine_git.diff", PatchFormat::kGit, 24, 5},
        {"regengine_mailbox_single.diff", PatchFormat::kGitMailbox, 12, 4},
        {"regengine_svn.diff", PatchFormat::kSvn, 31, 7},
        {"regengine_svn_working_copy.diff", PatchFormat::kSvn, 31, 7},
        {"regengine_svn_git.diff", PatchFormat::kSvn, 31, 7},
        {"regengine_hg.diff", PatchFormat::kHg, 31, 7},
        {"regengine_hg_git.diff", PatchFormat::kGit, 24, 5},
        {"regengine_hg_export.diff", PatchFormat::kHgExport, 24, 5}}};
    for(const auto& example : examples) {
        DYNAMIC_SECTION(example.name.to_string()) {
            const std::wstring path = std::wstring(L"" PATCH_TEST_EXAMPLES_DIR) + L"/RegEngine/" +
                                      std::wstring(example.name.begin(), example.name.end());
            REQUIRE(PatchArchive::CanOpen(path));
            const auto archive = PatchArchive::Open(path);
            REQUIRE(archive.GetFormat() == example.format);
            REQUIRE_FALSE(archive.HasUtf8Bom());
            REQUIRE(archive.GetEntries().size() == example.files + example.directories);
            size_t files = 0;
            size_t directories = 0;
            size_t samples = 0;
            bool has_document = false;
            bool has_deleted_shader = false;
            for(size_t index = 0; index < archive.GetEntries().size(); ++index) {
                const auto& entry = archive.GetEntries()[index];
                if(entry.is_directory) {
                    ++directories;
                    REQUIRE(entry.size == 0);
                    continue;
                }
                ++files;
                REQUIRE(entry.size > 0);
                has_document = has_document || entry.path == L"Doc/Keyboard shortcuts.txt";
                has_deleted_shader = has_deleted_shader || entry.path == L"WorkingDir/Data/ThreeD.hlsl";
                if(entry.path != L"Source/Renderer.cpp" && entry.path != L"WorkingDir/Shaders/GBuffer.hlsl") continue;
                ++samples;
                const std::string bytes = archive.ReadEntry(index);
                REQUIRE(bytes.size() == entry.size);
                REQUIRE(PatchArchive::Recognize(bytes));
                const auto extracted = PatchArchive::Parse(bytes);
                REQUIRE(extracted.GetFormat() == example.format);
                REQUIRE(extracted.GetEntries().back().path == entry.path);
                REQUIRE(extracted.ReadEntry(extracted.GetEntries().size() - 1) == bytes);
                if(example.format == PatchFormat::kGitMailbox) {
                    REQUIRE(bytes.starts_with("From 8c13598b45893b803366dd03ab6adaa30f8cd687 "));
                    REQUIRE(bytes.find("12 files changed") == std::string::npos);
                    REQUIRE(bytes.find("-- \n2.55.0.windows.2") != std::string::npos);
                }
                if(example.format == PatchFormat::kHgExport) {
                    REQUIRE(bytes.starts_with("# HG changeset patch\n"));
                    REQUIRE(bytes.find("# Node ID 0655e600ca6cea86f3d8dd0e25c5f4851a794950") != std::string::npos);
                    REQUIRE(bytes.find("Fixture net changes from Git 138a5680") != std::string::npos);
                }
            }
            REQUIRE(files == example.files);
            REQUIRE(directories == example.directories);
            REQUIRE(samples == 2);
            REQUIRE(has_document == (example.format != PatchFormat::kGitMailbox));
            REQUIRE(has_deleted_shader == (example.format == PatchFormat::kUnified ||
                    example.format == PatchFormat::kContext || example.format == PatchFormat::kSvn ||
                    example.format == PatchFormat::kHg));
        }
    }
}

TEST_CASE("Deleting the first, middle or last file preserves the other patch documents", "[delete]") {
    const std::array<std::string, 3> patches = {
        "diff -u old/a new/a\n" + MakePatch("a"), MakePatch("b"), MakePatch("c")};
    const std::array<wstr_view, 3> paths = {L"a", L"b", L"c"};
    for(size_t removed = 0; removed < patches.size(); ++removed) {
        DYNAMIC_SECTION("Delete file " << removed) {
            const TestFile file(patches[0] + patches[1] + patches[2]);
            PatchArchive::DeleteEntries(file.GetPath(), std::span(paths).subspan(removed, 1));
            std::string expected;
            for(size_t i = 0; i < patches.size(); ++i) {
                if(i != removed) expected += patches[i];
            }
            REQUIRE(file.ReadBytes() == expected);
            const auto archive = PatchArchive::Open(file.GetPath());
            REQUIRE(archive.GetEntries().size() == 2);
            size_t entry = 0;
            for(size_t i = 0; i < patches.size(); ++i) {
                if(i == removed) continue;
                REQUIRE(archive.GetEntries()[entry].path == paths[i].c_str());
                REQUIRE(archive.ReadEntry(entry) == patches[i]);
                ++entry;
            }
        }
    }
}

TEST_CASE("Batch directory deletion combines overlapping selections at path-component boundaries", "[delete]") {
    const std::string sibling = MakePatch("src_old/file.cpp");
    const std::string keep = MakePatch("keep.txt");
    const std::string other = MakePatch("other/x");
    const TestFile file(MakePatch("src/main.cpp") + MakePatch("src/lib/helper.h") +
                        MakePatch("src/lib/deep/a.h") + sibling + keep + other);
    const std::array<wstr_view, 4> paths = {L"src\\", L"SRC/main.cpp", L"src/lib", L"SRC"};
    PatchArchive::DeleteEntries(file.GetPath(), paths);
    REQUIRE(file.ReadBytes() == sibling + keep + other);
    const auto archive = PatchArchive::Open(file.GetPath());
    REQUIRE(archive.GetEntries().size() == 5);
    REQUIRE(archive.GetEntries()[0].path == L"src_old");
    REQUIRE(archive.GetEntries()[1].path == L"src_old/file.cpp");
    REQUIRE(archive.GetEntries()[2].path == L"keep.txt");
    REQUIRE(archive.GetEntries()[3].path == L"other");
    REQUIRE(archive.GetEntries()[4].path == L"other/x");

    const std::array<wstr_view, 2> files = {L"other/x", L"keep.txt"};
    PatchArchive::DeleteEntries(file.GetPath(), files);
    REQUIRE(file.ReadBytes() == sibling);
    REQUIRE(PatchArchive::Open(file.GetPath()).GetEntries().size() == 2);
}

TEST_CASE("Deletion matches Unicode names and Windows separators without changing surviving bytes", "[delete]") {
    const std::string keep = MakePatch("źródła/zostaw.txt");
    const TestFile file(detail::kUtf8Bom + MakePatch("źródła/żółć.txt") + keep);
    const std::array<wstr_view, 1> paths = {L"ŹRÓDŁA\\ŻÓŁĆ.TXT"};
    PatchArchive::DeleteEntries(file.GetPath(), paths);
    REQUIRE(file.ReadBytes() == detail::kUtf8Bom + keep);
    const auto archive = PatchArchive::Open(file.GetPath());
    REQUIRE(archive.GetEntries().size() == 2);
    REQUIRE(archive.GetEntries()[1].path == L"źródła/zostaw.txt");
    REQUIRE(archive.ReadEntry(1) == detail::kUtf8Bom + keep);
}

TEST_CASE("Deletion preserves BOM, separators, mixed line endings and a missing final newline", "[delete]") {
    for(const std::string ending : {"\n", "\r", "\r\n"}) {
        for(const bool bom : {false, true}) {
            DYNAMIC_SECTION("Ending size " << ending.size() << " first byte " << int(ending[0]) << " BOM " << bom) {
                const std::string prefix = bom ? detail::kUtf8Bom : "";
                const std::string first = "--- a" + ending + "+++ a" + ending +
                    "@@ -1 +1 @@" + ending + "-x" + ending + "+y" + ending;
                const std::string middle = MakePatch("b");
                const std::string last = "--- c\r\n+++ c\r@@ -1 +1 @@\n-x\r+y";
                const TestFile file(prefix + "\r\n" + first + "\n" + middle + "\r\n" + last);
                const auto original = PatchArchive::Open(file.GetPath());
                const std::string original_first = original.ReadEntry(0);
                const std::string original_last = original.ReadEntry(2);
                const std::array<wstr_view, 1> paths = {L"b"};
                PatchArchive::DeleteEntries(file.GetPath(), paths);
                REQUIRE(file.ReadBytes() == prefix + "\r\n" + first + "\n\r\n" + last);
                const auto archive = PatchArchive::Open(file.GetPath());
                REQUIRE(archive.GetEntries().size() == 2);
                REQUIRE(archive.ReadEntry(0) == original_first);
                REQUIRE(archive.ReadEntry(1) == original_last);
            }
        }
    }
}

TEST_CASE("Deleting every file produces an explicitly reopenable empty patch", "[delete]") {
    for(const bool bom : {false, true}) {
        DYNAMIC_SECTION("BOM " << bom) {
            const std::string prefix = bom ? detail::kUtf8Bom : "";
            const TestFile file(prefix + "\r\n" + MakePatch("src/a") + "\n\n" + MakePatch("src/b") + "\r\n");
            const std::array<wstr_view, 1> paths = {L"src"};
            RecordedWriteProgress progress{prefix.size()};
            PatchArchive::DeleteEntries(file.GetPath(), paths, {RecordedWriteProgress::Report, &progress});
            REQUIRE(file.ReadBytes() == prefix);
            const auto empty = PatchArchive::Open(file.GetPath());
            REQUIRE(empty.GetEntries().empty());
            REQUIRE(empty.GetFormat() == PatchFormat::kEmpty);
            REQUIRE(empty.HasUtf8Bom() == bom);
            REQUIRE_FALSE(PatchArchive::CanOpen(file.GetPath()));
            REQUIRE(progress.calls > 0);
            REQUIRE_FALSE(progress.invalid);
            REQUIRE(progress.last_written == prefix.size());
        }
    }
}

TEST_CASE("Invalid batch selections leave the original patch unchanged", "[delete]") {
    const std::string original = MakePatch("a") + MakePatch("b");
    const std::array<std::wstring, 7> invalid = {
        L"missing", L"", L"../a", L"/a", L"a*", L"/", std::wstring(L"a\0junk", 6)};
    for(size_t i = 0; i < invalid.size(); ++i) {
        DYNAMIC_SECTION("Invalid deletion selection " << i) {
            const TestFile file(original);
            const std::array<wstr_view, 2> paths = {L"a", invalid[i]};
            REQUIRE_THROWS_MATCHES(PatchArchive::DeleteEntries(file.GetPath(), paths), PatchError,
                Catch::Matchers::Predicate<PatchError>(
                    [](const PatchError& error) { return error.GetCode() == ErrorCode::kInvalidPath; }));
            REQUIRE(file.ReadBytes() == original);
        }
    }
}

TEST_CASE("Empty deletion selections perform no I/O", "[delete]") {
    const TestFile file(kPatch);
    const auto before = PatchArchive::Open(file.GetPath()).GetLastWriteTime();
    PatchArchive::DeleteEntries(file.GetPath(), {});
    REQUIRE(file.ReadBytes() == kPatch);
    REQUIRE(PatchArchive::Open(file.GetPath()).GetLastWriteTime() == before);
    REQUIRE_NOTHROW(PatchArchive::DeleteEntries(file.GetPath() + L".missing", {}));
}

TEST_CASE("Deletion parse and sharing failures do not modify the source", "[delete]") {
    const std::string truncated = "--- a\n+++ a\n@@ -1,2 +1,2 @@\n-x\n+y\n";
    const TestFile broken(truncated);
    const std::array<wstr_view, 1> paths = {L"a"};
    REQUIRE_THROWS_MATCHES(PatchArchive::DeleteEntries(broken.GetPath(), paths), PatchError,
        Catch::Matchers::Predicate<PatchError>(
            [](const PatchError& error) { return error.GetCode() == ErrorCode::kInvalidPatch; }));
    REQUIRE(broken.ReadBytes() == truncated);

    const TestFile locked(MakePatch("a") + MakePatch("b"));
    const HANDLE handle = CreateFileW(locked.GetPath().c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                                      OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    REQUIRE(handle != INVALID_HANDLE_VALUE);
    UniqueHandle reader(handle);
    REQUIRE_THROWS_MATCHES(PatchArchive::DeleteEntries(locked.GetPath(), paths), PatchError,
        Catch::Matchers::Predicate<PatchError>([](const PatchError& error) {
            return error.GetCode() == ErrorCode::kIoError && error.GetIoDetails() &&
                   error.GetIoDetails()->operation == "CreateFileW" &&
                   error.GetIoDetails()->win32_error == ERROR_SHARING_VIOLATION;
        }));
    REQUIRE(locked.ReadBytes() == MakePatch("a") + MakePatch("b"));
    reader.reset();
    PatchArchive::DeleteEntries(locked.GetPath(), paths);
    REQUIRE(locked.ReadBytes() == MakePatch("b"));
}

TEST_CASE("Deletion rewrites large surviving sections with monotonic byte progress", "[delete]") {
    const std::string keep = "--- keep\n+++ keep\n@@ -1 +1 @@\n-old\n+" +
                             std::string(3 * 1024 * 1024, 'x') + "\n";
    const TestFile file(MakePatch("remove") + keep);
    const std::array<wstr_view, 1> paths = {L"remove"};
    RecordedWriteProgress progress{keep.size()};
    PatchArchive::DeleteEntries(file.GetPath(), paths, {RecordedWriteProgress::Report, &progress});
    REQUIRE(file.ReadBytes() == keep);
    REQUIRE(progress.calls >= 4);
    REQUIRE_FALSE(progress.invalid);
    REQUIRE(progress.last_written == keep.size());
    const auto archive = PatchArchive::Open(file.GetPath());
    REQUIRE(archive.GetEntries().size() == 1);
    REQUIRE(archive.GetEntries()[0].size == keep.size());
    REQUIRE(archive.ReadEntry(0) == keep);
}

TEST_CASE("Context preambles, destination paths and file order survive extraction", "[context]") {
    const std::array<std::string, 5> preambles = {
        "diff -rcN old new\n", "diff -C 5 old new\n", "diff -C5 old new\n",
        "diff --context old new\n", "diff --context=5 old new\n"};
    const std::string first =
        "*** old/file.txt\t2026-11-02 12:00:00 +0100\n"
        "--- new/file.txt\t2026-11-02 12:00:01 +0100\n"
        "***************\n*** 1,2 ****\n  same\n! old\n--- 1,2 ----\n  same\n! new\n";
    const std::string next = MakeContextPatch("new/next.txt");
    for(const auto& preamble : preambles) {
        DYNAMIC_SECTION(preamble) {
            const auto archive = PatchArchive::Parse(preamble + first + next);
            REQUIRE(archive.GetFormat() == PatchFormat::kContext);
            REQUIRE(archive.GetEntries().size() == 3);
            REQUIRE(archive.GetEntries()[0].path == L"new");
            REQUIRE(archive.GetEntries()[0].is_directory);
            REQUIRE(archive.GetEntries()[1].path == L"new/file.txt");
            REQUIRE(archive.GetEntries()[2].path == L"new/next.txt");
            REQUIRE(archive.ReadEntry(1) == preamble + first);
            REQUIRE(archive.GetEntries()[1].size == preamble.size() + first.size());
            REQUIRE(archive.ReadEntry(2) == next);
        }
    }
}

TEST_CASE("Context bodies support inclusive ranges, omitted halves and empty ranges", "[context]") {
    const std::array<std::string, 9> hunks = {
        "***************\n*** 1,2 ****\n  same\n! old\n--- 1,2 ----\n  same\n! new\n",
        "***************\n*** 9,11 ****\n--- 8,13 ----\n  a\n  b\n  c\n+ x\n+ y\n+ z\n",
        "***************\n*** 8,13 ****\n  a\n  b\n  c\n- x\n- y\n- z\n--- 9,11 ----\n",
        "***************\n*** 0 ****\n--- 1 ----\n+ new\n",
        "***************\n*** 1 ****\n- old\n--- 0 ----\n",
        "***************\n*** 5 ****\n--- 6 ----\n+ new\n",
        "***************\n*** 6 ****\n- old\n--- 5 ----\n",
        "***************\n*** 1 ****\n  same\n--- 1 ----\n  same\n",
        "***************\n*** 0 ****\n--- 0 ----\n"};
    const std::string next = MakeContextPatch("b");
    for(size_t i = 0; i < hunks.size(); ++i) {
        DYNAMIC_SECTION("Context body form " << i) {
            const std::string first = "*** a\n--- a\n" + hunks[i];
            REQUIRE(PatchArchive::Recognize(first));
            const auto archive = PatchArchive::Parse(first + next);
            REQUIRE(archive.GetEntries().size() == 2);
            REQUIRE(archive.ReadEntry(0) == first);
            REQUIRE(archive.ReadEntry(1) == next);
            REQUIRE(PatchArchive::Parse(archive.ReadEntry(0)).ReadEntry(0) == first);
        }
    }
}

TEST_CASE("Context body text resembling headers and separators stays in its file", "[context]") {
    const std::string first =
        "*** a\n--- a\n***************\n*** 1,3 ****\n"
        "- *** old\n! --- 1 ----\n  ***************\n"
        "--- 1,3 ----\n+ *** new\n! --- 1 ----\n  ***************\n"
        "***************\n*** 10 ****\n! before\n--- 10 ----\n! after\n";
    const std::string next = MakeContextPatch("b");
    const auto archive = PatchArchive::Parse(first + next);
    REQUIRE(archive.GetEntries().size() == 2);
    REQUIRE(archive.ReadEntry(0) == first);
    REQUIRE(archive.ReadEntry(1) == next);
}

TEST_CASE("Context patches preserve BOM, line endings and no-newline markers", "[context]") {
    const std::array<std::string, 3> endings = {"\n", "\r", "\r\n"};
    for(size_t i = 0; i < endings.size(); ++i) {
        for(const bool bom : {false, true}) {
            DYNAMIC_SECTION("Context ending " << i << " BOM " << bom) {
                const auto& ending = endings[i];
                const std::string prefix = bom ? detail::kUtf8Bom : "";
                const std::string patch = "*** a" + ending + "--- a" + ending +
                    "***************" + ending + "*** 1 ****" + ending + "! old" + ending +
                    "\\ No newline at end of file" + ending + "--- 1 ----" + ending + "! new" + ending +
                    "\\ No newline at end of file";
                const TestFile file(prefix + patch);
                const auto archive = PatchArchive::Open(file.GetPath());
                REQUIRE(archive.HasUtf8Bom() == bom);
                REQUIRE(archive.GetEntries()[0].size == prefix.size() + patch.size());
                REQUIRE(archive.ReadEntry(0) == prefix + patch);
                REQUIRE(PatchArchive::Parse(archive.ReadEntry(0)).ReadEntry(0) == prefix + patch);
            }
        }
    }
    const std::string mixed = "*** a\r\n--- a\r***************\n*** 1 ****\r! old\n--- 1 ----\r\n! new";
    REQUIRE(PatchArchive::Parse(mixed).ReadEntry(0) == mixed);
}

TEST_CASE("Unused context hunk details are tolerated without changing extracted bytes", "[context]") {
    const std::string patch =
        "*** a\n--- a\n***************description\n*** 10 ****suffix\n! old\n"
        "\\ No newline at end of file\n\\ No newline at end of file\n--- 10 ----suffix\n! new\n"
        "***************\n*** 1 ****\n! previous\n--- 1 ----\n! next\n"
        "***************\n*** 1 ****\n! overlapping\n--- 1 ----\n! again\n";
    const auto archive = PatchArchive::Parse(patch);
    REQUIRE(archive.ReadEntry(0) == patch);
    const std::string large_positions =
        "*** a\n--- a\n***************\n*** 18446744073709551614,18446744073709551615 ****\n"
        "! a\n! b\n--- 18446744073709551614,18446744073709551615 ----\n! c\n! d\n";
    REQUIRE(PatchArchive::Parse(large_positions).ReadEntry(0) == large_positions);
}

TEST_CASE("Unusable context structure fails with parsing offsets", "[context][invalid]") {
    const std::array<std::string, 14> tails = {
        "", "***************\n", "***************\n*** 1 ****\n",
        "***************\n*** x ****\n--- 1 ----\n! x\n",
        "***************\n*** 1 ***\n--- 1 ----\n! x\n",
        "***************\n*** 2,1 ****\n--- 1 ----\n! x\n",
        "***************\n*** 18446744073709551616 ****\n--- 1 ----\n! x\n",
        "***************\n*** 0,18446744073709551615 ****\n--- 1 ----\n! x\n",
        "***************\n*** 1,2 ****\n! old\n--- 1 ----\n! new\n",
        "***************\n*** 1 ****\n! old\n--- 1,2 ----\n! new\n",
        "***************\n*** 1 ****\n! old\n--- 1 ----\n! new\n+ extra\n",
        "***************\n*** 1 ****\n? old\n--- 1 ----\n! new\n",
        "***************\n*** 1 ****\n! old\n--- 1 ---\n! new\n",
        "***************\n*** 1 ****\n! old\n--- 1 ----\n? new\n"};
    for(size_t i = 0; i < tails.size(); ++i) {
        DYNAMIC_SECTION("Unusable context case " << i) {
            const std::string patch = "*** a\n--- a\n" + tails[i];
            REQUIRE_THROWS_MATCHES(PatchArchive::Parse(patch), PatchError,
                Catch::Matchers::Predicate<PatchError>([](const PatchError& error) {
                    return error.GetCode() == ErrorCode::kInvalidPatch && error.GetByteOffset().has_value();
                }));
        }
    }
    RequireParseError("*** a\n", ErrorCode::kInvalidPatch);
}

TEST_CASE("Context recognition is bounded and does not require the complete first body", "[context][probe]") {
    const std::string patch = "*** a\n--- a\n***************\n*** 1 ****\n! " +
                              std::string(40 * 1024, 'x') + "\n--- 1 ----\n! new\n";
    REQUIRE(PatchArchive::Recognize(patch));
    const TestFile file(patch);
    REQUIRE(PatchArchive::CanOpen(file.GetPath()));
    REQUIRE(PatchArchive::Open(file.GetPath()).ReadEntry(0) == patch);
    const std::string truncated = "*** a\n--- a\n***************\n*** 1,2 ****\n! old\n";
    REQUIRE(PatchArchive::Recognize(truncated));
    RequireParseError(truncated, ErrorCode::kInvalidPatch);
    REQUIRE_FALSE(PatchArchive::Recognize("*** a\n--- a\n***************\n"));
    REQUIRE_FALSE(PatchArchive::Recognize(std::string(32 * 1024, '\n') + patch));
    const TestFile binary(patch + std::string(1, '\0'));
    REQUIRE(PatchArchive::CanOpen(binary.GetPath()));
    REQUIRE_THROWS_AS(PatchArchive::Open(binary.GetPath()), PatchError);
}

TEST_CASE("Context archives reuse path decoding, missing-side selection and collision checks", "[context][paths]") {
    const std::string unicode = MakeContextPatch("źródła/żółć file.cpp");
    const auto archive = PatchArchive::Parse(detail::kUtf8Bom + unicode);
    REQUIRE(archive.GetEntries()[1].path == L"źródła/żółć file.cpp");
    REQUIRE(archive.ReadEntry(1) == detail::kUtf8Bom + unicode);
    const std::string added = "*** /dev/null\n--- added\n***************\n*** 0 ****\n--- 1 ----\n+ new\n";
    const std::string removed = "*** removed\n--- /dev/null\n***************\n*** 1 ****\n- old\n--- 0 ----\n";
    const auto missing = PatchArchive::Parse(added + removed);
    REQUIRE(missing.GetEntries()[0].path == L"added");
    REQUIRE(missing.GetEntries()[1].path == L"removed");
    REQUIRE(missing.ReadEntry(0) == added);
    REQUIRE(missing.ReadEntry(1) == removed);
    REQUIRE(PatchArchive::Parse(MakeContextPatch("./src\\a")).GetEntries()[1].path == L"src/a");
    RequireParseError(MakeContextPatch("../outside"), ErrorCode::kInvalidPath);
    RequireParseError(MakeContextPatch("A") + MakeContextPatch("a"), ErrorCode::kInvalidPath);
    RequireParseError(MakeContextPatch("src") + MakeContextPatch("src/a"), ErrorCode::kInvalidPath);
    RequireParseError(detail::kUtf8Bom + MakeContextPatch(std::string("bad_\xFF")), ErrorCode::kInvalidPath);
    RequireParseError("*** /dev/null\n--- /dev/null\n***************\n*** 0 ****\n--- 0 ----\n",
                      ErrorCode::kInvalidPatch);
}

TEST_CASE("Mixed patch formats and unsupported context wrappers are rejected", "[context][scope]") {
    const std::string context = MakeContextPatch("context");
    RequireParseError(context + kPatch, ErrorCode::kInvalidPatch);
    RequireParseError(kPatch + context, ErrorCode::kInvalidPatch);
    RequireParseError("diff -u old new\n" + context, ErrorCode::kInvalidPatch);
    RequireParseError("diff -c old new\n" + kPatch, ErrorCode::kInvalidPatch);
    for(const std::string prefix : {"Only in old: a\n",
                                      "Binary files a and b differ\n"}) {
        REQUIRE_FALSE(PatchArchive::Recognize(prefix + context));
        RequireParseError(prefix + context, ErrorCode::kUnsupportedFeature);
        RequireParseError(context + prefix, ErrorCode::kUnsupportedFeature);
    }
}

TEST_CASE("Deleting context sections preserves the format, preambles and surviving bytes", "[context][delete]") {
    const std::array<std::string, 3> patches = {
        "diff -rcN old new\n" + MakeContextPatch("a"), MakeContextPatch("b"), MakeContextPatch("c")};
    const std::array<wstr_view, 3> paths = {L"a", L"b", L"c"};
    for(size_t removed = 0; removed < paths.size(); ++removed) {
        DYNAMIC_SECTION("Delete context entry " << removed) {
            const TestFile file(detail::kUtf8Bom + patches[0] + patches[1] + patches[2]);
            PatchArchive::DeleteEntries(file.GetPath(), std::span(paths).subspan(removed, 1));
            std::string expected = detail::kUtf8Bom;
            for(size_t i = 0; i < paths.size(); ++i) {
                if(i != removed) expected += patches[i];
            }
            REQUIRE(file.ReadBytes() == expected);
            const auto archive = PatchArchive::Open(file.GetPath());
            REQUIRE(archive.GetFormat() == PatchFormat::kContext);
            REQUIRE(archive.GetEntries().size() == 2);
            size_t entry = 0;
            for(size_t i = 0; i < paths.size(); ++i) {
                if(i == removed) continue;
                REQUIRE(archive.ReadEntry(entry) == detail::kUtf8Bom + patches[i]);
                const auto extracted = PatchArchive::Parse(archive.ReadEntry(entry));
                REQUIRE(extracted.GetFormat() == PatchFormat::kContext);
                REQUIRE(extracted.ReadEntry(0) == archive.ReadEntry(entry));
                ++entry;
            }
        }
    }
}

TEST_CASE("Context directory deletion handles overlaps and deleting all remaining entries", "[context][delete]") {
    for(const bool bom : {false, true}) {
        DYNAMIC_SECTION("Delete context tree with BOM " << bom) {
            const std::string prefix = bom ? detail::kUtf8Bom : "";
            const std::string sibling = MakeContextPatch("src_old/żółć.txt");
            const TestFile file(prefix + MakeContextPatch("src/a") + MakeContextPatch("src/lib/b") + sibling);
            const std::array<wstr_view, 3> paths = {L"SRC", L"src/a", L"src\\lib"};
            PatchArchive::DeleteEntries(file.GetPath(), paths);
            REQUIRE(file.ReadBytes() == prefix + sibling);
            const auto archive = PatchArchive::Open(file.GetPath());
            REQUIRE(archive.GetFormat() == PatchFormat::kContext);
            REQUIRE(archive.GetEntries().size() == 2);
            REQUIRE(archive.GetEntries()[1].path == L"src_old/żółć.txt");
            const std::array<wstr_view, 1> last = {L"src_old/ŻÓŁĆ.TXT"};
            PatchArchive::DeleteEntries(file.GetPath(), last);
            REQUIRE(file.ReadBytes() == prefix);
            REQUIRE(PatchArchive::Open(file.GetPath()).GetFormat() == PatchFormat::kEmpty);
            REQUIRE_FALSE(PatchArchive::CanOpen(file.GetPath()));
        }
    }
}

TEST_CASE("Failed context deletion leaves the source bytes unchanged", "[context][delete]") {
    const std::string patch = MakeContextPatch("a") + MakeContextPatch("b");
    const TestFile file(patch);
    const std::array<wstr_view, 2> paths = {L"a", L"missing"};
    REQUIRE_THROWS_AS(PatchArchive::DeleteEntries(file.GetPath(), paths), PatchError);
    REQUIRE(file.ReadBytes() == patch);
    const std::string truncated = "*** a\n--- a\n***************\n*** 1 ****\n! old\n--- 1,2 ----\n! new\n";
    const TestFile broken(truncated);
    const std::array<wstr_view, 1> remove = {L"a"};
    REQUIRE_THROWS_AS(PatchArchive::DeleteEntries(broken.GetPath(), remove), PatchError);
    REQUIRE(broken.ReadBytes() == truncated);
}

TEST_CASE("Git text sections strip display prefixes and preserve complete documents", "[git]") {
    const std::string first = MakeGitPatch("src/a.cpp");
    const std::string second = MakeGitPatch("readme.txt");
    const auto archive = PatchArchive::Parse(first + second);
    REQUIRE(archive.GetFormat() == PatchFormat::kGit);
    REQUIRE(archive.GetEntries().size() == 3);
    REQUIRE(archive.GetEntries()[0].path == L"src");
    REQUIRE(archive.GetEntries()[1].path == L"src/a.cpp");
    REQUIRE(archive.GetEntries()[2].path == L"readme.txt");
    REQUIRE(archive.GetEntries()[1].size == first.size());
    REQUIRE(archive.ReadEntry(1) == first);
    REQUIRE(archive.ReadEntry(2) == second);
    REQUIRE(PatchArchive::Parse(archive.ReadEntry(1)).ReadEntry(1) == first);
}

TEST_CASE("Git commit preambles survive per-file extraction and partial deletion", "[git][preamble][delete]") {
    const std::string first = MakeGitPatch("a.txt");
    const std::string second = "diff --git a/old.txt b/new.txt\nsimilarity index 100%\n"
        "rename from old.txt\nrename to new.txt\n";
    for(const size_t width : {size_t{40}, size_t{64}}) {
        for(const bool parent : {false, true}) {
            for(const bool bom : {false, true}) {
                DYNAMIC_SECTION("ID width " << width << " parent " << parent << " BOM " << bom) {
                    const std::string mark = bom ? detail::kUtf8Bom : "";
                    const std::string prefix = mark + "\r\n" + std::string(width, 'a') +
                        (parent ? " (from " + std::string(width, 'B') + ")" : "") + "\r\n\r\n";
                    const std::string bytes = prefix + first + second;
                    REQUIRE(PatchArchive::Recognize(bytes));
                    const auto archive = PatchArchive::Parse(bytes);
                    REQUIRE(archive.GetFormat() == PatchFormat::kGit);
                    REQUIRE(archive.GetEntries().size() == 2);
                    REQUIRE(archive.GetEntries()[0].path == L"a.txt");
                    REQUIRE(archive.GetEntries()[1].path == L"new.txt");
                    REQUIRE(archive.ReadEntry(0) == prefix + first);
                    REQUIRE(archive.ReadEntry(1) == prefix + second);
                    REQUIRE(archive.GetEntries()[0].size == prefix.size() + first.size());
                    REQUIRE(archive.GetEntries()[1].size == prefix.size() + second.size());
                    REQUIRE(PatchArchive::Parse(archive.ReadEntry(1)).ReadEntry(0) == prefix + second);
                    const TestFile file(bytes);
                    REQUIRE(PatchArchive::CanOpen(file.GetPath()));
                    {
                        const auto disk = PatchArchive::Open(file.GetPath());
                        REQUIRE(disk.ReadEntry(0) == prefix + first);
                    }
                    const std::array<wstr_view, 1> remove_first = {L"a.txt"};
                    PatchArchive::DeleteEntries(file.GetPath(), remove_first);
                    REQUIRE(file.ReadBytes() == prefix + second);
                    REQUIRE(PatchArchive::Open(file.GetPath()).ReadEntry(0) == prefix + second);
                    const std::array<wstr_view, 1> remove_last = {L"new.txt"};
                    PatchArchive::DeleteEntries(file.GetPath(), remove_last);
                    REQUIRE(file.ReadBytes() == mark);
                }
            }
        }
    }
}

TEST_CASE("Git commit preambles require a patch and do not admit arbitrary text or commit series", "[git][preamble][invalid]") {
    const std::string id(40, 'a');
    const std::string prefix = id + " (from " + std::string(40, 'b') + ")\n";
    REQUIRE_FALSE(PatchArchive::Recognize(prefix));
    RequireParseError(prefix, ErrorCode::kInvalidPatch);
    RequireParseError(prefix + MakePatch("a"), ErrorCode::kInvalidPatch);
    RequireParseError(prefix + prefix + MakeGitPatch("a"), ErrorCode::kInvalidPatch);
    RequireParseError(prefix + MakeGitPatch("a") + prefix + MakeGitPatch("b"), ErrorCode::kInvalidPatch);
    for(const std::string invalid : {
            std::string(39, 'a') + "\n", std::string(40, 'g') + "\n",
            id + " (from " + std::string(64, 'b') + ")\n",
            id + " (from " + std::string(40, 'b') + "\n", id + " extra text\n",
            std::string("arbitrary commentary\n")}) {
        DYNAMIC_SECTION(invalid) {
            REQUIRE_FALSE(PatchArchive::Recognize(invalid + MakeGitPatch("a")));
            RequireParseError(invalid + MakeGitPatch("a"), ErrorCode::kUnknownFormat);
        }
    }
}

TEST_CASE("Git metadata-only changes expose rename, copy, mode and empty-file operations", "[git]") {
    const std::array<std::string, 5> patches = {
        "diff --git a/old.txt b/new.txt\nsimilarity index 100%\nrename from old.txt\nrename to new.txt\n",
        "diff --git a/source.txt b/copy.txt\nsimilarity index 100%\ncopy from source.txt\ncopy to copy.txt\n",
        "diff --git a/script.sh b/script.sh\nold mode 100644\nnew mode 100755\n",
        "diff --git a/empty.txt b/empty.txt\nnew file mode 100644\nindex 0000000..e69de29\n",
        "diff --git a/deleted.txt b/deleted.txt\ndeleted file mode 100644\nindex e69de29..0000000\n"};
    const std::array<wstr_view, 5> paths = {L"new.txt", L"copy.txt", L"script.sh", L"empty.txt", L"deleted.txt"};
    std::string bytes;
    for(const auto& patch : patches) bytes += patch;
    const auto archive = PatchArchive::Parse(bytes);
    REQUIRE(archive.GetEntries().size() == patches.size());
    for(size_t i = 0; i < patches.size(); ++i) {
        CAPTURE(i);
        REQUIRE(PatchArchive::Recognize(patches[i]));
        REQUIRE(archive.GetEntries()[i].path == paths[i].c_str());
        REQUIRE(archive.GetEntries()[i].size == patches[i].size());
        REQUIRE(archive.ReadEntry(i) == patches[i]);
        REQUIRE(PatchArchive::Parse(archive.ReadEntry(i)).ReadEntry(0) == patches[i]);
    }
}

TEST_CASE("Git rename and copy paths preserve genuine a directories and unquoted spaces", "[git][paths]") {
    for(const std::string kind : {"rename", "copy"}) {
        DYNAMIC_SECTION(kind) {
            const std::string patch = "diff --git a/a/old b/part.txt b/a/new b/part.txt\n" +
                kind + " from a/old b/part.txt\n" + kind + " to a/new b/part.txt\n"
                "--- a/a/old b/part.txt\n+++ b/a/new b/part.txt\n@@ -1 +1 @@\n-old\n+new\n";
            const auto archive = PatchArchive::Parse(patch);
            REQUIRE(archive.GetEntries().back().path == L"a/new b/part.txt");
            REQUIRE(archive.ReadEntry(archive.GetEntries().size() - 1) == patch);
            const std::string bare = "diff --git a/old b/part.txt a/new b/part.txt\n" +
                kind + " from a/old b/part.txt\n" + kind + " to a/new b/part.txt\n";
            REQUIRE(PatchArchive::Parse(bare).GetEntries().back().path == L"a/new b/part.txt");
        }
    }
}

TEST_CASE("Git prefix handling distinguishes default names from unprefixed real components", "[git][paths]") {
    for(const bool prefixes : {false, true}) {
        for(const std::string path : {"src/a.txt", "a/a file.txt", "b/b file.txt"}) {
            DYNAMIC_SECTION("Prefixes " << prefixes << " path " << path) {
                const std::string patch = MakeGitPatch(path, prefixes);
                const auto archive = PatchArchive::Parse(patch);
                REQUIRE(archive.ReadEntry(archive.GetEntries().size() - 1) == patch);
                REQUIRE(archive.GetEntries().back().path ==
                        std::wstring(path.begin(), path.end()));
            }
        }
    }
    const std::string bare_mode = "diff --git a/real file a/real file\nold mode 100644\nnew mode 100755\n";
    REQUIRE(PatchArchive::Parse(bare_mode).GetEntries().back().path == L"a/real file");
    const std::string custom = "diff --git old/a new/a\n--- old/a\n+++ new/a\n@@ -1 +1 @@\n-x\n+y\n";
    RequireParseError(custom, ErrorCode::kUnsupportedFeature);
    const std::string anchored = "diff --git old/a new/b\nrename from a\nrename to b\n"
        "--- old/a\n+++ new/b\n@@ -1 +1 @@\n-x\n+y\n";
    REQUIRE(PatchArchive::Parse(anchored).GetEntries()[0].path == L"b");
}

TEST_CASE("Git metadata-only renames and copies resolve custom display prefixes", "[git][paths][delete]") {
    for(const std::string kind : {"rename", "copy"}) {
        for(const int style : {0, 1, 2}) {
            DYNAMIC_SECTION(kind << " header style " << style) {
                const std::string old_path = style == 0 ? "onefile" : "real/old name";
                const std::string new_path = style == 0 ? "otherfile" : "real/new name";
                const std::string names = style == 0 ? "a/sub/onefile b/sub/otherfile" :
                    style == 1 ? "old root/real/old name new root/real/new name" :
                                 "\"old root/real/old name\" \"new root/real/new name\"";
                const std::string patch = "diff --git " + names + "\nsimilarity index 100%\n" +
                    kind + " from " + old_path + "\n" + kind + " to " + new_path + "\n";
                REQUIRE(PatchArchive::Recognize(patch));
                const auto archive = PatchArchive::Parse(patch);
                const size_t index = archive.GetEntries().size() - 1;
                const std::wstring expected(new_path.begin(), new_path.end());
                REQUIRE(archive.GetEntries()[index].path == expected);
                REQUIRE(archive.GetEntries()[index].size == patch.size());
                REQUIRE(archive.ReadEntry(index) == patch);
                REQUIRE(PatchArchive::Parse(archive.ReadEntry(index)).GetEntries().back().path == expected);
                const TestFile file(patch + MakeGitPatch("keep"));
                REQUIRE(PatchArchive::CanOpen(file.GetPath()));
                const std::array<wstr_view, 1> selected = {expected.c_str()};
                PatchArchive::DeleteEntries(file.GetPath(), selected);
                REQUIRE(file.ReadBytes() == MakeGitPatch("keep"));
            }
        }
    }
    const std::string metadata = "\nrename from onefile\nrename to otherfile\n";
    RequireParseError("diff --git a/sub/different b/sub/otherfile" + metadata, ErrorCode::kUnsupportedFeature);
    RequireParseError("diff --git a/sub/onefile b/sub/different" + metadata, ErrorCode::kUnsupportedFeature);
    RequireParseError("diff --git a/sub/old b/sub/../outside\nrename from old\nrename to ../outside\n",
                      ErrorCode::kInvalidPath);
}

TEST_CASE("Git quoted paths decode octal UTF-8 bytes and explicit quote boundaries", "[git][paths]") {
    const std::string name = "\\305\\274\\303\\263\\305\\202\\304\\207.txt";
    const std::string patch = "diff --git \"a/" + name + "\" \"b/" + name + "\"\n"
        "--- \"a/" + name + "\"\n+++ \"b/" + name + "\"\n@@ -1 +1 @@\n-x\n+y\n";
    const auto archive = PatchArchive::Parse(detail::kUtf8Bom + patch);
    REQUIRE(archive.GetEntries()[0].path == L"żółć.txt");
    REQUIRE(archive.ReadEntry(0) == detail::kUtf8Bom + patch);
    for(const std::string header : {"\"a/a file\" b/a file", "a/a file \"b/a file\""}) {
        const auto mode = PatchArchive::Parse("diff --git " + header + "\nold mode 100644\nnew mode 100755\n");
        REQUIRE(mode.GetEntries()[0].path == L"a file");
    }
    const std::string rename = "diff --git \"a/a/" + name + "\" b/a/new.txt\n"
        "rename from \"a/" + name + "\"\nrename to a/new.txt\n";
    REQUIRE(PatchArchive::Parse(rename).GetEntries().back().path == L"a/new.txt");
}

TEST_CASE("Git missing-side headers and mode-only symlink changes remain patch documents", "[git]") {
    const std::string added = "diff --git a/new b/new\nnew file mode 100644\n--- /dev/null\n+++ b/new\n"
        "@@ -0,0 +1 @@\n+text\n";
    const std::string removed = "diff --git a/old b/old\ndeleted file mode 100644\n--- a/old\n+++ /dev/null\n"
        "@@ -1 +0,0 @@\n-text\n";
    const auto archive = PatchArchive::Parse(added + removed);
    REQUIRE(archive.GetEntries()[0].path == L"new");
    REQUIRE(archive.GetEntries()[1].path == L"old");
    REQUIRE(archive.ReadEntry(0) == added);
    REQUIRE(archive.ReadEntry(1) == removed);
    const std::string link = "diff --git a/link b/link\nold mode 100644\nnew mode 120000\n";
    REQUIRE(PatchArchive::Parse(link).ReadEntry(0) == link);
    const std::string submodule = "diff --git a/sub b/sub\nindex 123..456 160000\n--- a/sub\n+++ b/sub\n"
        "@@ -1 +1 @@\n-Subproject commit 123\n+Subproject commit 456\n";
    REQUIRE(PatchArchive::Parse(submodule).ReadEntry(0) == submodule);
}

TEST_CASE("Git literal and delta records preserve both directions and their terminators", "[git][binary]") {
    const std::array<std::string, 2> patches = {kGitLiteral, kGitDelta};
    for(size_t i = 0; i < patches.size(); ++i) {
        DYNAMIC_SECTION("Binary method " << i) {
            const auto& patch = patches[i];
            const std::string keep = MakeGitPatch("keep");
            const TestFile file(patch + keep);
            REQUIRE(PatchArchive::CanOpen(file.GetPath()));
            const auto archive = PatchArchive::Open(file.GetPath());
            REQUIRE(archive.GetEntries().size() == 2);
            REQUIRE(archive.GetEntries()[0].size == patch.size());
            REQUIRE(archive.ReadEntry(0) == patch);
            REQUIRE(archive.ReadEntry(1) == keep);
            REQUIRE(PatchArchive::Parse(archive.ReadEntry(0)).ReadEntry(0) == patch);
            const std::array<wstr_view, 1> remove = {i == 0 ? L"asset.bin" : L"blob.bin"};
            PatchArchive::DeleteEntries(file.GetPath(), remove);
            REQUIRE(file.ReadBytes() == keep);
        }
    }
    const std::string forward_only = "diff --git a/asset.bin b/asset.bin\nGIT binary patch\n"
        "literal 5\nMcmZR`OD*RD00ZO!RR910\n\n";
    REQUIRE(PatchArchive::Parse(forward_only).ReadEntry(0) == forward_only);
}

TEST_CASE("Git binary validation checks framing without interpreting compression or sizes", "[git][binary]") {
    const std::string prefix = "diff --git a/blob b/blob\nGIT binary patch\n";
    const std::array<std::string, 7> invalid = {
        "", "literal 5\n\n", "literal 5\nA00000\n",
        "literal 5\n000000\n\n", "literal 5\nz00000\n\n", "unknown 5\nA00000\n\n",
        "literal x\nA00000\n\n"};
    for(size_t i = 0; i < invalid.size(); ++i) {
        DYNAMIC_SECTION("Binary framing failure " << i) {
            RequireParseError(prefix + invalid[i], ErrorCode::kInvalidPatch);
        }
    }
    const std::string opaque = prefix + "delta 184467440737095516160\nA00000\n\n";
    REQUIRE(PatchArchive::Parse(opaque).ReadEntry(0) == opaque);
    const std::string unchecked = prefix + "literal 5\nA0000:\n\nliteral 5\nA0000:\n\n";
    REQUIRE(PatchArchive::Parse(unchecked).ReadEntry(0) == unchecked);
}

TEST_CASE("Git paths reject unusable escapes, Windows names and conflicting path fields", "[git][paths]") {
    for(const std::string name : {"../outside", "C:/drive", "CON.txt"}) {
        RequireParseError(MakeGitPatch(name), ErrorCode::kInvalidPath);
    }
    for(const std::string escape : {"\\t", "\\n", "\\r", "\\000", "\\\\", "\\\""}) {
        const std::string field = "bad" + escape + "name";
        const std::string patch = "diff --git \"a/" + field + "\" \"b/" + field + "\"\n"
            "old mode 100644\nnew mode 100755\n";
        RequireParseError(patch, ErrorCode::kInvalidPath);
    }
    for(const std::string field : {"\"a/bad\\q\"", "\"a/bad\\400\"", "\"a/unterminated"}) {
        RequireParseError("diff --git " + field + " b/a\nold mode 100644\nnew mode 100755\n",
                          ErrorCode::kInvalidPatch);
    }
    RequireParseError("diff --git a/a b/a\n--- a/other\n+++ b/a\n@@ -1 +1 @@\n-x\n+y\n",
                      ErrorCode::kInvalidPatch);
    RequireParseError("diff --git a/a b/b\nrename from a\n", ErrorCode::kInvalidPatch);
    RequireParseError("diff --git a/a b/b\nrename from \"\"\nrename to b\n", ErrorCode::kInvalidPatch);
    RequireParseError(MakeGitPatch("A") + MakeGitPatch("a"), ErrorCode::kInvalidPath);
    RequireParseError("diff --git a/src b/src\ndeleted file mode 100644\n" + MakeGitPatch("src/a"),
                      ErrorCode::kInvalidPath);
}

TEST_CASE("Git recognition accepts metadata-only probes while full opening checks sections", "[git][probe]") {
    const std::string mode = "diff --git a/a b/a\nold mode 100644\nnew mode 100755\n";
    REQUIRE(PatchArchive::Recognize(mode));
    REQUIRE_FALSE(PatchArchive::Recognize("diff --git a/a b/a\n"));
    const std::string index_only = "diff --git a/a b/a\nindex 123..456 100644\n";
    REQUIRE(PatchArchive::Recognize(index_only));
    RequireParseError(index_only, ErrorCode::kInvalidPatch);
    const std::string large = "diff --git a/a b/a\n--- a/a\n+++ b/a\n@@ -1 +1 @@\n-old\n+" +
                              std::string(40 * 1024, 'x') + "\n";
    REQUIRE(PatchArchive::Recognize(large));
    REQUIRE(PatchArchive::Parse(large).ReadEntry(0) == large);
    REQUIRE_FALSE(PatchArchive::Recognize(std::string(32 * 1024, '\n') + mode));
    const TestFile binary(large + std::string(1, '\0'));
    REQUIRE(PatchArchive::CanOpen(binary.GetPath()));
    REQUIRE_THROWS_AS(PatchArchive::Open(binary.GetPath()), PatchError);
}

TEST_CASE("Git archives reject combined diffs, nested submodule output and email envelopes", "[git][scope]") {
    const std::string git = MakeGitPatch("a");
    for(const std::string prefix : {"diff --cc a\n", "diff --combined a\n",
                                      "From 123 Mon Sep 17 00:00:00 2001\n"}) {
        RequireParseError(prefix + git, ErrorCode::kUnsupportedFeature);
        RequireParseError(git + prefix, ErrorCode::kUnsupportedFeature);
    }
    RequireParseError("diff --git a/sub b/sub\nindex 123..456 160000\nSubmodule sub 123..456:\n",
                      ErrorCode::kUnsupportedFeature);
    RequireParseError(git + MakeContextPatch("b"), ErrorCode::kInvalidPatch);
    RequireParseError(MakeContextPatch("b") + git, ErrorCode::kUnsupportedFeature);
    RequireParseError(kPatch + git, ErrorCode::kUnsupportedFeature);
}

TEST_CASE("Git mixed section deletion handles directories, overlaps and empty results", "[git][delete]") {
    for(const bool bom : {false, true}) {
        DYNAMIC_SECTION("Git deletion BOM " << bom) {
            const std::string prefix = bom ? detail::kUtf8Bom : "";
            const std::string mode = "diff --git a/src/mode b/src/mode\nold mode 100644\nnew mode 100755\n";
            const std::string keep = MakeGitPatch("src_old/keep");
            const TestFile file(prefix + MakeGitPatch("src/a") + mode + kGitDelta + keep);
            const std::array<wstr_view, 3> remove = {L"src", L"SRC/mode", L"blob.bin"};
            PatchArchive::DeleteEntries(file.GetPath(), remove);
            REQUIRE(file.ReadBytes() == prefix + keep);
            REQUIRE(PatchArchive::Open(file.GetPath()).GetEntries().back().path == L"src_old/keep");
            const std::array<wstr_view, 1> last = {L"src_old"};
            PatchArchive::DeleteEntries(file.GetPath(), last);
            REQUIRE(file.ReadBytes() == prefix);
            REQUIRE(PatchArchive::Open(file.GetPath()).GetEntries().empty());
            REQUIRE_FALSE(PatchArchive::CanOpen(file.GetPath()));
        }
    }
}

TEST_CASE("Failed Git deletions preserve the original patch bytes", "[git][delete]") {
    const std::string patch = MakeGitPatch("a") + kGitLiteral;
    const TestFile file(patch);
    const std::array<wstr_view, 2> paths = {L"a", L"missing"};
    REQUIRE_THROWS_AS(PatchArchive::DeleteEntries(file.GetPath(), paths), PatchError);
    REQUIRE(file.ReadBytes() == patch);
    const std::string broken = "diff --git a/a b/a\n--- a/a\n+++ b/a\n@@ -1,2 +1 @@\n-old\n+new\n";
    const TestFile truncated(broken);
    const std::array<wstr_view, 1> remove = {L"a"};
    REQUIRE_THROWS_AS(PatchArchive::DeleteEntries(truncated.GetPath(), remove), PatchError);
    REQUIRE(truncated.ReadBytes() == broken);
}

TEST_CASE("Git text and binary sections preserve BOM and original line endings", "[git][bytes]") {
    const std::array<std::string, 3> endings = {"\n", "\r", "\r\n"};
    for(size_t i = 0; i < endings.size(); ++i) {
        for(const bool bom : {false, true}) {
            DYNAMIC_SECTION("Git ending " << i << " BOM " << bom) {
                const std::string prefix = bom ? detail::kUtf8Bom : "";
                std::string binary;
                std::string text;
                for(const char ch : kGitLiteral) {
                    if(ch == '\n') binary += endings[i];
                    else binary += ch;
                }
                for(const char ch : MakeGitPatch("a")) {
                    if(ch == '\n') text += endings[i];
                    else text += ch;
                }
                text.resize(text.size() - endings[i].size());
                const auto archive = PatchArchive::Parse(prefix + binary + text);
                REQUIRE(archive.GetEntries().size() == 2);
                REQUIRE(archive.ReadEntry(0) == prefix + binary);
                REQUIRE(archive.ReadEntry(1) == prefix + text);
                REQUIRE(PatchArchive::Parse(archive.ReadEntry(0)).ReadEntry(0) == prefix + binary);
                REQUIRE(PatchArchive::Parse(archive.ReadEntry(1)).ReadEntry(0) == prefix + text);
            }
        }
    }
}

TEST_CASE("Git text traversal preserves header-like source and tolerates unused details", "[git]") {
    const std::string patch = "diff --git a/a b/a\nfuture-extension opaque value\nindex unused..unused\n"
        "--- a/a\n+++ b/a\n@@ -184467440737095516160 +0 @@description\n--- old\n+++ new\n"
        "\\ No newline at end of file\n\\ No newline at end of file\n@@ -1 +1 @@\n same\n";
    const auto archive = PatchArchive::Parse(patch + MakeGitPatch("b"));
    REQUIRE(archive.GetEntries().size() == 2);
    REQUIRE(archive.ReadEntry(0) == patch);
    REQUIRE(archive.ReadEntry(1) == MakeGitPatch("b"));
}

TEST_CASE("Mailbox extraction keeps the message and footer but omits review summaries", "[mailbox]") {
    const std::string prefix = kMailHeaders + kMailMessage + "---\n";
    const std::string first = MakeGitPatch("src/a");
    const std::string second = MakeGitPatch("b");
    const std::string bytes = prefix + kMailSummary + first + second + kMailFooter;
    REQUIRE(PatchArchive::Recognize(bytes));
    const TestFile file(bytes);
    REQUIRE(PatchArchive::CanOpen(file.GetPath()));
    const auto archive = PatchArchive::Open(file.GetPath());
    REQUIRE(archive.GetFormat() == PatchFormat::kGitMailbox);
    REQUIRE(archive.GetEntries().size() == 3);
    REQUIRE(archive.GetEntries()[0].path == L"src");
    REQUIRE(archive.GetEntries()[0].is_directory);
    REQUIRE(archive.GetEntries()[1].path == L"src/a");
    REQUIRE(archive.GetEntries()[2].path == L"b");
    for(size_t i = 1; i < 3; ++i) {
        const std::string expected = prefix + (i == 1 ? first : second) + kMailFooter;
        REQUIRE(archive.ReadEntry(i) == expected);
        REQUIRE(archive.GetEntries()[i].size == expected.size());
        const auto extracted = PatchArchive::Parse(expected);
        REQUIRE(extracted.GetFormat() == PatchFormat::kGitMailbox);
        REQUIRE(extracted.ReadEntry(extracted.GetEntries().size() - 1) == expected);
    }
}

TEST_CASE("Mailbox extraction handles absent separators, messages and signatures", "[mailbox][bytes]") {
    for(const bool separator : {false, true}) {
        for(const bool signature : {false, true}) {
            DYNAMIC_SECTION("Separator " << separator << " signature " << signature) {
                const std::string prefix = kMailHeaders + (separator ? kMailMessage + "---\n" : "");
                const std::string footer = signature ? "\n" + kMailFooter : "";
                std::string patch = MakeGitPatch("a");
                if(!signature) patch.pop_back();
                const std::string bytes = prefix + patch + footer;
                const auto archive = PatchArchive::Parse(bytes);
                REQUIRE(archive.ReadEntry(0) == bytes);
                REQUIRE(archive.GetEntries()[0].size == bytes.size());
            }
        }
    }
    const std::string preamble = kMailHeaders + "Keep this whole message.\n src/a | 2 +-\n\n";
    REQUIRE(PatchArchive::Parse(preamble + MakeGitPatch("a")).ReadEntry(0) == preamble + MakeGitPatch("a"));
}

TEST_CASE("Mailbox headers recognize long messages before any patch is visible", "[mailbox][probe]") {
    const std::string headers = kMailHeaders.substr(0, kMailHeaders.size() - 1);
    REQUIRE(PatchArchive::Recognize(headers));
    REQUIRE(PatchArchive::Recognize(kMailHeaders));
    RequireParseError(headers, ErrorCode::kInvalidPatch);
    RequireParseError(kMailHeaders, ErrorCode::kInvalidPatch);
    const std::string message = kMailHeaders + std::string(40 * 1024, 'x') + "\n\n";
    const std::string patch = MakeGitPatch("a");
    const TestFile file(message + patch);
    REQUIRE(PatchArchive::CanOpen(file.GetPath()));
    REQUIRE(PatchArchive::Open(file.GetPath()).ReadEntry(0) == message + patch);
    REQUIRE(PatchArchive::Parse(message + patch).ReadEntry(0) == message + patch);
    REQUIRE(PatchArchive::Recognize(message));
    RequireParseError(message, ErrorCode::kInvalidPatch);
}

TEST_CASE("Mailbox headers support folding, case variation and unencoded MIME controls", "[mailbox][headers]") {
    const std::string headers =
        "From " + std::string(64, 'a') + " unused timestamp\n"
        "fRoM: Author\n\t<author@example.com>\n"
        "dAtE: unused date\nSuBjEcT: [PATCH]\n continued subject\n"
        "MIME-Version: 1.0\nContent-Type:\n Text/Plain; charset=UTF-8\n"
        "Content-Transfer-Encoding: 8BIT\nContent-Disposition: inline\n"
        "X-Future: ignored\n\n";
    const std::string bytes = headers + MakeGitPatch("a");
    REQUIRE(PatchArchive::Recognize(bytes));
    REQUIRE(PatchArchive::Parse(bytes).ReadEntry(0) == bytes);
    for(const std::string encoding : {"7bit", "8bit", "binary"}) {
        const std::string raw = kMailHeaders.substr(0, kMailHeaders.size() - 1) +
            "Content-Transfer-Encoding: " + encoding + "\n\n" + MakeGitPatch("a");
        REQUIRE(PatchArchive::Parse(raw).ReadEntry(0) == raw);
    }
    for(const std::string field : {"Content-Type: multipart/mixed; boundary=abc\n",
                                  "Content-Type: text/html\n", "Content-Type: message/rfc822\n",
                                  "Content-Transfer-Encoding: base64\n",
                                  "Content-Transfer-Encoding:\n quoted-printable\n",
                                  "Content-Disposition: attachment; filename=a.patch\n"}) {
        DYNAMIC_SECTION(field) {
            const std::string encoded = kMailHeaders.substr(0, kMailHeaders.size() - 1) + field + "\n" + MakeGitPatch("a");
            REQUIRE_FALSE(PatchArchive::Recognize(encoded));
            RequireParseError(encoded, ErrorCode::kUnsupportedFeature);
        }
    }
}

TEST_CASE("Mailbox parsing rejects missing or unusable header structure", "[mailbox][invalid]") {
    const std::string envelope = kMailHeaders.substr(0, kMailHeaders.find('\n') + 1);
    for(const std::string fields : {"From: Author\nDate: unused\n\n",
                                   "From: Author\nSubject: title\n\n",
                                   "Date: unused\nSubject: title\n\n",
                                   " continuation without a field\n\n",
                                   "From Author\nDate: unused\nSubject: title\n\n",
                                   "Bad Name: value\n\n"}) {
        DYNAMIC_SECTION(fields) {
            REQUIRE_FALSE(PatchArchive::Recognize(envelope + fields));
            RequireParseError(envelope + fields + MakeGitPatch("a"), ErrorCode::kInvalidPatch);
        }
    }
}

TEST_CASE("Mailbox preambles skip quoted commands and preserve opaque text", "[mailbox]") {
    const std::string message = kMailHeaders +
        "Example command:\ndiff --git a/example b/example\nThis is explanatory text.\n\n";
    const std::string prefix = message + "---\n";
    const std::string summary = "Unknown summary syntax is deliberately ignored.\n\n";
    const std::string patch = MakeGitPatch("a");
    const std::string footer = "-- \nArbitrary footer\ndiff --git a/quoted b/quoted\nindex unused\n";
    const auto archive = PatchArchive::Parse(prefix + summary + patch + footer);
    REQUIRE(archive.GetEntries().size() == 1);
    REQUIRE(archive.ReadEntry(0) == prefix + patch + footer);
}

TEST_CASE("Mailbox parsing reuses Git binary and metadata-only sections", "[mailbox][binary]") {
    const std::array<std::string, 4> patches = {kGitLiteral, kGitDelta,
        "diff --git a/empty b/empty\nnew file mode 100644\nindex unused..unused\n",
        "diff --git a/old b/new\nsimilarity index 100%\nrename from old\nrename to new\n"};
    for(const auto& patch : patches) {
        DYNAMIC_SECTION(patch.substr(0, patch.find('\n'))) {
            const std::string bytes = kMailHeaders + patch + kMailFooter;
            const auto archive = PatchArchive::Parse(bytes);
            REQUIRE(archive.GetEntries().size() == 1);
            REQUIRE(archive.ReadEntry(0) == bytes);
            REQUIRE(archive.GetEntries()[0].size == bytes.size());
            REQUIRE(PatchArchive::Parse(archive.ReadEntry(0)).ReadEntry(0) == bytes);
        }
    }
}

TEST_CASE("Mailbox footers tolerate a missing separator space only at section boundaries", "[mailbox][bytes][delete]") {
    const std::array<std::string, 3> patches = {
        "diff --git a/a b/a\n--- a/a\n+++ b/a\n@@ -1,2 +1 @@\n--\n-old\n+new\n",
        kGitLiteral, "diff --git a/empty b/empty\nnew file mode 100644\n"};
    const std::array<wstr_view, 3> paths = {L"a", L"asset.bin", L"empty"};
    const std::string footer = "--\n2.38.1\n";
    for(size_t i = 0; i < patches.size(); ++i) {
        DYNAMIC_SECTION("Section kind " << i) {
            const std::string bytes = kMailHeaders + MakeGitPatch("keep") + patches[i] + footer;
            const std::string expected = kMailHeaders + patches[i] + footer;
            REQUIRE(PatchArchive::Recognize(bytes));
            const auto archive = PatchArchive::Parse(bytes);
            REQUIRE(archive.GetFormat() == PatchFormat::kGitMailbox);
            REQUIRE(archive.GetEntries().size() == 2);
            REQUIRE(archive.GetEntries()[1].path == paths[i].c_str());
            REQUIRE(archive.GetEntries()[1].size == expected.size());
            REQUIRE(archive.ReadEntry(1) == expected);
            REQUIRE(PatchArchive::Parse(expected).ReadEntry(0) == expected);
            const TestFile file(bytes);
            REQUIRE(PatchArchive::CanOpen(file.GetPath()));
            REQUIRE(PatchArchive::Open(file.GetPath()).ReadEntry(1) == expected);
            const std::array<wstr_view, 1> remove_first = {L"keep"};
            PatchArchive::DeleteEntries(file.GetPath(), remove_first);
            REQUIRE(file.ReadBytes() == expected);
            const std::array<wstr_view, 1> remove_last = {paths[i]};
            PatchArchive::DeleteEntries(file.GetPath(), remove_last);
            REQUIRE(file.ReadBytes().empty());
        }
    }
    const std::string truncated_text = "diff --git a/a b/a\n--- a/a\n+++ b/a\n@@ -1 +1 @@\n-old\n";
    RequireParseError(kMailHeaders + truncated_text + footer, ErrorCode::kInvalidPatch);
    RequireParseError(kMailHeaders + kGitLiteral.substr(0, kGitLiteral.size() - 1) + footer,
                      ErrorCode::kInvalidPatch);
    RequireParseError(kMailHeaders + MakeGitPatch("a") + footer + kMailHeaders + MakeGitPatch("b"),
                      ErrorCode::kUnsupportedFeature);
    RequireParseError(MakeGitPatch("a") + footer, ErrorCode::kInvalidPatch);
}

TEST_CASE("Mailbox parsing preserves BOM and original line endings", "[mailbox][bytes]") {
    for(const std::string ending : {"\n", "\r\n", "\r"}) {
        for(const bool bom : {false, true}) {
            DYNAMIC_SECTION("Ending " << ending.size() << " byte " << int(ending[0]) << " BOM " << bom) {
                const std::string original = kMailHeaders + kMailMessage + "---\n" + MakeGitPatch("a") +
                                             "\n-- \nSignature without final newline";
                std::string bytes = bom ? detail::kUtf8Bom : "";
                for(const char ch : original) bytes += ch == '\n' ? ending : std::string(1, ch);
                const auto archive = PatchArchive::Parse(bytes);
                REQUIRE(archive.ReadEntry(0) == bytes);
                REQUIRE(archive.GetEntries()[0].size == bytes.size());
                auto original_archive = PatchArchive::Parse(bytes);
                auto moved = std::move(original_archive);
                REQUIRE(moved.ReadEntry(0) == bytes);
            }
        }
    }
}

TEST_CASE("Mailbox opening rejects further messages including after signatures", "[mailbox][scope]") {
    const std::string first = kMailHeaders + MakeGitPatch("a");
    for(const std::string gap : {std::string(), kMailFooter, std::string("\n")}) {
        DYNAMIC_SECTION("Gap " << gap.size()) {
            const std::string series = first + gap + kMailHeaders + MakeGitPatch("b");
            REQUIRE(PatchArchive::Recognize(series));
            RequireParseError(series, ErrorCode::kUnsupportedFeature);
            const TestFile file(series);
            const std::array<wstr_view, 1> selected = {L"a"};
            REQUIRE_THROWS_AS(PatchArchive::DeleteEntries(file.GetPath(), selected), PatchError);
            REQUIRE(file.ReadBytes() == series);
        }
    }
    RequireParseError(kMailHeaders + MakePatch("a"), ErrorCode::kInvalidPatch);
    RequireParseError(first + MakeContextPatch("b"), ErrorCode::kInvalidPatch);
    RequireParseError(kMailHeaders + "diff --git a/a b/a\nindex x..y\n", ErrorCode::kInvalidPatch);
}

TEST_CASE("Mailbox paths and text hunks retain existing Git validation", "[mailbox][invalid]") {
    RequireParseError(kMailHeaders + MakeGitPatch("../outside"), ErrorCode::kInvalidPath);
    RequireParseError(kMailHeaders + MakeGitPatch("A") + MakeGitPatch("a"), ErrorCode::kInvalidPath);
    RequireParseError(kMailHeaders + "diff --git a/a b/a\n--- a/a\n+++ b/a\n@@ -2 +2 @@\n",
                      ErrorCode::kInvalidPatch);
    RequireParseError(kMailHeaders + MakeGitPatch("a") + "Unexpected trailing text\n", ErrorCode::kInvalidPatch);
}

TEST_CASE("Mailbox batch deletion drops summaries and preserves retained message bytes", "[mailbox][delete]") {
    for(const bool bom : {false, true}) {
        DYNAMIC_SECTION("BOM " << bom) {
            const std::string prefix = (bom ? detail::kUtf8Bom : "") + kMailHeaders + kMailMessage + "---\n";
            const std::string first = MakeGitPatch("src/a");
            const std::string second = MakeGitPatch("src/sub/b");
            const std::string keep = MakeGitPatch("keep");
            const TestFile file(prefix + kMailSummary + first + second + kGitLiteral + keep + kMailFooter);
            const auto original = PatchArchive::Open(file.GetPath());
            const std::array<wstr_view, 3> selected = {L"SRC\\", L"src/sub/b", L"asset.bin"};
            const std::string expected = prefix + keep + kMailFooter;
            RecordedWriteProgress progress{expected.size()};
            PatchArchive::DeleteEntries(file.GetPath(), selected, {RecordedWriteProgress::Report, &progress});
            REQUIRE(file.ReadBytes() == expected);
            REQUIRE_FALSE(progress.invalid);
            REQUIRE(progress.last_written == expected.size());
            REQUIRE(original.ReadEntry(1) == prefix + first + kMailFooter);
            const auto remaining = PatchArchive::Open(file.GetPath());
            REQUIRE(remaining.GetEntries().size() == 1);
            REQUIRE(remaining.ReadEntry(0) == expected);
            const std::array<wstr_view, 1> last = {L"keep"};
            PatchArchive::DeleteEntries(file.GetPath(), last);
            REQUIRE(file.ReadBytes() == (bom ? detail::kUtf8Bom : ""));
            REQUIRE(PatchArchive::Open(file.GetPath()).GetFormat() == PatchFormat::kEmpty);
        }
    }
}

TEST_CASE("Mailbox deletion preserves messages without separators and fails before writing", "[mailbox][delete]") {
    const std::string prefix = kMailHeaders + kMailMessage;
    const std::string bytes = prefix + MakeGitPatch("a") + MakeGitPatch("b");
    const TestFile file(bytes);
    const std::array<wstr_view, 1> missing = {L"missing"};
    REQUIRE_THROWS_AS(PatchArchive::DeleteEntries(file.GetPath(), missing), PatchError);
    REQUIRE(file.ReadBytes() == bytes);
    PatchArchive::DeleteEntries(file.GetPath(), {});
    REQUIRE(file.ReadBytes() == bytes);
    const std::array<wstr_view, 1> selected = {L"a"};
    PatchArchive::DeleteEntries(file.GetPath(), selected);
    REQUIRE(file.ReadBytes() == prefix + MakeGitPatch("b"));
    REQUIRE(PatchArchive::Open(file.GetPath()).ReadEntry(0) == prefix + MakeGitPatch("b"));
}

TEST_CASE("Mailbox deletion retains the footer when removing first, middle or last sections", "[mailbox][delete]") {
    std::string headers;
    for(const char ch : kMailHeaders) headers += ch == '\n' ? "\r\n" : std::string(1, ch);
    const std::string prefix = detail::kUtf8Bom + headers + "Keep the original message.\r\n---\r\n";
    const std::array<std::string, 3> patches = {MakeGitPatch("a"), kGitLiteral,
        "diff --git a/empty b/empty\nnew file mode 100644\n"};
    const std::string footer = "\r\n-- \rCustom signature without a final newline";
    const std::array<wstr_view, 3> names = {L"a", L"asset.bin", L"empty"};
    for(size_t removed = 0; removed < patches.size(); ++removed) {
        DYNAMIC_SECTION("Remove " << removed) {
            const TestFile file(prefix + kMailSummary + patches[0] + patches[1] + patches[2] + footer);
            PatchArchive::DeleteEntries(file.GetPath(), std::span<const wstr_view>(&names[removed], 1));
            std::string expected = prefix;
            for(size_t i = 0; i < patches.size(); ++i) {
                if(i != removed) expected += patches[i];
            }
            expected += footer;
            REQUIRE(file.ReadBytes() == expected);
            const auto archive = PatchArchive::Open(file.GetPath());
            REQUIRE(archive.GetEntries().size() == 2);
            size_t entry = 0;
            for(size_t i = 0; i < patches.size(); ++i) {
                if(i == removed) continue;
                REQUIRE(archive.ReadEntry(entry) == prefix + patches[i] + footer);
                REQUIRE(archive.GetEntries()[entry].size == archive.ReadEntry(entry).size());
                ++entry;
            }
        }
    }
}

TEST_CASE("SVN uses Index names and preserves conflicting header annotations verbatim", "[svn][paths]") {
    const std::string patch = MakeSvnHeader("src/file.txt") +
        "--- ../../old\t(revision 123)\n+++ C:/other\t(working copy)\n"
        "@@ -1 +1 @@\n-old\n+new\n";
    const auto archive = PatchArchive::Parse(patch);
    REQUIRE(archive.GetFormat() == PatchFormat::kSvn);
    REQUIRE(archive.GetEntries().size() == 2);
    REQUIRE(archive.GetEntries()[0].path == L"src");
    REQUIRE(archive.GetEntries()[0].is_directory);
    REQUIRE(archive.GetEntries()[1].path == L"src/file.txt");
    REQUIRE(archive.GetEntries()[1].size == patch.size());
    REQUIRE(archive.ReadEntry(1) == patch);
    REQUIRE(PatchArchive::Parse(archive.ReadEntry(1)).ReadEntry(1) == patch);
    RequireParseError(MakeSvnHeader("../unsafe") + MakePatch("safe"), ErrorCode::kInvalidPath);
}

TEST_CASE("SVN creation deletion and empty-file headers remain per-file entries", "[svn]") {
    const std::array<std::string, 4> patches = {
        MakeSvnHeader("added") + "--- added\t(nonexistent)\n+++ added\t(revision 2)\n@@ -0,0 +1 @@\n+text\n",
        MakeSvnHeader("removed") + "--- removed\t(revision 1)\n+++ removed\t(nonexistent)\n@@ -1 +0,0 @@\n-text\n",
        MakeSvnHeader("empty-added") + "--- empty-added\t(nonexistent)\n+++ empty-added\t(working copy)\n",
        MakeSvnHeader("empty-removed") + "--- empty-removed\t(revision 1)\n+++ empty-removed\t(nonexistent)\n"};
    std::string bytes;
    for(const auto& patch : patches) bytes += patch;
    const auto archive = PatchArchive::Parse(bytes);
    REQUIRE(archive.GetEntries().size() == patches.size());
    for(size_t i = 0; i < patches.size(); ++i) {
        REQUIRE(PatchArchive::Recognize(patches[i]));
        REQUIRE(archive.ReadEntry(i) == patches[i]);
        REQUIRE(archive.GetEntries()[i].size == patches[i].size());
        REQUIRE(PatchArchive::Parse(archive.ReadEntry(i)).ReadEntry(0) == patches[i]);
    }
}

TEST_CASE("SVN empty-file wrappers are recognizable standalone and between sections", "[svn][delete]") {
    const std::array<std::string, 3> patches = {
        MakeSvnHeader("empty-first"), MakeSvnPatch("changed"), MakeSvnHeader("empty-last")};
    for(const auto& patch : patches) {
        REQUIRE(PatchArchive::Recognize(patch));
        const auto archive = PatchArchive::Parse(patch);
        REQUIRE(archive.GetFormat() == PatchFormat::kSvn);
        REQUIRE(archive.GetEntries().size() == 1);
        REQUIRE(archive.GetEntries()[0].size == patch.size());
        REQUIRE(archive.ReadEntry(0) == patch);
    }
    const std::string bytes = patches[0] + patches[1] + patches[2];
    REQUIRE(PatchArchive::Recognize(bytes));
    const auto archive = PatchArchive::Parse(bytes);
    REQUIRE(archive.GetEntries().size() == patches.size());
    REQUIRE(archive.GetEntries()[0].path == L"empty-first");
    REQUIRE(archive.GetEntries()[2].path == L"empty-last");
    for(size_t i = 0; i < patches.size(); ++i) REQUIRE(archive.ReadEntry(i) == patches[i]);

    const TestFile file(bytes);
    const std::array<wstr_view, 1> changed = {L"changed"};
    PatchArchive::DeleteEntries(file.GetPath(), changed);
    REQUIRE(file.ReadBytes() == patches[0] + patches[2]);
    REQUIRE(PatchArchive::CanOpen(file.GetPath()));
    REQUIRE(PatchArchive::Open(file.GetPath()).GetEntries().size() == 2);
    const std::array<wstr_view, 2> empty = {L"empty-first", L"empty-last"};
    PatchArchive::DeleteEntries(file.GetPath(), empty);
    REQUIRE(file.ReadBytes().empty());

    RequireParseError(MakeSvnHeader("../unsafe"), ErrorCode::kInvalidPath);
    RequireParseError(MakeSvnHeader("."), ErrorCode::kInvalidPath);
    RequireParseError(patches[0] + "Index: broken\n", ErrorCode::kInvalidPatch);
    RequireParseError(patches[0] + "diff --git a/empty b/empty\nindex 123..456\n",
                      ErrorCode::kInvalidPatch);
}

TEST_CASE("SVN property-only and content-plus-property changes form complete entries", "[svn][properties]") {
    const std::string property = MakeSvnProperties("only");
    const std::string combined = MakeSvnPatch("both") + "\nProperty changes on: conflicting/name\n____\n"
        "Added: svn:executable\n## -0,0 +1 ##\n+*\n\\ No newline at end of property\n"
        "Deleted: custom\n## -2 +0,0 ##\n-Index: fake\n-Property changes on: fake\n";
    const auto archive = PatchArchive::Parse(property + combined + MakeSvnPatch("next"));
    REQUIRE(archive.GetEntries().size() == 3);
    REQUIRE(archive.GetEntries()[0].path == L"only");
    REQUIRE(archive.ReadEntry(0) == property);
    REQUIRE(archive.GetEntries()[1].path == L"both");
    REQUIRE(archive.ReadEntry(1) == combined);
    REQUIRE(archive.GetEntries()[1].size == combined.size());
    REQUIRE(PatchArchive::Parse(combined).ReadEntry(0) == combined);
    const TestFile file(property + combined);
    const std::array<wstr_view, 1> selected = {L"both"};
    PatchArchive::DeleteEntries(file.GetPath(), selected);
    REQUIRE(file.ReadBytes() == property);
}

TEST_CASE("SVN property headers can accompany file headers without content hunks", "[svn][properties]") {
    const std::string patch = MakeSvnHeader("props.txt") + "--- props.txt\t(revision 1)\n"
        "+++ props.txt\t(working copy)\n\nProperty changes on: ignored\n____\n"
        "Modified: custom\n## arbitrary unused range description\n-old\n+new\n";
    REQUIRE(PatchArchive::Recognize(patch));
    const auto archive = PatchArchive::Parse(patch);
    REQUIRE(archive.GetEntries().size() == 1);
    REQUIRE(archive.ReadEntry(0) == patch);
    const std::string legacy = MakeSvnHeader("legacy") + "Property changes on: legacy\n____\n"
        "Name: custom\n   - old\n   + new\nModified: svn:mergeinfo\n   Merged /branch:r2-4\n";
    REQUIRE(PatchArchive::Parse(legacy).ReadEntry(0) == legacy);
}

TEST_CASE("SVN binary notices with properties are preserved without replacement data", "[svn][binary]") {
    for(const std::string notice : {"Cannot display: file marked as a binary type.\nsvn:mime-type = application/octet-stream\n",
                                   "Cannot display: file marked as a binary type.\n",
                                   "Binary files old/asset.bin and new/asset.bin differ\n"}) {
        DYNAMIC_SECTION(notice) {
            const std::string patch = MakeSvnHeader("asset.bin") + notice +
                "\nProperty changes on: ignored\n____\nModified: custom\n## -1 +1 ##\n-old\n+new\n";
            REQUIRE(PatchArchive::Recognize(patch));
            const auto archive = PatchArchive::Parse(patch + MakeSvnPatch("keep"));
            REQUIRE(archive.GetEntries().size() == 2);
            REQUIRE(archive.GetEntries()[0].path == L"asset.bin");
            REQUIRE(archive.ReadEntry(0) == patch);
            REQUIRE(archive.GetEntries()[0].size == patch.size());
            REQUIRE(PatchArchive::Parse(patch).ReadEntry(0) == patch);
            const TestFile file(patch + MakeSvnPatch("keep"));
            const std::array<wstr_view, 1> selected = {L"asset.bin"};
            PatchArchive::DeleteEntries(file.GetPath(), selected);
            REQUIRE(file.ReadBytes() == MakeSvnPatch("keep"));
        }
    }
    const std::string notice_only = MakeSvnHeader("asset.bin") + "Cannot display: binary type\n";
    REQUIRE(PatchArchive::Parse(notice_only).ReadEntry(0) == notice_only);
}

TEST_CASE("SVN directory-property classification uses all Index paths case-insensitively", "[svn][properties]") {
    const std::string root = MakeSvnProperties(".");
    const std::string directory = MakeSvnProperties("SRC");
    const std::string first = MakeSvnPatch("src/a");
    const std::string unknown = MakeSvnProperties("unknown-directory");
    for(const bool before : {false, true}) {
        DYNAMIC_SECTION("Directory properties before descendants " << before) {
            const std::string bytes = root + (before ? directory + first : first + directory) + unknown;
            const auto archive = PatchArchive::Parse(bytes);
            REQUIRE(archive.GetEntries().size() == 3);
            REQUIRE(archive.GetEntries()[0].path == L"src");
            REQUIRE(archive.GetEntries()[0].is_directory);
            REQUIRE(archive.GetEntries()[1].path == L"src/a");
            REQUIRE(archive.ReadEntry(1) == first);
            REQUIRE(archive.GetEntries()[2].path == L"unknown-directory");
            REQUIRE_FALSE(archive.GetEntries()[2].is_directory);
            REQUIRE(archive.ReadEntry(2) == unknown);
        }
    }
    const auto properties = PatchArchive::Parse(MakeSvnProperties("dir.txt") + MakeSvnProperties("dir.txt/child"));
    REQUIRE(properties.GetEntries().size() == 2);
    REQUIRE(properties.GetEntries()[0].is_directory);
    REQUIRE(properties.GetEntries()[1].path == L"dir.txt/child");
}

TEST_CASE("SVN root properties alone form a recognizable archive with zero entries", "[svn][empty]") {
    const std::string bytes = MakeSvnProperties(".");
    REQUIRE(PatchArchive::Recognize(bytes));
    const TestFile file(bytes);
    REQUIRE(PatchArchive::CanOpen(file.GetPath()));
    const auto archive = PatchArchive::Open(file.GetPath());
    REQUIRE(archive.GetFormat() == PatchFormat::kSvn);
    REQUIRE(archive.GetEntries().empty());
    PatchArchive::DeleteEntries(file.GetPath(), {});
    REQUIRE(file.ReadBytes() == bytes);
    const std::array<wstr_view, 1> root = {L"."};
    REQUIRE_THROWS_AS(PatchArchive::DeleteEntries(file.GetPath(), root), PatchError);
    REQUIRE(file.ReadBytes() == bytes);
}

TEST_CASE("SVN deleting every visible entry retains root properties and BOM", "[svn][delete]") {
    for(const bool bom : {false, true}) {
        DYNAMIC_SECTION("BOM " << bom) {
            const std::string prefix = bom ? detail::kUtf8Bom : "";
            const std::string root = MakeSvnProperties(".");
            const TestFile file(prefix + MakeSvnPatch("src/a") + root + MakeSvnPatch("src/b"));
            const std::array<wstr_view, 1> selected = {L"src"};
            RecordedWriteProgress progress{prefix.size() + root.size()};
            PatchArchive::DeleteEntries(file.GetPath(), selected, {RecordedWriteProgress::Report, &progress});
            REQUIRE(file.ReadBytes() == prefix + root);
            REQUIRE_FALSE(progress.invalid);
            REQUIRE(progress.last_written == prefix.size() + root.size());
            REQUIRE(PatchArchive::CanOpen(file.GetPath()));
            const auto remaining = PatchArchive::Open(file.GetPath());
            REQUIRE(remaining.GetFormat() == PatchFormat::kSvn);
            REQUIRE(remaining.GetEntries().empty());
        }
    }
}

TEST_CASE("SVN directory properties survive recursive deletion and are reinferred on reopening", "[svn][delete]") {
    const std::string hidden = MakeSvnProperties("src");
    const std::string root = MakeSvnProperties(".");
    const std::string keep = MakeSvnPatch("src_old/keep");
    const TestFile file(hidden + MakeSvnPatch("src/a") + root + MakeSvnPatch("src/sub/b") + keep);
    const auto original = PatchArchive::Open(file.GetPath());
    const std::array<wstr_view, 2> selected = {L"SRC\\", L"src/sub/b"};
    PatchArchive::DeleteEntries(file.GetPath(), selected);
    REQUIRE(file.ReadBytes() == hidden + root + keep);
    const auto archive = PatchArchive::Open(file.GetPath());
    REQUIRE(archive.GetEntries().size() == 3);
    REQUIRE(archive.GetEntries()[0].path == L"src");
    REQUIRE_FALSE(archive.GetEntries()[0].is_directory);
    REQUIRE(archive.ReadEntry(0) == hidden);
    REQUIRE(original.GetEntries()[0].is_directory);
    REQUIRE(original.ReadEntry(1) == MakeSvnPatch("src/a"));
}

TEST_CASE("SVN deletion without hidden properties follows the empty archive rule", "[svn][delete]") {
    for(const bool bom : {false, true}) {
        DYNAMIC_SECTION("BOM " << bom) {
            const std::string prefix = bom ? detail::kUtf8Bom : "";
            const TestFile file(prefix + MakeSvnProperties("unknown") + MakeSvnPatch("file"));
            const std::array<wstr_view, 2> selected = {L"unknown", L"file"};
            PatchArchive::DeleteEntries(file.GetPath(), selected);
            REQUIRE(file.ReadBytes() == prefix);
            REQUIRE(PatchArchive::Open(file.GetPath()).GetFormat() == PatchFormat::kEmpty);
            REQUIRE_FALSE(PatchArchive::CanOpen(file.GetPath()));
        }
    }
}

TEST_CASE("SVN probe recognition is bounded and need not consume the first payload", "[svn][probe]") {
    const std::string header = MakeSvnHeader("a") + "--- a\t(revision 1)\n+++ a\t(revision 2)\n";
    const std::string patch = header + "@@ -1 +1 @@\n-old\n+" + std::string(40 * 1024, 'x') + "\n";
    REQUIRE(PatchArchive::Recognize(patch));
    const TestFile file(patch);
    REQUIRE(PatchArchive::CanOpen(file.GetPath()));
    REQUIRE(PatchArchive::Open(file.GetPath()).ReadEntry(0) == patch);
    REQUIRE(PatchArchive::Recognize(header + "@@ -2,2 +1 @@\n"));
    RequireParseError(header + "@@ -2,2 +1 @@\n", ErrorCode::kInvalidPatch);
    REQUIRE(PatchArchive::Recognize(MakeSvnHeader("p") + "Property changes on: p\n____\n"));
    REQUIRE(PatchArchive::Recognize(MakeSvnHeader("p")));
    REQUIRE_FALSE(PatchArchive::Recognize(std::string(32 * 1024, '\n') + patch));
    REQUIRE(PatchArchive::Recognize(std::string(8 * 1024, '\n') + patch));
    const TestFile late_binary(patch + std::string(1, '\0'));
    REQUIRE(PatchArchive::CanOpen(late_binary.GetPath()));
    REQUIRE_THROWS_AS(PatchArchive::Open(late_binary.GetPath()), PatchError);
}

TEST_CASE("SVN source and property body lines resembling headers keep their boundaries", "[svn]") {
    const std::string patch = MakeSvnHeader("a") + "--- a\n+++ a\n@@ -2 +2 @@\n"
        "-Index: fake\n+Property changes on: fake\n@@ -1 +1 @@suffix\n same\n"
        "\nProperty changes on: a\n____\nModified: custom\n## -1 +1 ##\n"
        "-Index: fake\n+Index: fake\n\\ No newline at end of property\n";
    const auto archive = PatchArchive::Parse(patch + MakeSvnPatch("next"));
    REQUIRE(archive.GetEntries().size() == 2);
    REQUIRE(archive.ReadEntry(0) == patch);
    REQUIRE(archive.ReadEntry(1) == MakeSvnPatch("next"));
}

TEST_CASE("SVN path decoding keeps UTF-8 ANSI fallback and safety rules", "[svn][paths]") {
    const std::string unicode = MakeSvnPatch("./źródła\\żółć file.cpp");
    const auto archive = PatchArchive::Parse(detail::kUtf8Bom + unicode);
    REQUIRE(archive.GetEntries()[1].path == L"źródła/żółć file.cpp");
    REQUIRE(archive.ReadEntry(1) == detail::kUtf8Bom + unicode);
    for(const std::string path : {"../outside", "C:/outside", "/absolute", "NUL.txt", "a/../b", "a/"}) {
        RequireParseError(MakeSvnProperties(path), ErrorCode::kInvalidPath);
    }
    RequireParseError(MakeSvnPatch("A") + MakeSvnPatch("a"), ErrorCode::kInvalidPath);
    RequireParseError(MakeSvnPatch("a") + MakeSvnPatch("a/b"), ErrorCode::kInvalidPath);
    RequireParseError(detail::kUtf8Bom + MakeSvnPatch(std::string("bad_\xFF")), ErrorCode::kInvalidPath);
    if(GetACP() != CP_UTF8) {
        const std::string ansi = MakeSvnPatch(std::string("name_\xFF"));
        REQUIRE(PatchArchive::Parse(ansi).ReadEntry(0) == ansi);
    }
}

TEST_CASE("SVN original BOM endings and missing final newline survive extraction", "[svn][bytes]") {
    for(const std::string ending : {"\n", "\r\n", "\r"}) {
        for(const bool bom : {false, true}) {
            DYNAMIC_SECTION("Ending " << int(ending[0]) << " length " << ending.size() << " BOM " << bom) {
                const std::string original = MakeSvnPatch("a") +
                    "\\ No newline at end of file\n\nProperty changes on: a\n____\nModified: custom\n"
                    "## -1 +1 ##\n-old\n+new";
                std::string bytes = bom ? detail::kUtf8Bom : "";
                for(const char ch : original) bytes += ch == '\n' ? ending : std::string(1, ch);
                const auto archive = PatchArchive::Parse(bytes);
                REQUIRE(archive.ReadEntry(0) == bytes);
                REQUIRE(archive.GetEntries()[0].size == bytes.size());
                REQUIRE(PatchArchive::Parse(archive.ReadEntry(0)).ReadEntry(0) == bytes);
            }
        }
    }
}

TEST_CASE("SVN malformed framing and mixed document formats fail before rewriting", "[svn][invalid]") {
    const std::array<std::string, 9> invalid = {
        "Index: a\n", "Index: a\nnot a separator\n", MakeSvnHeader("a") + "unexpected text\n",
        MakeSvnHeader("a") + "--- a\n", MakeSvnHeader("a") + "@@ -1 +1 @@\n-old\n+new\n",
        MakeSvnHeader("a") + "Property changes on: a\n",
        MakeSvnHeader("a") + "Property changes on: a\nnot an underline\n",
        MakeSvnHeader("a") + "Property changes on: a\n____\n",
        MakeSvnHeader("a") + "Property changes on: a\n____\nUnknown text\n"};
    for(size_t i = 0; i < invalid.size(); ++i) {
        DYNAMIC_SECTION("Invalid SVN framing " << i) {
            RequireParseError(invalid[i], ErrorCode::kInvalidPatch);
        }
    }
    RequireParseError(MakeSvnPatch("."), ErrorCode::kInvalidPath);
    const std::string svn = MakeSvnPatch("a");
    RequireParseError(svn + MakePatch("b"), ErrorCode::kInvalidPatch);
    RequireParseError(MakePatch("b") + svn, ErrorCode::kUnsupportedFeature);
    RequireParseError(svn + MakeGitPatch("b"), ErrorCode::kUnsupportedFeature);
    RequireParseError(MakeGitPatch("b") + svn, ErrorCode::kUnsupportedFeature);
    RequireParseError(svn + MakeContextPatch("b"), ErrorCode::kInvalidPatch);
    RequireParseError(MakeContextPatch("b") + svn, ErrorCode::kUnsupportedFeature);
    RequireParseError(svn + kMailHeaders + MakeGitPatch("b"), ErrorCode::kUnsupportedFeature);
    const std::string broken = svn + MakeSvnHeader("b") + MakePatch("b") + "unexpected trailer\n";
    const TestFile file(broken);
    const std::array<wstr_view, 1> selected = {L"a"};
    REQUIRE_THROWS_AS(PatchArchive::DeleteEntries(file.GetPath(), selected), PatchError);
    REQUIRE(file.ReadBytes() == broken);
    const TestFile valid(svn);
    const std::array<wstr_view, 1> missing = {L"missing"};
    REQUIRE_THROWS_AS(PatchArchive::DeleteEntries(valid.GetPath(), missing), PatchError);
    REQUIRE(valid.ReadBytes() == svn);
}

TEST_CASE("RegEngine SVN fixtures expose every section and support recursive deletion", "[svn][fixture][delete]") {
    for(const std::string name : {"regengine_svn.diff", "regengine_svn_working_copy.diff", "regengine_svn_git.diff"}) {
        DYNAMIC_SECTION(name) {
            const std::wstring path = std::wstring(L"" PATCH_TEST_EXAMPLES_DIR) + L"/RegEngine/" +
                                      std::wstring(name.begin(), name.end());
            const auto archive = PatchArchive::Open(path);
            REQUIRE(archive.GetFormat() == PatchFormat::kSvn);
            std::string copy;
            std::string retained;
            for(size_t i = 0; i < archive.GetEntries().size(); ++i) {
                const auto& entry = archive.GetEntries()[i];
                if(entry.is_directory) continue;
                const std::string bytes = archive.ReadEntry(i);
                REQUIRE(bytes.starts_with("Index: "));
                if(name == "regengine_svn_git.diff") REQUIRE(bytes.find("\ndiff --git ") != std::string::npos);
                REQUIRE(bytes.size() == entry.size);
                const auto extracted = PatchArchive::Parse(bytes);
                REQUIRE(extracted.GetEntries().back().path == entry.path);
                REQUIRE(extracted.ReadEntry(extracted.GetEntries().size() - 1) == bytes);
                copy += bytes;
                if(!entry.path.starts_with(L"Source/")) retained += bytes;
            }
            const TestFile file(copy);
            const std::array<wstr_view, 1> selected = {L"Source"};
            PatchArchive::DeleteEntries(file.GetPath(), selected);
            REQUIRE(file.ReadBytes() == retained);
            REQUIRE(PatchArchive::Open(file.GetPath()).GetFormat() == PatchFormat::kSvn);
        }
    }
}

TEST_CASE("SVN Git metadata-only operations retain Index names and complete sections", "[svn][git][metadata]") {
    const std::array<std::string, 5> operations = {
        "new file mode 100644\n", "deleted file mode 100644\n", "old mode 100644\nnew mode 100755\n",
        "copy from trunk/old name@123\ncopy to branches/new name\n",
        "similarity index 100%\nrename from trunk/old name\nrename to branches/new name\n"};
    for(size_t i = 0; i < operations.size(); ++i) {
        for(const bool headers : {false, true}) {
            DYNAMIC_SECTION("Operation " << i << " file headers " << headers) {
                const std::string patch = MakeSvnHeader("new name") +
                    "diff --git a/trunk/old name b/branches/new name\n" + operations[i] +
                    (headers ? "--- a/trunk/old name\t(revision 123)\n"
                               "+++ b/branches/new name\t(working copy)\n" : "");
                REQUIRE(PatchArchive::Recognize(patch));
                const auto archive = PatchArchive::Parse(patch + MakeSvnPatch("keep"));
                REQUIRE(archive.GetFormat() == PatchFormat::kSvn);
                REQUIRE(archive.GetEntries().size() == 2);
                REQUIRE(archive.GetEntries()[0].path == L"new name");
                REQUIRE(archive.GetEntries()[0].size == patch.size());
                REQUIRE(archive.ReadEntry(0) == patch);
                REQUIRE(PatchArchive::Parse(archive.ReadEntry(0)).ReadEntry(0) == patch);
                const TestFile file(patch + MakeSvnPatch("keep"));
                const std::array<wstr_view, 1> selected = {L"new name"};
                PatchArchive::DeleteEntries(file.GetPath(), selected);
                REQUIRE(file.ReadBytes() == MakeSvnPatch("keep"));
            }
        }
    }
}

TEST_CASE("SVN Git annotations remain opaque while Index paths retain safety checks", "[svn][git][paths]") {
    const std::string body = "diff --git \"a/trunk/old name\" \"b/branches/new name\"\n"
        "copy from trunk/old name@123\ncopy to branches/new name\n"
        "index arbitrary unused identity\nx-extension retained\n"
        "--- ../../unused\t(revision 123)\n+++ C:/unused\t(working copy)\n"
        "@@ -1 +1 @@\n-old\n+new\n";
    const std::string patch = MakeSvnHeader("a/źródła file.txt") + body;
    const auto archive = PatchArchive::Parse(patch);
    REQUIRE(archive.GetEntries().back().path == L"a/źródła file.txt");
    REQUIRE(archive.ReadEntry(1) == patch);
    REQUIRE(archive.GetEntries()[1].size == patch.size());
    for(const std::string path : {"../outside", "C:/outside", "NUL.txt"}) {
        RequireParseError(MakeSvnHeader(path) + body, ErrorCode::kInvalidPath);
    }
    RequireParseError(patch + MakeSvnHeader("A/ŹRÓDŁA FILE.TXT") + body, ErrorCode::kInvalidPath);
}

TEST_CASE("SVN Git property sections keep directory and file-operation semantics", "[svn][git][properties][delete]") {
    const std::string properties = "\nProperty changes on: trunk/ignored\n____\nModified: custom\n"
        "## arbitrary property range\n-old\n+new\n";
    const std::string root = MakeSvnHeader(".") + "diff --git a/ b/\n--- a/\n+++ b/\n" + properties;
    const std::string directory = MakeSvnHeader("dir") +
        "diff --git a/trunk/dir b/trunk/dir\n--- a/trunk/dir\n+++ b/trunk/dir\n" + properties;
    const std::string child = MakeSvnHeader("dir/file") + MakeGitPatch("trunk/dir/file") + properties;
    const std::string mode = MakeSvnHeader("run") +
        "diff --git a/trunk/run b/trunk/run\nold mode 100644\nnew mode 100755\n" + properties;
    REQUIRE(PatchArchive::Recognize(root));
    REQUIRE(PatchArchive::Parse(root).GetEntries().empty());
    const auto archive = PatchArchive::Parse(root + directory + child + mode);
    REQUIRE(archive.GetEntries().size() == 3);
    REQUIRE(archive.GetEntries()[0].is_directory);
    REQUIRE(archive.GetEntries()[1].path == L"dir/file");
    REQUIRE(archive.ReadEntry(1) == child);
    REQUIRE(archive.GetEntries()[2].path == L"run");
    REQUIRE(archive.ReadEntry(2) == mode);
    REQUIRE(PatchArchive::Parse(mode).ReadEntry(0) == mode);
    const TestFile file(root + directory + child + mode);
    const std::array<wstr_view, 2> selected = {L"dir", L"run"};
    PatchArchive::DeleteEntries(file.GetPath(), selected);
    REQUIRE(file.ReadBytes() == root + directory);
    REQUIRE(PatchArchive::Open(file.GetPath()).ReadEntry(0) == directory);
    // A file operation with properties must not be hidden as directory properties.
    RequireParseError(mode + MakeSvnPatch("run/child"), ErrorCode::kInvalidPath);
    RequireParseError(MakeSvnHeader(".") + "diff --git a/ b/\nnew file mode 100644\n" + properties,
                      ErrorCode::kInvalidPath);
}

TEST_CASE("SVN Git binary payloads preserve records properties and deletion boundaries", "[svn][git][binary][delete]") {
    const std::array<std::string, 3> payloads = {
        kGitLiteral, kGitDelta,
        "diff --git a/asset.bin b/asset.bin\nGIT binary patch\nliteral 0\nA00000\n\n"};
    for(size_t i = 0; i < payloads.size(); ++i) {
        for(const bool bom : {false, true}) {
            DYNAMIC_SECTION("Payload " << i << " BOM " << bom) {
                const std::string prefix = bom ? detail::kUtf8Bom : "";
                const std::string patch = MakeSvnHeader("asset.bin") + payloads[i] +
                    "\nProperty changes on: asset.bin\n____\nModified: svn:mime-type\n"
                    "## -1 +1 ##\n-old\n+application/octet-stream\n";
                REQUIRE(PatchArchive::Recognize(prefix + patch));
                const auto archive = PatchArchive::Parse(prefix + patch + MakeSvnPatch("keep"));
                REQUIRE(archive.GetEntries().size() == 2);
                REQUIRE(archive.GetEntries()[0].path == L"asset.bin");
                REQUIRE(archive.GetEntries()[0].size == prefix.size() + patch.size());
                REQUIRE(archive.ReadEntry(0) == prefix + patch);
                REQUIRE(PatchArchive::Parse(archive.ReadEntry(0)).ReadEntry(0) == prefix + patch);
                const TestFile file(prefix + patch + MakeSvnPatch("keep"));
                const std::array<wstr_view, 1> selected = {L"asset.bin"};
                PatchArchive::DeleteEntries(file.GetPath(), selected);
                REQUIRE(file.ReadBytes() == prefix + MakeSvnPatch("keep"));
                const TestFile all(prefix + patch);
                PatchArchive::DeleteEntries(all.GetPath(), selected);
                REQUIRE(all.ReadBytes() == prefix);
            }
        }
    }
}

TEST_CASE("SVN Git probes accept metadata and binary prefixes without full payloads", "[svn][git][probe]") {
    const std::string header = MakeSvnHeader("file") + "diff --git a/trunk/file b/trunk/file\n";
    REQUIRE_FALSE(PatchArchive::Recognize(header));
    REQUIRE_FALSE(PatchArchive::Recognize(header + "index 123..456\n"));
    REQUIRE(PatchArchive::Recognize(header + "new file mode 100644\n"));
    REQUIRE(PatchArchive::Recognize(header + "GIT binary patch\n"));
    REQUIRE(PatchArchive::Recognize(header + "GIT binary patch\nliteral 123\n"));
    RequireParseError(header + "GIT binary patch\nliteral 123\n", ErrorCode::kInvalidPatch);
    REQUIRE_FALSE(PatchArchive::Recognize(header + "GIT binary patch\nliteral invalid\n"));
    const std::string patch = header + "new file mode 100644\nx-extension " +
        std::string(40 * 1024, 'x') + "\n";
    REQUIRE(PatchArchive::Recognize(patch));
    const TestFile file(patch);
    REQUIRE(PatchArchive::CanOpen(file.GetPath()));
    REQUIRE(PatchArchive::Open(file.GetPath()).ReadEntry(0) == patch);
    const std::string text = MakeSvnHeader("file") + MakeGitPatch("trunk/file");
    REQUIRE(PatchArchive::Recognize(text.substr(0, text.find("-old"))));
    RequireParseError(text.substr(0, text.find("-old")), ErrorCode::kInvalidPatch);
}

TEST_CASE("SVN Git original BOM line endings and final metadata line are preserved", "[svn][git][bytes]") {
    for(const std::string ending : {"\n", "\r\n", "\r"}) {
        for(const bool bom : {false, true}) {
            DYNAMIC_SECTION("Ending size " << ending.size() << " first " << int(ending[0]) << " BOM " << bom) {
                const std::string original = MakeSvnHeader("empty") +
                    "diff --git a/trunk/empty b/trunk/empty\nnew file mode 100644";
                const std::string prefix = bom ? detail::kUtf8Bom : "";
                std::string bytes = prefix;
                for(const char ch : original) bytes += ch == '\n' ? ending : std::string(1, ch);
                REQUIRE(PatchArchive::Recognize(bytes));
                const auto archive = PatchArchive::Parse(bytes);
                REQUIRE(archive.GetEntries()[0].size == bytes.size());
                REQUIRE(archive.ReadEntry(0) == bytes);
                REQUIRE(PatchArchive::Parse(archive.ReadEntry(0)).ReadEntry(0) == bytes);
                const TestFile file(bytes);
                const std::array<wstr_view, 1> selected = {L"empty"};
                PatchArchive::DeleteEntries(file.GetPath(), selected);
                REQUIRE(file.ReadBytes() == prefix);
            }
        }
    }
}

TEST_CASE("SVN Git unusable metadata payloads and mixed wrappers fail before rewriting", "[svn][git][invalid]") {
    const std::string header = MakeSvnHeader("bad") + "diff --git a/bad b/bad\n";
    const std::array<std::string, 10> invalid = {
        "", "index 123..456\n", "copy from old@123\n", "rename to new\n", "old mode 100644\n",
        "new file mode 100644\nGIT binary patch\nliteral 1\nA00000\n",
        "GIT binary patch\nliteral 1\nAshorter\n\n",
        "GIT binary patch\nliteral invalid\nA00000\n\n",
        "--- a/bad\n", "@@ -1 +1 @@\n-old\n+new\n"};
    for(size_t i = 0; i < invalid.size(); ++i) {
        DYNAMIC_SECTION("Malformed section " << i) {
            RequireParseError(header + invalid[i], ErrorCode::kInvalidPatch);
            const std::string bytes = MakeSvnPatch("keep") + header + invalid[i];
            const TestFile file(bytes);
            const std::array<wstr_view, 1> selected = {L"keep"};
            REQUIRE_THROWS_AS(PatchArchive::DeleteEntries(file.GetPath(), selected), PatchError);
            REQUIRE(file.ReadBytes() == bytes);
        }
    }
    RequireParseError(MakeSvnHeader("bad") + "diff --git \n", ErrorCode::kInvalidPatch);
    RequireParseError(header + "diff --cc bad\n", ErrorCode::kUnsupportedFeature);
    RequireParseError(header + MakeGitPatch("bad"), ErrorCode::kUnsupportedFeature);
    RequireParseError(MakeSvnHeader("bad") + "GIT binary patch\nliteral 0\nA00000\n\n",
                      ErrorCode::kInvalidPatch);
    RequireParseError(MakeSvnHeader("bad") + MakeGitPatch("bad") + MakeGitPatch("other"),
                      ErrorCode::kUnsupportedFeature);
}

TEST_CASE("Git and mailbox binary notices are complete per-file patch entries", "[git][mailbox][binary]") {
    for(const bool mail : {false, true}) {
        for(const bool index : {false, true}) {
            DYNAMIC_SECTION("Mailbox " << mail << " index header " << index) {
                const std::string prefix = mail ? kMailHeaders + kMailMessage + "---\n" : "";
                const std::string suffix = mail ? kMailFooter : "";
                const std::string notice = "diff --git a/asset.bin b/asset.bin\n" +
                    std::string(index ? "index abc..def 100644\n" : "") +
                    "Binary files a/asset.bin and b/asset.bin differ\n";
                const std::string keep = MakeGitPatch("keep");
                const std::string bytes = prefix + (mail ? kMailSummary : "") + notice + keep + suffix;
                REQUIRE(PatchArchive::Recognize(bytes));
                const TestFile file(bytes);
                REQUIRE(PatchArchive::CanOpen(file.GetPath()));
                const auto archive = PatchArchive::Open(file.GetPath());
                REQUIRE(archive.GetFormat() == (mail ? PatchFormat::kGitMailbox : PatchFormat::kGit));
                REQUIRE(archive.GetEntries().size() == 2);
                REQUIRE(archive.GetEntries()[0].path == L"asset.bin");
                REQUIRE(archive.GetEntries()[0].size == prefix.size() + notice.size() + suffix.size());
                REQUIRE(archive.ReadEntry(0) == prefix + notice + suffix);
                REQUIRE(archive.ReadEntry(1) == prefix + keep + suffix);
                const auto extracted = PatchArchive::Parse(archive.ReadEntry(0));
                REQUIRE(extracted.GetEntries().size() == 1);
                REQUIRE(extracted.ReadEntry(0) == archive.ReadEntry(0));
            }
        }
    }
}

TEST_CASE("Git binary notices coexist with creation deletion rename copy and mode metadata", "[git][binary]") {
    const std::array<std::string, 6> sections = {
        "diff --git a/added b/added\nnew file mode 100644\nBinary files /dev/null and b/added differ\n",
        "diff --git a/deleted b/deleted\ndeleted file mode 100644\nBinary files a/deleted and /dev/null differ\n",
        "diff --git a/old b/renamed\nrename from old\nrename to renamed\nBinary files a/old and b/renamed differ\n",
        "diff --git a/source b/copied\ncopy from source\ncopy to copied\nBinary files a/source and b/copied differ\n",
        "diff --git a/script b/script\nold mode 100644\nnew mode 100755\nBinary files a/script and b/script differ\n",
        "diff --git a/headers b/headers\n--- /dev/null\n+++ b/headers\nBinary files /dev/null and b/headers differ\n"};
    const std::array<wstr_view, 6> names = {L"added", L"deleted", L"renamed", L"copied", L"script", L"headers"};
    std::string bytes;
    for(const auto& section : sections) bytes += section;
    const auto archive = PatchArchive::Parse(bytes);
    REQUIRE(archive.GetEntries().size() == sections.size());
    for(size_t i = 0; i < sections.size(); ++i) {
        REQUIRE(PatchArchive::Recognize(sections[i]));
        REQUIRE(archive.GetEntries()[i].path == names[i].c_str());
        REQUIRE(archive.ReadEntry(i) == sections[i]);
        REQUIRE(PatchArchive::Parse(sections[i]).ReadEntry(0) == sections[i]);
    }
}

TEST_CASE("Git notice display names do not override safe section paths", "[git][binary][paths]") {
    for(const bool prefixes : {false, true}) {
        const std::string bytes = MakeGitNotice("a/file and name.bin", prefixes);
        const auto archive = PatchArchive::Parse(bytes);
        REQUIRE(archive.GetEntries().back().path == L"a/file and name.bin");
        REQUIRE(archive.ReadEntry(archive.GetEntries().size() - 1) == bytes);
    }
    const std::string quoted = "diff --git \"a/\\305\\274.bin\" \"b/\\305\\274.bin\"\n"
        "Binary files ignored-old and ignored-new differ\n";
    const auto archive = PatchArchive::Parse(detail::kUtf8Bom + quoted);
    REQUIRE(archive.GetEntries()[0].path == L"ż.bin");
    REQUIRE(archive.ReadEntry(0) == detail::kUtf8Bom + quoted);
    const std::string unicode = MakeGitNotice("źródła/żółć.bin");
    REQUIRE(PatchArchive::Parse(unicode).GetEntries().back().path == L"źródła/żółć.bin");
    RequireParseError(MakeGitNotice("../outside"), ErrorCode::kInvalidPath);
    RequireParseError(MakeGitNotice("A") + MakeGitNotice("a"), ErrorCode::kInvalidPath);
}

TEST_CASE("Binary notice entries preserve BOM endings and an unterminated final line", "[git][mailbox][binary][bytes]") {
    for(const bool mail : {false, true}) {
        for(const std::string ending : {"\n", "\r\n", "\r"}) {
            for(const bool bom : {false, true}) {
                DYNAMIC_SECTION("Mailbox " << mail << " ending " << int(ending[0]) << " size " << ending.size() << " BOM " << bom) {
                    std::string original = (mail ? kMailHeaders : "") + MakeGitNotice("asset.bin");
                    original.pop_back();
                    std::string bytes = bom ? detail::kUtf8Bom : "";
                    for(const char ch : original) bytes += ch == '\n' ? ending : std::string(1, ch);
                    const auto archive = PatchArchive::Parse(bytes);
                    REQUIRE(archive.ReadEntry(0) == bytes);
                    REQUIRE(archive.GetEntries()[0].size == bytes.size());
                    REQUIRE(PatchArchive::Parse(archive.ReadEntry(0)).ReadEntry(0) == bytes);
                }
            }
        }
    }
}

TEST_CASE("Binary notice deletion shares ordinary Git and mailbox rewrite behavior", "[git][mailbox][binary][delete]") {
    for(const bool mail : {false, true}) {
        for(const bool bom : {false, true}) {
            DYNAMIC_SECTION("Mailbox " << mail << " BOM " << bom) {
                const std::string bom_bytes = bom ? detail::kUtf8Bom : "";
                const std::string prefix = bom_bytes + (mail ? kMailHeaders + kMailMessage + "---\n" : "");
                const std::string suffix = mail ? kMailFooter : "";
                const std::string keep = kGitLiteral + MakeGitPatch("keep");
                const TestFile file(prefix + (mail ? kMailSummary : "") + MakeGitNotice("src/a") +
                                    keep + MakeGitNotice("src/sub/b") + suffix);
                const std::array<wstr_view, 2> selected = {L"SRC", L"src/sub/b"};
                const std::string expected = prefix + keep + suffix;
                RecordedWriteProgress progress{expected.size()};
                PatchArchive::DeleteEntries(file.GetPath(), selected, {RecordedWriteProgress::Report, &progress});
                REQUIRE(file.ReadBytes() == expected);
                REQUIRE_FALSE(progress.invalid);
                const auto archive = PatchArchive::Open(file.GetPath());
                REQUIRE(archive.GetEntries().size() == 2);
                REQUIRE(archive.ReadEntry(0) == prefix + kGitLiteral + suffix);
                const std::array<wstr_view, 2> remaining = {L"asset.bin", L"keep"};
                PatchArchive::DeleteEntries(file.GetPath(), remaining);
                REQUIRE(file.ReadBytes() == bom_bytes);
                REQUIRE(PatchArchive::Open(file.GetPath()).GetEntries().empty());
            }
        }
    }
    const TestFile single(MakeGitNotice("asset.bin"));
    const std::array<wstr_view, 1> selected = {L"asset.bin"};
    PatchArchive::DeleteEntries(single.GetPath(), selected);
    REQUIRE(single.ReadBytes().empty());
}

TEST_CASE("Unusable binary notices still fail", "[git][binary][invalid]") {
    const std::string header = "diff --git a/asset b/asset\n";
    for(const std::string notice : {"Binary files ", "Binary files a and b",
                                   "Binary files a differ", "Binary files  and b differ",
                                   "Binary files a and  differ", "Binary files a and b differ extra"}) {
        DYNAMIC_SECTION(notice) {
            const std::string bytes = header + notice + "\n";
            REQUIRE_FALSE(PatchArchive::Recognize(bytes));
            RequireParseError(bytes, ErrorCode::kInvalidPatch);
        }
    }
    const std::string notice = MakeGitNotice("asset");
    RequireParseError(notice + "unexpected trailer\n", ErrorCode::kInvalidPatch);
    RequireParseError(notice + "Binary files a and b differ\n", ErrorCode::kUnsupportedFeature);
    RequireParseError("Binary files a and b differ\n", ErrorCode::kUnsupportedFeature);
    const std::string broken = header + "Binary files a and b\n";
    const TestFile file(broken);
    const std::array<wstr_view, 1> selected = {L"asset"};
    REQUIRE_THROWS_AS(PatchArchive::DeleteEntries(file.GetPath(), selected), PatchError);
    REQUIRE(file.ReadBytes() == broken);
}

TEST_CASE("Mercurial native diffs use preamble paths for one comparison", "[hg][paths]") {
    for(const bool two_revisions : {false, true}) {
        DYNAMIC_SECTION("Two revisions " << two_revisions) {
            const std::string first = MakeHgPatch("a/żółć file.txt", two_revisions);
            const std::string second = "diff -r 0123456789ab " +
                std::string(two_revisions ? "-r abcdef012345 " : "") +
                "b/keep.txt\n--- unrelated old name\tdate\n+++ unrelated new name\tdate\n"
                "@@ -1 +1 @@\n-old\n+new\n";
            const auto archive = PatchArchive::Parse(first + second);
            REQUIRE(archive.GetFormat() == PatchFormat::kHg);
            REQUIRE(archive.GetEntries().size() == 4);
            REQUIRE(archive.GetEntries()[1].path == L"a/żółć file.txt");
            REQUIRE(archive.GetEntries()[3].path == L"b/keep.txt");
            REQUIRE(archive.ReadEntry(1) == first);
            REQUIRE(archive.ReadEntry(3) == second);
            REQUIRE(PatchArchive::Parse(archive.ReadEntry(1)).GetEntries().back().path == L"a/żółć file.txt");
        }
    }
}

TEST_CASE("Mercurial native additions deletions and counted bodies preserve their bytes", "[hg]") {
    const std::array<std::string, 3> sections = {
        "diff -r old added\n--- /dev/null\n+++ b/added\tdate\n@@ -0,0 +1 @@\n+new\n",
        "diff -r old deleted\n--- a/deleted\tdate\n+++ /dev/null\n@@ -1 +0,0 @@\n-old\n",
        "diff -r old modified\n--- a/modified\n+++ b/modified\n"
        "@@ -999999999999999999999999,2 +0,2 @@ unused text\n"
        "--- source text\n+++ source text\n context\n\\ No newline at end of file\n"
        "@@ -0,0 +0,0 @@\n"};
    const auto archive = PatchArchive::Parse(sections[0] + sections[1] + sections[2]);
    REQUIRE(archive.GetEntries().size() == 3);
    REQUIRE(archive.GetEntries()[0].path == L"added");
    REQUIRE(archive.GetEntries()[1].path == L"deleted");
    for(size_t i = 0; i < sections.size(); ++i) {
        REQUIRE(archive.ReadEntry(i) == sections[i]);
        REQUIRE(PatchArchive::Parse(archive.ReadEntry(i)).GetEntries().size() == 1);
    }
}

TEST_CASE("Mercurial binary notices use enclosing paths and need no binary payload", "[hg][binary]") {
    for(const bool exported : {false, true}) {
        for(const bool plural : {false, true}) {
            DYNAMIC_SECTION("Export " << exported << " plural " << plural) {
                const std::string prefix = exported ? kHgHeaders + kHgMessage : "";
                const std::string notice = "diff -r old -r new asset.bin\n" + std::string(plural ?
                    "Binary files unrelated old and unrelated new differ\n" :
                    "Binary file unrelated display name has changed\n");
                const auto archive = PatchArchive::Parse(prefix + notice);
                REQUIRE(PatchArchive::Recognize(prefix + notice));
                REQUIRE(archive.GetEntries().size() == 1);
                REQUIRE(archive.GetEntries()[0].path == L"asset.bin");
                REQUIRE(archive.GetEntries()[0].size == prefix.size() + notice.size());
                REQUIRE(archive.ReadEntry(0) == prefix + notice);
                REQUIRE(PatchArchive::Parse(archive.ReadEntry(0)).ReadEntry(0) == prefix + notice);
            }
        }
    }
    const std::string git_notice = "diff --git a/asset.bin b/asset.bin\nBinary file asset.bin has changed\n";
    const auto archive = PatchArchive::Parse(kHgHeaders + git_notice);
    REQUIRE(archive.GetEntries()[0].path == L"asset.bin");
    REQUIRE(archive.ReadEntry(0) == kHgHeaders + git_notice);
}

TEST_CASE("Mercurial export extraction preserves all metadata and the complete message", "[hg][export]") {
    for(const bool git : {false, true}) {
        DYNAMIC_SECTION("Git body " << git) {
            const std::string prefix = kHgHeaders + kHgMessage;
            const std::array<std::string, 2> sections = {
                git ? MakeGitPatch("first") : MakeHgPatch("first"),
                git ? MakeGitPatch("second") : MakeHgPatch("second")};
            const auto archive = PatchArchive::Parse(prefix + sections[0] + sections[1] + "\n");
            REQUIRE(archive.GetFormat() == PatchFormat::kHgExport);
            REQUIRE(archive.GetEntries().size() == 2);
            for(size_t i = 0; i < sections.size(); ++i) {
                const std::string expected = prefix + sections[i] + "\n";
                REQUIRE(archive.GetEntries()[i].size == expected.size());
                REQUIRE(archive.ReadEntry(i) == expected);
                const auto extracted = PatchArchive::Parse(expected);
                REQUIRE(extracted.GetFormat() == PatchFormat::kHgExport);
                REQUIRE(extracted.GetEntries().size() == 1);
                REQUIRE(extracted.ReadEntry(0) == expected);
            }
        }
    }
}

TEST_CASE("Mercurial exports reuse Git binary and metadata-only section handling", "[hg][export][binary]") {
    const std::array<std::string, 5> sections = {
        kGitLiteral, kGitDelta,
        "diff --git a/old b/renamed\nsimilarity index 100%\nrename from old\nrename to renamed\n",
        "diff --git a/original b/copied\nsimilarity index 100%\ncopy from original\ncopy to copied\n",
        "diff --git a/empty b/empty\nnew file mode 100644\n"};
    std::string bytes = kHgHeaders;
    for(const auto& section : sections) bytes += section;
    const auto archive = PatchArchive::Parse(bytes);
    REQUIRE(archive.GetEntries().size() == sections.size());
    REQUIRE(archive.GetEntries()[2].path == L"renamed");
    REQUIRE(archive.GetEntries()[3].path == L"copied");
    for(size_t i = 0; i < sections.size(); ++i) {
        REQUIRE(archive.ReadEntry(i) == kHgHeaders + sections[i]);
        REQUIRE(PatchArchive::Parse(archive.ReadEntry(i)).GetEntries().size() == 1);
    }
}

TEST_CASE("Mercurial export messages treat quoted commands and surrounding text as opaque", "[hg][export]") {
    const std::string prefix = kHgHeaders +
        "A quoted command follows:\ndiff -r old example\nNot file headers.\n\n"
        "diff --git a/example b/example\nAnother quoted command.\n\n---\nopaque summary\n\n";
    const std::string section = MakeHgPatch("actual");
    const auto archive = PatchArchive::Parse(prefix + section);
    REQUIRE(archive.GetEntries().size() == 1);
    REQUIRE(archive.GetEntries()[0].path == L"actual");
    REQUIRE(archive.ReadEntry(0) == prefix + section);
    const std::string extended = "diff --git a/actual b/actual\nunknown extension retained\n"
                                 "old mode 100644\nnew mode 100755\n";
    const auto git_archive = PatchArchive::Parse(kHgHeaders + kHgMessage + extended);
    REQUIRE(git_archive.GetEntries()[0].path == L"actual");
    REQUIRE(git_archive.ReadEntry(0) == kHgHeaders + kHgMessage + extended);
}

TEST_CASE("Mercurial recognition handles bounded native probes and long export messages", "[hg][probe][io]") {
    const std::string native = "diff -r old file\n--- a/file\n+++ b/file\n@@ -1 +1 @@\n";
    REQUIRE(PatchArchive::Recognize(native));
    RequireParseError(native, ErrorCode::kInvalidPatch);
    const std::string bytes = kHgHeaders + std::string(40000, 'x') + "\n\n" + MakeHgPatch("file");
    REQUIRE(PatchArchive::Recognize(bytes));
    REQUIRE(PatchArchive::Recognize(kHgHeaders));
    RequireParseError(kHgHeaders, ErrorCode::kInvalidPatch);
    const TestFile file(bytes);
    REQUIRE(PatchArchive::CanOpen(file.GetPath()));
    REQUIRE(PatchArchive::Open(file.GetPath()).ReadEntry(0) == bytes);
}

TEST_CASE("Mercurial documents preserve BOM line endings and no final newline", "[hg][bytes]") {
    for(const bool exported : {false, true}) {
        for(const bool bom : {false, true}) {
            for(const std::string ending : {"\n", "\r\n", "\r"}) {
                DYNAMIC_SECTION("Export " << exported << " BOM " << bom << " ending size " << ending.size() << " first " << int(ending[0])) {
                    std::string original = (exported ? kHgHeaders + kHgMessage : "") + MakeHgPatch("żółć.txt");
                    original.pop_back();
                    std::string bytes = bom ? detail::kUtf8Bom : "";
                    for(const char ch : original) bytes += ch == '\n' ? ending : std::string(1, ch);
                    const auto archive = PatchArchive::Parse(bytes);
                    REQUIRE(archive.GetEntries()[0].path == L"żółć.txt");
                    REQUIRE(archive.GetEntries()[0].size == bytes.size());
                    REQUIRE(archive.ReadEntry(0) == bytes);
                    REQUIRE(PatchArchive::Parse(archive.ReadEntry(0)).ReadEntry(0) == bytes);
                }
            }
        }
    }
}

TEST_CASE("Mercurial deletion retains the original export metadata and surviving sections", "[hg][delete]") {
    for(const bool exported : {false, true}) {
        for(const bool git : {false, true}) {
            if(git && !exported) continue;
            for(size_t removed = 0; removed < 3; ++removed) {
                DYNAMIC_SECTION("Export " << exported << " Git " << git << " removed " << removed) {
                    const std::string prefix = exported ? kHgHeaders + kHgMessage : "";
                    const std::array<std::string, 3> sections = {
                        git ? MakeGitPatch("a") : MakeHgPatch("a"),
                        git ? MakeGitNotice("b") : MakeHgNotice("b"),
                        git ? MakeGitPatch("c") : MakeHgPatch("c")};
                    const TestFile file(prefix + sections[0] + sections[1] + sections[2]);
                    const std::array<wstr_view, 3> paths = {L"a", L"b", L"c"};
                    PatchArchive::DeleteEntries(file.GetPath(), std::span(paths).subspan(removed, 1));
                    std::string expected = prefix;
                    for(size_t i = 0; i < sections.size(); ++i) if(i != removed) expected += sections[i];
                    REQUIRE(file.ReadBytes() == expected);
                    const auto archive = PatchArchive::Open(file.GetPath());
                    REQUIRE(archive.GetEntries().size() == 2);
                    size_t index = 0;
                    for(size_t i = 0; i < sections.size(); ++i) {
                        if(i != removed) REQUIRE(archive.ReadEntry(index++) == prefix + sections[i]);
                    }
                }
            }
        }
    }
}

TEST_CASE("Mercurial directory deletion combines overlaps and removes the final envelope", "[hg][delete]") {
    for(const bool exported : {false, true}) {
        for(const bool bom : {false, true}) {
            DYNAMIC_SECTION("Export " << exported << " BOM " << bom) {
                const std::string bom_bytes = bom ? detail::kUtf8Bom : "";
                const std::string prefix = bom_bytes + (exported ? kHgHeaders + kHgMessage : "");
                const std::string keep = MakeHgPatch("keep");
                const TestFile file(prefix + MakeHgPatch("src/a") + keep + MakeHgNotice("src/sub/b"));
                const std::array<wstr_view, 2> selected = {L"SRC", L"src/sub/b"};
                RecordedWriteProgress progress{prefix.size() + keep.size()};
                PatchArchive::DeleteEntries(file.GetPath(), selected, {RecordedWriteProgress::Report, &progress});
                REQUIRE(file.ReadBytes() == prefix + keep);
                REQUIRE_FALSE(progress.invalid);
                REQUIRE(PatchArchive::Open(file.GetPath()).GetEntries().size() == 1);
                const std::array<wstr_view, 1> remaining = {L"keep"};
                PatchArchive::DeleteEntries(file.GetPath(), remaining);
                REQUIRE(file.ReadBytes() == bom_bytes);
                REQUIRE(PatchArchive::Open(file.GetPath()).GetEntries().empty());
            }
        }
    }
}

TEST_CASE("Mercurial rejects unusable framing and unsafe or ambiguous paths", "[hg][invalid]") {
    const std::array<std::string, 9> invalid = {
        "diff -r old\n", "diff -r old -r new\n", "diff -r old file\n--- a/file\n",
        "diff -r old file\n--- a/file\n+++ b/file\n",
        "diff -r old file\n--- a/file\n+++ b/file\n@@ -1 +1 @@\n-old\n",
        "diff -r old file\nBinary file  has changed\n",
        "diff -r old file\nBinary file file has changed extra\n",
        "# HG changeset patch\n# User Author\n\n" + MakeGitNotice("asset"),
        kHgHeaders + "Only a commit message\n"};
    for(const auto& bytes : invalid) {
        DYNAMIC_SECTION(bytes) { RequireParseError(bytes, ErrorCode::kInvalidPatch); }
    }
    RequireParseError(MakeHgPatch("../outside"), ErrorCode::kInvalidPath);
    RequireParseError(MakeHgNotice("CON"), ErrorCode::kInvalidPath);
    RequireParseError(kHgHeaders + MakeHgPatch("A") + MakeHgNotice("a"), ErrorCode::kInvalidPath);
    RequireParseError(MakeHgPatch("path") + MakeHgPatch("path/child"), ErrorCode::kInvalidPath);
}

TEST_CASE("Mercurial rejects changeset series and mixed bodies before rewriting", "[hg][scope][delete]") {
    const std::array<std::string, 5> unsupported = {
        kHgHeaders + MakeHgPatch("a") + kHgHeaders + MakeHgPatch("b"),
        kHgHeaders + kHgMessage + kHgHeaders + MakeGitPatch("a"),
        MakeHgPatch("a") + "diff -r other -r pair b\nBinary file b has changed\n",
        MakeHgPatch("a") + MakeGitPatch("b"),
        kHgHeaders + MakeGitPatch("a") + MakeHgPatch("b")};
    for(const auto& bytes : unsupported) {
        DYNAMIC_SECTION(bytes) {
            RequireParseError(bytes, ErrorCode::kUnsupportedFeature);
            const TestFile file(bytes);
            const std::array<wstr_view, 1> selected = {L"a"};
            REQUIRE_THROWS_AS(PatchArchive::DeleteEntries(file.GetPath(), selected), PatchError);
            REQUIRE(file.ReadBytes() == bytes);
        }
    }
    const TestFile file(kHgHeaders + MakeHgPatch("a"));
    const std::array<wstr_view, 1> missing = {L"missing"};
    REQUIRE_THROWS_AS(PatchArchive::DeleteEntries(file.GetPath(), missing), PatchError);
    REQUIRE(file.ReadBytes() == kHgHeaders + MakeHgPatch("a"));
}
