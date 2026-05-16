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

#ifndef UPDNLMT16
#define UPDNLMT16(Var, Max, Min)                                      \
	do                                                                 \
	{                                                                  \
		(Var) = ((Var) >= (Max)) ? (Max) : (Var);                      \
		(Var) = ((Var) <= (Min)) ? (Min) : (Var);                      \
	} while (0)
#endif

#ifndef S2U
#define S2U(x) (*((volatile uint16_t *)(&(x))))
#endif

#ifndef CRC_KEY
#define CRC_KEY 7
#endif

#ifndef I2C_RW_W
#define I2C_RW_W 0
#endif

#ifndef I2C_RW_R
#define I2C_RW_R 1
#endif

#ifndef FALSE
typedef enum _BOOL {
	FALSE = 0,
	TRUE
} BOOL;
#endif

#ifndef DELAYB10MS_0MS
#define DELAYB10MS_0MS  ((uint16_t)0)     /* 0ms */
#define DELAYB10MS_30MS ((uint16_t)3)     /* 30ms */
#define DELAYB10MS_50MS ((uint16_t)5)     /* 50ms */
#define DELAYB10MS_100MS ((uint16_t)10)   /* 100ms */
#define DELAYB10MS_200MS ((uint16_t)20)   /* 200ms */
#define DELAYB10MS_250MS ((uint16_t)25)   /* 250ms */
#define DELAYB10MS_500MS ((uint16_t)50)   /* 500ms */
#define DELAYB10MS_1S   ((uint16_t)100)   /* 1s */
#define DELAYB10MS_1S5  ((uint16_t)150)   /* 1.5s */
#define DELAYB10MS_2S   ((uint16_t)200)   /* 2s */
#define DELAYB10MS_2S5  ((uint16_t)250)   /* 2.5s */
#define DELAYB10MS_3S   ((uint16_t)300)   /* 3s */
#define DELAYB10MS_4S   ((uint16_t)400)   /* 4s */
#define DELAYB10MS_5S   ((uint16_t)500)   /* 5s */
#define DELAYB10MS_10S  ((uint16_t)1000)  /* 10s */
#define DELAYB10MS_30S  ((uint16_t)3000)  /* 30s */
#define DELAYB10MS_2MIN ((uint16_t)12000) /* 2min */
#endif

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
