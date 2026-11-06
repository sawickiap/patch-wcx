#[[
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
]]

add_library(patch_backend STATIC
    patch_archive.hpp patch_archive.cpp
    write_progress.hpp
    patch_error.hpp patch_error.cpp
    text_buffer.hpp text_buffer.cpp
    file_io.hpp file_io.cpp
    patch_paths.hpp patch_paths.cpp
    formats/unified_parser.hpp formats/unified_parser.cpp
    formats/context_parser.hpp formats/context_parser.cpp
    formats/git_parser.hpp formats/git_parser.cpp
    formats/mailbox_parser.hpp formats/mailbox_parser.cpp
    formats/svn_parser.hpp formats/svn_parser.cpp
    formats/hg_parser.hpp formats/hg_parser.cpp
    formats/parser_common.hpp formats/parser_common.cpp
    formats/patch_parser.hpp formats/patch_parser.cpp
    precompiled_header.hpp
)
target_include_directories(patch_backend PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/..")
target_link_libraries(patch_backend PUBLIC str_view)
target_compile_definitions(patch_backend PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN STRICT)
target_precompile_headers(patch_backend PRIVATE precompiled_header.hpp)
if(MSVC)
    target_compile_options(patch_backend PRIVATE /W4 /utf-8 /permissive-)
endif()
