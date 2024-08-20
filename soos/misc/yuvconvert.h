#pragma once

#include <3ds.h>

/**
 * todo: description
 *
 * This is a reimplementation. Internally it works a little differently
 * from the decompiled code, but it should function the same and
 * produce the same output for a given input.
 *
 * @param screenbuf Pointer to the image buffer.
 * @param width Width, in pixels, of the image. (This is basically always 240.)
 * @param porch scrw = capin.screencapture[scr].framebuf_widthbytesize / bsiz; porch = scrw - 240;
 *              (undefined behavior if this is negative)
 * @param height Height, in pixels, of the image. (For top-screen on n3DS, this is 400.)
 */
void yuvConvertFromRGB565(u32* screenbuf, u32 width, int porch, u32 height);

void yuvConvertFromRGBA4(u32* screenbuf, u32 width, int porch, u32 height);
