/** @file
 *
 *  @author Oliver Adams
 *  @copyright Copyright (C) 2019
 */

#ifndef PLATFORMS_H
#define PLATFORMS_H

/* C89/C99/C11 keywords */
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L
#define C99
#define INLINE inline
#define RESTRICT restrict
#define INLINE_DEFINITION(x) x
#endif

#define C89
#ifndef INLINE
#define INLINE
#endif
#ifndef RESTRICT
#define RESTRICT
#endif
#ifndef INLINE_DEFINITION
#define INLINE_DEFINITION(x) ;
#endif

#ifdef PLATFORMS_CONFIG
#include <platforms_config.h>
#endif

/* Memory management */
#ifndef MALLOC
#define MALLOC malloc
#endif

#ifndef REALLOC
#define REALLOC realloc
#endif

#ifndef CALLOC
#define CALLOC calloc
#endif

#ifndef FREE
#define FREE free
#endif

/* Min/Max */
#ifndef MAX
#define MAX(x, y) ((x) < (y)? (y): (x))
#endif

#ifndef MIN
#define MIN(x, y) ((x) < (y)? (x): (y))
#endif

#ifdef __clang__
#define CLANG_COMPILER 1
#else
#define CLANG_COMPILER 0
#endif

#if defined(__GNUC__) && !CLANG_COMPILER
#define GCC_COMPILER 1
#else
#define GCC_COMPILER 0
#endif

#ifdef __INTEL_COMPILER
#define INTEL_COMPILER 1
#else
#define INTEL_COMPILER 0
#endif

#ifdef _MSC_VER
#define MSVC_COMPILER 1
#else
#define MSVC_COMPILER 0
#endif

#if defined(__linux) || defined(__linux__)
#define LINUX_OS 1
#ifdef CC_INCLUDE_NETWORK
#define INVALID_SOCKET (-1)
#define SOCKET_ERROR (-1)
#define SOCKET int

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#endif /* CC_INCLUDE_NETWORK */
#else
#define LINUX_OS 0
#endif

#if defined(__WIN32) || defined(__WIN64) || defined(WIN32) || defined(WIN64)
#define WINDOWS_OS 1
#ifdef CC_INCLUDE_NETWORK
#include <winsock2.h>
#include <ws2tcpip.h>
#endif /* CC_INCLUDE_NETWORK */
#include <windows.h>
#else
#define WINDOWS_OS 0
#endif

#if (defined(__alpha__) | defined(__alpha) | defined(_M_ALPHA))
#define ALPHA_CPU 1
#else
#define ALPHA_CPU 0
#endif

#if (defined(__amd64__) | defined(__amd64) | defined(__x86_64__) | defined(__x86_64) | defined(_M_AMD64))
#define AMD64_CPU 1
#else
#define AMD64_CPU 0
#endif

#if (defined(__arm__) | defined(__thumb__) | defined(__TARGET_ARCH_ARM) | defined(__TARGET_ARCH_THUMB) | defined(_ARM) | defined(_M_ARM) | defined(_M_ARMT) | defined(__arm))
#define ARM_CPU 1
#else
#define ARM_CPU 0
#endif

#if (defined(__aarch64__))
#define ARM64_CPU 1
#else
#define ARM64_CPU 0
#endif

#if (defined(__bfin) | defined(__BFIN__))
#define BLACKFIN_CPU 1
#else
#define BLACKFIN_CPU 0
#endif

#if (defined(__convex__))
#define CONVEX_CPU 1
#else
#define CONVEX_CPU 0
#endif

#if (defined(__epiphany__))
#define EPIPHANY_CPU 1
#else
#define EPIPHANY_CPU 0
#endif

#if (defined(__hppa__) | defined(__HPPA__) | defined(__hppa))
#define HPPA_RISC_CPU 1
#else
#define HPPA_RISC_CPU 0
#endif

#if (defined(i386) | defined(__i386) | defined(__i386__) | defined(__i486__) | defined(__i586__) | defined(__i686__) | defined(__IA32__) | \
     defined(_M_I86) | defined(_M_IX86) | defined(__X86__) | defined(_X86_) | defined(__THW_INTEL__) | defined(__I86__) | defined(__INTEL__) | defined(__386))
#define X86_CPU 1
#else
#define X86_CPU 0
#endif

#if (defined(__ia64__) | defined(_IA64) | defined(__IA64__) | defined(__ia64) | defined(_M_IA64) | defined(__itanium__))
#define ITANIUM_CPU 1
#else
#define ITANIUM_CPU 0
#endif

#if (defined(__m68k__) | defined(M68000) | defined(__MC68K__))
#define M68K_CPU 1
#else
#define M68K_CPU 0
#endif

#if (defined(__mips__) | defined(mips) | defined(__mips) | defined(__MIPS__))
#define MIPS_CPU 1
#else
#define MIPS_CPU 0
#endif

#if (defined(__powerpc) | defined(__powerpc__) | defined(__powerpc64__) | defined(__POWERPC__) | defined(__ppc__) | defined(__ppc64__) | \
     defined(__PPC__) | defined(__PPC64__) | defined(_ARCH_PPC) | defined(_ARCH_PPC64) | defined(_M_PPC) | defined(_ARCH_PPC) | \
     defined(_ARCH_PPC64) | defined(__PPCGECKO__) | defined(__PPCBROADWAY__) | defined(_XENON) | defined(__ppc))
#define POWERPC_CPU 1
#else
#define POWERPC_CPU 0
#endif

#if (defined(pyr))
#define PYRAMID_CPU 1
#else
#define PYRAMID_CPU 0
#endif

#if (defined(__THW_RS6000) | defined(_IBMR2) | defined(_POWER) | defined(_ARCH_PWR) | defined(_ARCH_PWR2) | defined(_ARCH_PWR3) | defined(_ARCH_PWR4))
#define RS6000_CPU 1
#else
#define RS6000_CPU 0
#endif

#if (defined(__sparc__) | defined(__sparc) | defined(__sparc_v8__) | defined(__sparc_v9__) | defined(__sparc_v8) | defined(__sparc_v9))
#define SPARC_CPU 1
#else
#define SPARC_CPU 0
#endif

#if (defined(__sh__) | defined(__sh1__) | defined(__sh2__) | defined(__sh3__) | defined(__sh4__) | defined(__sh5__))
#define SUPERH_CPU 1
#else
#define SUPERH_CPU 0
#endif

#if (defined(__370__) | defined(__THW_370__) | defined(__s390__) | defined(__s390x__) | defined(__zarch__) | defined(__SYSC_ZARCH__))
#define SYSTEMZ_CPU 1
#else
#define SYSTEMZ_CPU 0
#endif

#if (defined(__TMS470__))
#define TMS470_CPU 1
#else
#define TMS470_CPU 0
#endif

#if X86_CPU | AMD64_CPU
# if MSVC_COMPILER
#  include <intrin.h>
# elif GCC_COMPILER | CLANG_COMPILER
#  include <cpuid.h>
#  include <x86intrin.h>
# endif
#elif ARM_CPU | ARM64_CPU
# include <sys/auxv.h>
# include <asm/hwcap.h>
#endif

#define UNUSED(x) (void) (x);

#endif // PLATFORMS_H
