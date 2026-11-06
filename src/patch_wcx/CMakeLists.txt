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

add_library(patch_wcx SHARED
    entry_points.cpp entry_points_legacy.cpp
    archive_handle.hpp archive_handle.cpp
    error_translation.hpp error_translation.cpp
    precompiled_header.hpp
    patch_wcx.rc
)
target_link_libraries(patch_wcx PRIVATE patch_backend total_commander)
target_compile_definitions(patch_wcx PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN STRICT UNICODE _UNICODE)
target_precompile_headers(patch_wcx PRIVATE precompiled_header.hpp)
set_target_properties(patch_wcx PROPERTIES PREFIX "" SUFFIX ".wcx64")
if(MSVC)
    target_compile_options(patch_wcx PRIVATE /W4 /utf-8 /permissive-)
endif()
