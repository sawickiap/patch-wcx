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

#pragma once

#include <patch_backend/precompiled_header.hpp>
#include <cstring>
#include <new>

#include <wcxhead.h>

// The x86 module definition file exports undecorated aliases for the stdcall symbols.
#ifdef _WIN64
#define PATCH_WCX_EXPORT extern "C" __declspec(dllexport)
#else
#define PATCH_WCX_EXPORT extern "C"
#endif
