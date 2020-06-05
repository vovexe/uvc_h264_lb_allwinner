#include <string.h> 
#include <stdlib.h>
#include "overlay.h"

#define SG_H            16          //Sign Generator Height for example
#define SG_W            12          //Sign Generator Widtht for example

#define SG_size         SG_H * SG_W

#define Y_black         0x00
#define U_black         0x80
#define V_black         0x80
#define UV_black        0x80

#define Y_white         0xFF
#define U_white         0x80
#define V_white         0x80

#define UV_white_black  U_black     //= V_black, U_white, V_white - excelent!!!

#define S               Y_white
#define B               Y_black


/*
//Recomended align this massive to 8 (or 4)-byte address
const unsigned char SG_Y_table[3*SG_size] =         //test SG-table has only 3 sign. Full table should have 256 ASCII signs, i.e. table size should be 256*SG_size bytes. It's necessary generate it next time
{
//...
    B,B,B,B,B,B,B,B,B,B,B,B,
    B,B,B,B,B,B,B,B,B,B,B,B,
    B,B,B,B,S,S,S,S,B,B,B,B,
    B,B,B,B,S,S,S,S,B,B,B,B,
    B,B,S,S,B,B,B,B,S,S,B,B,
    B,B,S,S,B,B,B,B,S,S,B,B,
    B,B,S,S,B,B,B,B,S,S,B,B,
    B,B,S,S,B,B,B,B,S,S,B,B,
    B,B,S,S,B,B,B,B,S,S,B,B,
    B,B,S,S,B,B,B,B,S,S,B,B,
    B,B,S,S,B,B,B,B,S,S,B,B,
    B,B,S,S,B,B,B,B,S,S,B,B,
    B,B,B,B,S,S,S,S,B,B,B,B,
    B,B,B,B,S,S,S,S,B,B,B,B,
    B,B,B,B,B,B,B,B,B,B,B,B,
    B,B,B,B,B,B,B,B,B,B,B,B,

    B,B,B,B,B,B,B,B,B,B,B,B,
    B,B,B,B,B,B,B,B,B,B,B,B,
    B,B,B,B,B,B,S,S,B,B,B,B,
    B,B,B,B,B,B,S,S,B,B,B,B,
    B,B,B,B,S,S,S,S,B,B,B,B,
    B,B,B,B,S,S,S,S,B,B,B,B,
    B,B,S,S,B,B,S,S,B,B,B,B,
    B,B,S,S,B,B,S,S,B,B,B,B,
    B,B,B,B,B,B,S,S,B,B,B,B,
    B,B,B,B,B,B,S,S,B,B,B,B,
    B,B,B,B,B,B,S,S,B,B,B,B,
    B,B,B,B,B,B,S,S,B,B,B,B,
    B,B,B,B,S,S,S,S,S,S,B,B,
    B,B,B,B,S,S,S,S,S,S,B,B,
    B,B,B,B,B,B,B,B,B,B,B,B,
    B,B,B,B,B,B,B,B,B,B,B,B,
//...
    B,B,B,B,B,B,B,B,B,B,B,B,
    B,B,B,B,B,B,B,B,B,B,B,B,
    B,B,B,B,S,S,S,S,B,B,B,B,
    B,B,B,B,S,S,S,S,B,B,B,B,
    B,B,B,B,S,S,S,S,B,B,B,B,
    B,B,B,B,S,S,S,S,B,B,B,B,
    B,B,B,B,B,B,B,B,B,B,B,B,
    B,B,B,B,B,B,B,B,B,B,B,B,
    B,B,B,B,B,B,B,B,B,B,B,B,
    B,B,B,B,B,B,B,B,B,B,B,B,
    B,B,B,B,S,S,S,S,B,B,B,B,
    B,B,B,B,S,S,S,S,B,B,B,B,
    B,B,B,B,S,S,S,S,B,B,B,B,
    B,B,B,B,S,S,S,S,B,B,B,B,
    B,B,B,B,B,B,B,B,B,B,B,B,
    B,B,B,B,B,B,B,B,B,B,B,B
//...   
};
*/

