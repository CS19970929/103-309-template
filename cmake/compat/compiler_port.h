#ifndef COMPILER_PORT_H
#define COMPILER_PORT_H

#if defined(__GNUC__)
#define COMPILER_WEAK __attribute__((weak))
#define COMPILER_PACKED __attribute__((packed))
#define COMPILER_ALIGNED(x) __attribute__((aligned(x)))
#define COMPILER_USED __attribute__((used))
#define COMPILER_NAKED __attribute__((naked))
#define COMPILER_SECTION(name) __attribute__((section(name)))
#define COMPILER_NORETURN __attribute__((noreturn))
#elif defined(__CC_ARM)
#define COMPILER_WEAK __weak
#define COMPILER_PACKED __packed
#define COMPILER_ALIGNED(x) __align(x)
#define COMPILER_USED
#define COMPILER_NAKED __asm
#define COMPILER_SECTION(name)
#define COMPILER_NORETURN
#else
#define COMPILER_WEAK
#define COMPILER_PACKED
#define COMPILER_ALIGNED(x)
#define COMPILER_USED
#define COMPILER_NAKED
#define COMPILER_SECTION(name)
#define COMPILER_NORETURN
#endif

#endif
