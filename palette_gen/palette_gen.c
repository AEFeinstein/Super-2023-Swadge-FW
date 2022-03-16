#include <stdint.h>
#include <stdio.h>

#define SWAP(x) (((x>>8)&(0xFF))|((x<<8)&(0xFF00)))

typedef struct { /*__attribute__((packed))*/
    uint16_t b:5;
    uint16_t a:1; // This is actually the LSB for green when transferring the framebuffer to the TFT
    uint16_t g:5;
    uint16_t r:5;
} rgba_pixel_t;

typedef union {
    rgba_pixel_t px;
    uint16_t val;
} rgba_pixel_disp_t;

int main(int argc, char ** argv)
{

    printf("#ifndef _PALETTE_H_\n");
    printf("#define _PALETTE_H_\n\n");

    printf("#include <stdint.h>\n\n");


    printf("uint16_t paletteColors[216] = \n{\n");
    for(int r = 0; r < 6; r++)
    {
        for(int g = 0; g < 6; g++)
        {
            for(int b = 0; b < 6; b++)
            {
                rgba_pixel_disp_t px =
                {
                    .px.r = (r * 0x1F) / 5,
                    .px.g = (g * 0x1F) / 5,
                    .px.b = (b * 0x1F) / 5,
                    .px.a = 0,
                };
                printf("    0x%04X,\n", SWAP(px.val));
            }
        }
    }
    printf("};\n\n");

    printf("uint32_t paletteColorsEmu[216] = \n{\n");
    for(int r = 0; r < 6; r++)
    {
        for(int g = 0; g < 6; g++)
        {
            for(int b = 0; b < 6; b++)
            {
                uint32_t px = (((r * 0xFF) / 5) << 24) | (((g * 0xFF) / 5) << 16) | (((b * 0xFF) / 5) << 8) | 0xFF;
                printf("    0x%08X,\n", px);
            }
        }
    }
    printf("};\n\n");

    printf("typedef enum\n{\n");
    for(int r = 0; r < 6; r++)
    {
        for(int g = 0; g < 6; g++)
        {
            for(int b = 0; b < 6; b++)
            {
                printf("    c%d%d%d,\n", r, g, b);
            }
        }
    }
    printf("} paletteColor_t;\n\n");

    printf("#endif");
    return 0;
}