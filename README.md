# Patch WCX

Patch WCX is a Total Commander packer plugin that opens `.diff` and `.patch` files as virtual archives, letting you browse the affected files and directories or delete them.

- ❓ Do you use [Total Commander](https://www.ghisler.com/) as your favorite file manager?
- ❓ Are you a developer working with large diff/patch files?

With Patch WCX, you can ➡️ open a patch like an archive, ➡️ navigate its directory tree, and ➡️ view, ➡️ extract, or ➡️ delete just the changes for individual files you need.

**Version 1.0** — see the [changelog](CHANGELOG.md).

Source code and issue tracker: [github.com/sawickiap/patch-wcx](https://github.com/sawickiap/patch-wcx).

## Features

- Browse affected files in their directory hierarchy.
- View individual file changes with Total Commander's viewer and search their patch text.
- Extract selected files or directories as standalone per-file patch documents.
- Delete selected changes from the original patch, including entire directory subtrees.

Virtual files contain patch documents. For example, viewing or extracting `src/main.cpp` gives you the patch for that file, not its reconstructed C++ source. The plugin also doesn't support adding/replacing entries.

## Video

[![Watch video](https://img.youtube.com/vi/7WrxciCwWbY/hqdefault.jpg)](https://www.youtube.com/watch?v=7WrxciCwWbY)

## Requirements

- x64 PC (Intel/AMD)
- Windows 10 or 11
- 64-bit version of Total Commander

## Installation

1. Download **`patch-wcx.zip`** from [GitHub Releases](https://github.com/sawickiap/patch-wcx/releases).
2. Open the ZIP in Total Commander. It triggers the plugin installer.
3. Confirm installation and choose the plugin directory. The package registers the plugin for `.patch` and `.diff` files.

For manual installation, extract the ZIP, then open: Configuration → Options → Plugins → Packer plugins (.WCX) → Configure. Add `patch_wcx.wcx64` for both the `patch` and `diff` extensions.

## Usage

1. Select a `.patch` or `.diff` file and press **Ctrl+PgDn** to enter it as an archive. **Enter** also works when no external file association takes precedence.
2. Browse its files and directories as usual.
3. Press **F3** to view a file's patch, or **F5** to extract selected entries to the other panel.
4. Press **F8** or **Del** to remove selected entries' changes from the original patch file. Deleting a directory removes changes to all files beneath it.

Entries keep the affected filenames, even though their contents are patch text. For a renamed or moved file, the target path is used in the virtual tree. Git's usual `a/` and `b/` prefixes are hidden in the virtual tree; other directory prefixes are preserved.

## Supported formats

- **Unified diff** — including GNU `diff -u` and recursive `diff -ruN` output.
- **GNU-style context diff** — `diff -c` / `diff -rcN`.
- **Git extended patches** — text hunks, renames/copies, mode changes, empty-file operations, binary notices, and encoded `GIT binary patch` sections.
- **Git `format-patch`** — one plain-text mailbox message per input file.
- **Subversion (`svn diff`, including `--git`)** — content/property changes, Git-style metadata, binary notices, and encoded binary patches.
- **Mercurial** — native and Git-mode diffs, plus single-changeset `hg export` files.

### Additional considerations

<details>
<summary>Git patches and binary changes</summary>

Metadata-only changes are supported, as are symlink changes and ordinary submodule-reference hunks. Extracting them preserves the patch text; it does not create links or fetch submodules. Nested submodule diffs and submodule log output are unsupported.

Encoded binary payloads are preserved. Binary-difference notices in Git, SVN, and Mercurial patches are also exposed, but contain no replacement binary data. Extracting `image.png` produces its patch section, not a PNG.

</details>

<details>
<summary>Git email patches: single messages, mailboxes, and MIME</summary>

One plain-text `git format-patch` message is supported. Multi-message mailboxes, multipart messages, attachments, HTML, and base64/quoted-printable bodies are unsupported. Export each commit to a separate patch file when working with a series.

Per-file extraction retains the original mail headers, commit message, and footer, while omitting the commentary/diffstat block between the standalone `---` separator and the patch. Original commit IDs remain provenance; a filtered patch does not recreate the original commit.

</details>

<details>
<summary>Subversion patches and property changes</summary>

Content and property changes for a file are kept together. Property-only sections for known directories are hidden and preserved when visible entries are deleted. If the patch does not establish whether a property-only path is a directory, it appears as a file-like entry.

`svn diff --git` is supported, including metadata-only copies/renames, mode changes, empty-file operations, and encoded binary payloads. The Git-style headers are preserved in extracted patches.

</details>

<details>
<summary>Mercurial exports</summary>

One changeset per export file is supported, with either native unified or Git-mode sections. Multi-changeset exports and mixed native/Git bodies are unsupported. Extraction preserves changeset metadata and the commit message; the original Node ID does not identify the filtered result, so exact changeset recreation is not guaranteed.

</details>

### Unsupported formats

- Multi-message Git mailboxes, multipart/encoded email patches, and multi-changeset Mercurial exports.
- Traditional normal diff, `ed`/forward-`ed` scripts, and RCS scripts.
- Git combined merge diffs, word diffs, and standalone raw/status/stat/summary reports.
- Side-by-side or HTML comparison reports, `diff3` output, conflict-marker files, and conditional source output (`diff -D`).
- Standalone binary deltas such as bsdiff and Xdelta/VCDIFF, and Git/Mercurial bundles.
- JSON Patch.

UTF-16/UTF-32 patches, mixed-format documents, unsafe or Windows-incompatible paths, duplicate paths, and case or file/directory name collisions are also unsupported. UTF-8 is supported, with or without a BOM; paths fall back to the system ANSI code page when needed. Extracting a patch does not certify that it can be applied successfully.

## Building from source

The project uses C++20, CMake, and Visual Studio 2026 with the C++ desktop tools. The supplied Visual Studio preset requires CMake 4.2 or newer. Initialize the Catch2 submodule before building tests.

```powershell
cmake --preset vs2026
cmake --build --preset release
ctest --preset release
```

The plugin is generated at `build/src/patch_wcx/Release/patch_wcx.wcx64`. To build without tests, configure with `cmake --preset vs2026 -DBUILD_TESTING=OFF` and omit the `ctest` command.

## Author and license

Created by **Adam Sawicki** — see homepage/blog: [asawicki.info](https://asawicki.info).

Patch WCX is free software licensed under the **GNU General Public License (GPL), version 3**. See [LICENSE](LICENSE) for the full license text.
