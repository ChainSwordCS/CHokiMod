//#pragma once
#ifndef SOOS_IMANIP_COLORMAP_H_
#define SOOS_IMANIP_COLORMAP_H_

#include <3ds.h>

/* hard-coded to 16bpp
 *
 * @param buffer ...
 * @param bufferMaxSize Maximum size of the buffer, in bytes.
 * @param imgDim Size of the image, in pixels. For example: 240*400
 * @param color_map_data A void* to be written to when this function finishes without error.
 * @param color_map_length A u16 to be written to when this function finishes without error.
 * @return 0 on success. -1 on error, such as if the would-be color palette
 *         is greater than 255 colors in length.
 */
int convertRawToColorMappedTga(void* buffer, u32 bufferMaxSize, u32 imgDim, void* *color_map_data, u16 *color_map_length);

#endif /* SOOS_IMANIP_COLORMAP_H_ */
