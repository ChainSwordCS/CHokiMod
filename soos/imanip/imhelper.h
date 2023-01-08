/*
	imhelper.h - Image Manipulation helper functions

	Part of ChirunoMod - A utility background process for the Nintendo 3DS,
	purpose-built for screen-streaming over WiFi.

	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once

#include <3ds.h>

void cvt1624_help1(u32,u8*,u8**,u8**);
void cvt1624_help2_forrgba4(u8*,u8*);
void cvt1624_help2_forrgb5a1(u8*,u8*);
void cvt1624_help2_forrgb565(u8*,u8*);

void cvt1632i_row1_rgb565(u32,u32*); // Unfinished ?
void cvt1632i_row2_rgb565(u32,u32*); // Unfinished ?