//[33:126]
unsigned int AML_table[(126-33+1)*16] =
{
    0,0,0,0,8,8,8,8,8,8,0,8,0,0,0,0,
    0,0,0,0,20,20,20,0,0,0,0,0,0,0,0,0,
    0,0,0,0,80,80,252,40,40,252,40,40,0,0,0,0,
    0,0,0,16,56,84,20,56,80,84,56,16,0,0,0,0,
    0,0,0,0,272,296,168,144,576,1344,1312,544,0,0,0,0,
    0,0,0,0,48,72,72,48,296,196,196,312,0,0,0,0,
    0,0,0,0,4,4,4,0,0,0,0,0,0,0,0,0,
    0,0,0,0,16,8,8,4,4,4,4,8,8,16,0,0,
    0,0,0,0,4,8,8,16,16,16,16,8,8,4,0,0,
    0,0,0,0,0,84,56,84,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,16,16,124,16,16,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,8,8,4,0,0,
    0,0,0,0,0,0,0,0,0,28,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,8,0,0,0,0,
    0,0,0,0,16,16,16,8,8,8,4,4,4,0,0,0,
    0,0,0,0,56,68,68,68,68,68,68,56,0,0,0,0,
    0,0,0,0,32,48,40,32,32,32,32,32,0,0,0,0,
    0,0,0,0,56,68,64,32,16,8,4,124,0,0,0,0,
    0,0,0,0,56,68,64,48,64,64,68,56,0,0,0,0,
    0,0,0,0,64,96,80,72,68,252,64,64,0,0,0,0,
    0,0,0,0,124,4,4,60,64,64,68,56,0,0,0,0,
    0,0,0,0,56,68,4,60,68,68,68,56,0,0,0,0,
    0,0,0,0,124,64,32,32,16,16,8,8,0,0,0,0,
    0,0,0,0,56,68,68,56,68,68,68,56,0,0,0,0,
    0,0,0,0,56,68,68,68,120,64,64,56,0,0,0,0,
    0,0,0,0,0,0,0,8,0,0,0,8,0,0,0,0,
    0,0,0,0,0,0,0,8,0,0,0,8,8,4,0,0,
    0,0,0,0,0,0,192,48,12,48,192,0,0,0,0,0,
    0,0,0,0,0,0,0,124,0,124,0,0,0,0,0,0,
    0,0,0,0,0,0,12,48,192,48,12,0,0,0,0,0,
    0,0,0,0,56,68,68,32,16,16,0,16,0,0,0,0,
    0,0,0,0,0,480,528,1480,1320,1320,968,1040,992,0,0,0,
    0,0,0,0,32,32,80,80,136,248,260,260,0,0,0,0,
    0,0,0,0,120,136,136,120,136,136,136,120,0,0,0,0,
    0,0,0,0,240,264,4,4,4,4,264,240,0,0,0,0,
    0,0,0,0,120,136,264,264,264,264,136,120,0,0,0,0,
    0,0,0,0,248,8,8,120,8,8,8,248,0,0,0,0,
    0,0,0,0,248,8,8,120,8,8,8,8,0,0,0,0,
    0,0,0,0,240,264,4,4,452,260,264,240,0,0,0,0,
    0,0,0,0,264,264,264,504,264,264,264,264,0,0,0,0,
    0,0,0,0,8,8,8,8,8,8,8,8,0,0,0,0,
    0,0,0,0,32,32,32,32,32,32,36,24,0,0,0,0,
    0,0,0,0,136,72,40,24,40,72,136,264,0,0,0,0,
    0,0,0,0,8,8,8,8,8,8,8,248,0,0,0,0,
    0,0,0,0,520,792,792,680,680,584,584,520,0,0,0,0,
    0,0,0,0,136,152,152,168,168,200,200,136,0,0,0,0,
    0,0,0,0,112,136,260,260,260,260,136,112,0,0,0,0,
    0,0,0,0,120,136,136,136,120,8,8,8,0,0,0,0,
    0,0,0,0,112,136,260,260,260,260,200,496,0,0,0,0,
    0,0,0,0,248,264,264,264,248,72,136,264,0,0,0,0,
    0,0,0,0,112,136,8,48,64,128,136,112,0,0,0,0,
    0,0,0,0,124,16,16,16,16,16,16,16,0,0,0,0,
    0,0,0,0,264,264,264,264,264,264,264,240,0,0,0,0,
    0,0,0,0,260,260,136,136,80,80,32,32,0,0,0,0,
    0,0,0,0,1092,1092,1092,680,680,680,272,272,0,0,0,0,
    0,0,0,0,136,80,80,32,32,80,80,136,0,0,0,0,
    0,0,0,0,68,68,40,40,16,16,16,16,0,0,0,0,
    0,0,0,0,252,128,64,32,16,8,4,252,0,0,0,0,
    0,0,0,0,24,8,8,8,8,8,8,8,8,24,0,0,
    0,0,0,0,4,4,4,8,8,8,16,16,16,0,0,0,
    0,0,0,0,12,8,8,8,8,8,8,8,8,12,0,0,
    0,0,0,0,16,40,40,68,68,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,252,0,0,
    0,0,0,0,4,8,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,56,68,120,68,120,0,0,0,0,
    0,0,0,0,8,8,8,120,136,136,136,120,0,0,0,0,
    0,0,0,0,0,0,0,56,68,4,68,56,0,0,0,0,
    0,0,0,0,64,64,64,120,68,68,68,120,0,0,0,0,
    0,0,0,0,0,0,0,56,68,124,4,120,0,0,0,0,
    0,0,0,0,16,8,8,28,8,8,8,8,0,0,0,0,
    0,0,0,0,0,0,0,120,68,68,68,120,64,56,0,0,
    0,0,0,0,8,8,8,120,136,136,136,136,0,0,0,0,
    0,0,0,0,0,8,0,8,8,8,8,8,0,0,0,0,
    0,0,0,0,0,8,0,8,8,8,8,8,8,4,0,0,
    0,0,0,0,8,8,8,72,40,24,40,72,0,0,0,0,
    0,0,0,0,8,8,8,8,8,8,8,8,0,0,0,0,
    0,0,0,0,0,0,0,440,584,584,584,584,0,0,0,0,
    0,0,0,0,0,0,0,120,136,136,136,136,0,0,0,0,
    0,0,0,0,0,0,0,56,68,68,68,56,0,0,0,0,
    0,0,0,0,0,0,0,120,136,136,136,120,8,8,0,0,
    0,0,0,0,0,0,0,120,68,68,68,120,64,64,0,0,
    0,0,0,0,0,0,0,104,24,8,8,8,0,0,0,0,
    0,0,0,0,0,0,0,56,4,24,32,28,0,0,0,0,
    0,0,0,0,0,8,8,28,8,8,8,16,0,0,0,0,
    0,0,0,0,0,0,0,136,136,136,136,240,0,0,0,0,
    0,0,0,0,0,0,0,68,68,40,40,16,0,0,0,0,
    0,0,0,0,0,0,0,292,292,340,136,136,0,0,0,0,
    0,0,0,0,0,0,0,68,40,16,40,68,0,0,0,0,
    0,0,0,0,0,0,0,68,68,40,40,16,16,12,0,0,
    0,0,0,0,0,0,0,124,32,16,8,124,0,0,0,0,
    0,0,0,0,16,8,8,8,8,4,8,8,8,8,16,0,
    0,0,0,0,8,8,8,8,8,8,8,8,8,8,0,0,
    0,0,0,0,4,8,8,8,8,16,8,8,8,8,4,0,
    0,0,0,0,0,0,0,80,40,0,0,0,0,0,0,0
};

