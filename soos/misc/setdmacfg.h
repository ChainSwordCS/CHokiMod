/*
    setdmacfg.h - Shortcut functions to adjust DMA Config block.

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


/**
 * DmaConfig struct. Based on documentation by 3dbrew.org and by libctru / devkitPro.
 * Implemented here for ease of use with legacy versions of libctru, for the time being.
 *
 * For more information, see:
 * - https://www.3dbrew.org/wiki/Corelink_DMA_Engines
 * - https://libctru.devkitpro.org/structDmaConfig.html
 * - https://github.com/devkitPro/libctru/blob/master/libctru/include/3ds/svc.h
 *
 * Note: as of 2024.09.12, the libctru docs say it's srcCfg before dstCfg,
 * but 3dbrew.org says it's dstCfg before srcCfg.
 * I believe the libctru docs are incorrect about this, but I'm not certain.
 */
typedef struct {
    s8 channelId;
    s8 endianSwapSize;
    u8 flags;
    const u8 _padding;

    //DmaDeviceConfig_ dstCfg;
    s8 dst_deviceId;
    s8 dst_allowedAlignments;
    s16 dst_burstSize;
    s16 dst_transferSize;
    s16 dst_burstStride;
    s16 dst_transferStride;

    //DmaDeviceConfig_ srcCfg;
    s8 src_deviceId;
    s8 src_allowedAlignments;
    s16 src_burstSize;
    s16 src_transferSize;
    s16 src_burstStride;
    s16 src_transferStride;
} DmaConfig_;

/**
 * unused
 *
 * todo: are transferSize and burstStride erroneously switched around?
 *
 * mini doc:
 *  burstSize: Number of bytes transferred in a burst-loop. Can be 0, in which case the max allowed alignment is used as a unit.
 *  burstStride: Burst loop stride, can be <= 0.
 *  transferSize: Number of bytes transferred in a transfer-loop, which is comprised of one or more burst-loops.
 *  transferStride: Transfer-loop stride, can be <= 0.
 */
typedef struct {
    s8 deviceId; // aka "Peripheral ID"
    s8 allowedAlignments; // aka "Allowed Burst Sizes". Default is 1|2|4|8 (which equals 15)
    s16 burstSize; // aka "Gather Granule Size"
    s16 transferSize; // aka "Scatter Granule Size"
    s16 burstStride; // aka "Gather Stride"
    s16 transferStride; // aka "Scatter Stride"
} DmaDeviceConfig_;

// Initialize DMA Config Block.
// No need to memset before calling.
// But afterwards, please call updateDmaCfg() to finish setting the variables.
void initCustomDmaCfg(u8*);

// Pass variables:
// void* dmacfgblk = Pointer to DMA Config Block
// u8 source_bpp = Bits Per Pixel of the framebuffer we want to read from.
// u8 interlaced = Do we want to output interlaced video? (1 = yes, 0 = no)
// u32 rowstride = Stride, as specified in the CaptureInfo
void updateDmaCfgBpp(u8*,u8,u8,u32);

void updateDmaCfg_GBVC(u8*,u8,u8,u32);
