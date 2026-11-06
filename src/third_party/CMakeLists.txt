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

add_library(str_view INTERFACE)
target_include_directories(str_view SYSTEM INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/str_view")

add_library(total_commander INTERFACE)
target_include_directories(total_commander SYSTEM INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/total_commander")

if(BUILD_TESTING)
    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Catch2/CMakeLists.txt")
        message(FATAL_ERROR "Initialize src/third_party/Catch2, or configure with BUILD_TESTING=OFF.")
    endif()
    add_subdirectory(Catch2 EXCLUDE_FROM_ALL)
endif()
