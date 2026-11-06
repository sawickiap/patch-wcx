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

add_executable(tests test_main.cpp patch_archive_tests.cpp)
target_link_libraries(tests PRIVATE patch_backend Catch2::Catch2)
target_precompile_headers(tests PRIVATE "${PROJECT_SOURCE_DIR}/src/patch_backend/precompiled_header.hpp")
target_compile_definitions(tests PRIVATE
    NOMINMAX WIN32_LEAN_AND_MEAN
    PATCH_TEST_EXAMPLES_DIR="${PROJECT_SOURCE_DIR}/examples"
)
if(MSVC)
    target_compile_options(tests PRIVATE /W4 /utf-8 /permissive-)
endif()

include("${PROJECT_SOURCE_DIR}/src/third_party/Catch2/extras/Catch.cmake")
catch_discover_tests(tests)
