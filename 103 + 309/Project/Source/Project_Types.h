#ifndef PROJECT_TYPES_H
#define PROJECT_TYPES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef uint8_t bms_u8;
typedef uint16_t bms_u16;
typedef uint32_t bms_u32;
typedef int8_t bms_i8;
typedef int16_t bms_i16;
typedef int32_t bms_i32;
typedef uint8_t bms_bool_t;

#ifndef BMS_INLINE
#if defined(__CC_ARM)
#define BMS_INLINE static __inline
#else
#define BMS_INLINE static inline
#endif
#endif

#ifndef BMS_FALSE
#define BMS_FALSE ((bms_bool_t)0U)
#endif

#ifndef BMS_TRUE
#define BMS_TRUE ((bms_bool_t)1U)
#endif

#endif