unsigned char SG_Y_table[256*SG_size];

void generate_table(unsigned int *source, unsigned char *destination)
{
    int i, x, y, index = 0;
    
    for(i = 0; i < 33*SG_size; i++)                     //symbols [0:32] is space
        *destination++ = B;
    
    for(i = 33; i < 127; i++)
        for(y = 0; y < SG_H; y++, index++)              //symbols [33:126]
            for(x = 0; x < SG_W; x++)                   //bits [0:11] used, [12:16] - no need
                *destination++ = (source[index] & (1 << x)) ? S : B;
    
    for(i = 127*SG_size; i < 256*SG_size; i++)          //symbols [127:255] is space
        *destination++ = B;
}


void sw_overlay_nv12(unsigned char *yuv420, const char *overlay_message, int overlay_len_message, int x_frame_size, int y_frame_size, int x_overlay_offset, int y_overlay_offset)
{
    unsigned char *offset = yuv420 + y_overlay_offset*x_frame_size + x_overlay_offset;
    int i, y, x;
    int offset_string_overlay = 0;
    int x_frame_size_div_2 = x_frame_size / 2;

//Y-components overlay
    for(y = 0; y < SG_H; y++)
    {
        x = 0;
        for(i = 0; i < overlay_len_message; i++)
        {
            memcpy(offset + x, SG_Y_table + (overlay_message[i]*SG_size) + offset_string_overlay, SG_W);    //copy horisontal SG line of sign
            x += SG_W;
        }
        offset_string_overlay += SG_W;
        offset += x_frame_size;
    }

//UV-components overlay NV12
#if 1
    offset = yuv420 + (x_frame_size * y_frame_size) + (y_overlay_offset*x_frame_size/4) + x_overlay_offset/2;
    x = SG_W*overlay_len_message;
    for(y = 0; y < SG_H/2; y++)
    {
        memset(offset, UV_black, x);                                                                         //set horisontal full line of all message
        offset += x_frame_size;
    }
#else    
//U-components overlay      
    offset = yuv420 + (x_frame_size * y_frame_size) + (y_overlay_offset*x_frame_size/4) + x_overlay_offset/2;
    x = (SG_W/2)*overlay_len_message;
    for(y = 0; y < SG_H/2; y++)
    {
        memset(offset, U_black, x);                                                                         //set horisontal full line of all message
        offset += x_frame_size_div_2;
    }
    
//V-components overlay      
    offset = yuv420 + (x_frame_size * y_frame_size) + (x_frame_size * y_frame_size)/4 + (y_overlay_offset*x_frame_size/4) + x_overlay_offset/2;
    x = (SG_W/2)*overlay_len_message;
    for(y = 0; y < SG_H/2; y++)
    {
        memset(offset, V_black, x);                                                                         //set horisontal full line of all message
        offset += x_frame_size_div_2;
    }
#endif    
}

void sw_overlay_nv12_init() {
    generate_table(AML_table, SG_Y_table);
}
