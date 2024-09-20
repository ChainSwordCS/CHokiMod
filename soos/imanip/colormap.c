#include "colormap.h"
#include <3ds.h>

int convertRawToColorMappedTga(void* buffer, u32 bufferMaxSize, u32 imgDim, void* *color_map_data, u16 *color_map_length)
{
    u16* imageBuffer = (u16*)buffer;

    /* write colors to the palette buffer starting from the end and going backwards.
     * this shouldn't really ever reach 16bpp image data, i think.
     * but just in case, this strategy is to minimize data corruption.
     */
    //u16* paletteBuffer = (u16*)(buffer+imgDim*2);
    const u32 paletteLengthMax = 255;
    u32 paletteLength = 0; // measured in colors / pixels
    // for each color added, we move the start of the buffer backwards.
    u16* paletteBuffer = (u16*)(buffer+bufferMaxSize);
    for(u32 i = 0; i < imgDim; i++)
    {
        u16 curPixelColor = imageBuffer[i];
        u32 j = 0;
        while(j < paletteLength)
        {
            if(paletteBuffer[j] == curPixelColor)
                break;
            j++;
        }
        /* if we read through the whole palette without finding the color,
         * we need to add that color to the palette. */
        if(j == paletteLength)
        {
            if(paletteLength == paletteLengthMax)
                return -1;
            paletteLength++;
            paletteBuffer = (u16*)((u32)paletteBuffer - 2);
            paletteBuffer[0] = curPixelColor;
        }
    }

    /* then, convert the image. */
    u8* newImageBuffer = (u8*)buffer;
    for(u32 i = 0; i < imgDim; i++)
    {
        u16 curPixelColor = imageBuffer[i];
        for(u32 j = 0; j < paletteLength; j++)
        {
            if(paletteBuffer[j] == curPixelColor)
            {
                newImageBuffer[i] = (u8)j;
                break;
            }
        }
    }

    *color_map_data = (void*)paletteBuffer;
    *color_map_length = (u16)paletteLength;

    return 0;
}
