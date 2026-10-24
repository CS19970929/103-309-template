/* Real SysTime_GetMicroseconds is inserted between these stubs and main. */
#include <stdint.h>
#include <assert.h>
#include <stdio.h>
typedef uint32_t UINT32;
static UINT32 s_u32Sys10msTickCount, masked, flag, counters[2], reads;
#define TIM3 3
#define TIM_FLAG_Update 1
#define RESET 0
static UINT32 __get_PRIMASK(void) { return masked; }
static void __disable_irq(void) { masked=1; }
static void __enable_irq(void) { masked=0; }
static UINT32 TIM_GetCounter(int timer) { assert(masked); return counters[reads++ ? 1 : 0]; }
static UINT32 TIM_GetFlagStatus(int timer,int bit) { return flag; }
/* REAL_CLOCK_FUNCTION */
int main(void)
{
    s_u32Sys10msTickCount=1; counters[0]=17;
    assert(SysTime_GetMicroseconds()==10170 && !masked);
    flag=1; reads=0; counters[0]=999; counters[1]=2;
    assert(SysTime_GetMicroseconds()==20020 && !masked);
    reads=0; masked=1;
    assert(SysTime_GetMicroseconds()==20020 && masked);
    masked=0; flag=0; reads=0; s_u32Sys10msTickCount=UINT32_MAX; counters[0]=999;
    assert(SysTime_GetMicroseconds()==(UINT32)(UINT32_MAX*10000U+9990U));
    puts("PASS TIM3 microseconds: pending overflow, re-read after overflow, interrupt-mask preservation, 32-bit wrap");
    return 0;
}
