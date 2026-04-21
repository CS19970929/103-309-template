#include "display_segmap.h"

/*
 * bit0=a, bit1=b, bit2=c, bit3=d, bit4=e, bit5=f, bit6=g
 */
uint8_t display_digit_to_7seg_mask(uint8_t digit)
{
    static const uint8_t s_digit_map[10] =
    {
        0x3Fu, /* 0 => a b c d e f */
        0x06u, /* 1 => b c */
        0x5Bu, /* 2 => a b d e g */
        0x4Fu, /* 3 => a b c d g */
        0x66u, /* 4 => b c f g */
        0x6Du, /* 5 => a c d f g */
        0x7Du, /* 6 => a c d e f g */
        0x07u, /* 7 => a b c */
        0x7Fu, /* 8 => a b c d e f g */
        0x6Fu  /* 9 => a b c d f g */
    };

    if (digit < 10u)
    {
        return s_digit_map[digit];
    }

    return 0u;
}
