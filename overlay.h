#ifndef OVERLAY_H
#define OVERLAY_H

#define SG_H            16          //Sign Generator Height for example
#define SG_W            12          //Sign Generator Widtht for example

#define GLYPH_SYM_SPACE_PX  2 
#define GLYPH_ROW_SPACE_PX  0
#define DEF_WHITE_CLR   0x00007FFF
#define TXT_LINE(x) ((SG_H + GLYPH_ROW_SPACE_PX) * (x))

void sw_overlay_nv12(unsigned char *yuv420, const char *overlay_message, int overlay_len_message, int x_frame_size, int y_frame_size, int x_overlay_offset, int y_overlay_offset);
void sw_overlay_nv12_init();
#endif
