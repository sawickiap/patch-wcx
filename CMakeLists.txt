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

cmake_minimum_required(VERSION 3.31)
project(patch_wcx LANGUAGES CXX RC)

if(NOT WIN32)
    message(FATAL_ERROR "Patch WCX requires Windows x86 or x64.")
endif()
if(CMAKE_VS_PLATFORM_NAME)
    if(NOT ((CMAKE_VS_PLATFORM_NAME STREQUAL "Win32" AND CMAKE_SIZEOF_VOID_P EQUAL 4) OR
            (CMAKE_VS_PLATFORM_NAME STREQUAL "x64" AND CMAKE_SIZEOF_VOID_P EQUAL 8)))
        message(FATAL_ERROR "Patch WCX requires a Win32 or x64 target.")
    endif()
else()
    include(CheckCXXSourceCompiles)
    check_cxx_source_compiles([=[
        #if defined(_M_ARM64EC)
        #error Unsupported target
        #elif defined(_M_IX86) || defined(__i386__)
        static_assert(sizeof(void*) == 4);
        #elif defined(_M_X64) || defined(__x86_64__)
        static_assert(sizeof(void*) == 8);
        #else
        #error Unsupported target
        #endif
        int main() {}
    ]=] PATCH_WCX_X86_OR_X64)
    if(NOT PATCH_WCX_X86_OR_X64)
        message(FATAL_ERROR "Patch WCX requires an x86 or x64 target.")
    endif()
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

include(CTest)
add_subdirectory(src/third_party)
add_subdirectory(src/patch_backend)
add_subdirectory(src/patch_wcx)
if(BUILD_TESTING)
    add_subdirectory(src/tests)
endif()
