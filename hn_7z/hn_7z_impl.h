/* hn_7z_impl.h -- 7z decoder amalgamation + Hyper Neiva wrapper
   7z SDK by Igor Pavlov, public domain. Included from miniz.c to
   keep build.bat object list unchanged. */
#ifndef HN7Z_IMPL_H
#define HN7Z_IMPL_H
/* ==== 7zAlloc.c ==== */
/* 7zAlloc.c -- Allocation functions for 7z processing
2023-03-04 : Igor Pavlov : Public domain */

#include "Precomp.h"

#include <stdlib.h>

#include "7zAlloc.h"

/* #define SZ_ALLOC_DEBUG */
/* use SZ_ALLOC_DEBUG to debug alloc/free operations */

#ifdef SZ_ALLOC_DEBUG

/*
#ifdef _WIN32
#include "7zWindows.h"
#endif
*/

#include <stdio.h>
static int g_allocCount = 0;
static int g_allocCountTemp = 0;

static void Print_Alloc(const char *s, size_t size, int *counter)
{
  const unsigned size2 = (unsigned)size;
  fprintf(stderr, "\n%s count = %10d : %10u bytes; ", s, *counter, size2);
  (*counter)++;
}
static void Print_Free(const char *s, int *counter)
{
  (*counter)--;
  fprintf(stderr, "\n%s count = %10d", s, *counter);
}
#endif

void *SzAlloc(ISzAllocPtr p, size_t size)
{
  UNUSED_VAR(p)
  if (size == 0)
    return 0;
  #ifdef SZ_ALLOC_DEBUG
  Print_Alloc("Alloc", size, &g_allocCount);
  #endif
  return malloc(size);
}

void SzFree(ISzAllocPtr p, void *address)
{
  UNUSED_VAR(p)
  #ifdef SZ_ALLOC_DEBUG
  if (address)
    Print_Free("Free ", &g_allocCount);
  #endif
  free(address);
}

void *SzAllocTemp(ISzAllocPtr p, size_t size)
{
  UNUSED_VAR(p)
  if (size == 0)
    return 0;
  #ifdef SZ_ALLOC_DEBUG
  Print_Alloc("Alloc_temp", size, &g_allocCountTemp);
  /*
  #ifdef _WIN32
  return HeapAlloc(GetProcessHeap(), 0, size);
  #endif
  */
  #endif
  return malloc(size);
}

void SzFreeTemp(ISzAllocPtr p, void *address)
{
  UNUSED_VAR(p)
  #ifdef SZ_ALLOC_DEBUG
  if (address)
    Print_Free("Free_temp ", &g_allocCountTemp);
  /*
  #ifdef _WIN32
  HeapFree(GetProcessHeap(), 0, address);
  return;
  #endif
  */
  #endif
  free(address);
}

/* ==== 7zBuf.c ==== */
/* 7zBuf.c -- Byte Buffer
2017-04-03 : Igor Pavlov : Public domain */

#include "Precomp.h"

#include "7zBuf.h"

void Buf_Init(CBuf *p)
{
  p->data = 0;
  p->size = 0;
}

int Buf_Create(CBuf *p, size_t size, ISzAllocPtr alloc)
{
  p->size = 0;
  if (size == 0)
  {
    p->data = 0;
    return 1;
  }
  p->data = (Byte *)ISzAlloc_Alloc(alloc, size);
  if (p->data)
  {
    p->size = size;
    return 1;
  }
  return 0;
}

void Buf_Free(CBuf *p, ISzAllocPtr alloc)
{
  ISzAlloc_Free(alloc, p->data);
  p->data = 0;
  p->size = 0;
}

/* ==== 7zCrc.c ==== */
/* 7zCrc.c -- CRC32 calculation and init
2024-03-01 : Igor Pavlov : Public domain */

#include "Precomp.h"

#include "7zCrc.h"
#include "CpuArch.h"

// for debug:
// #define __ARM_FEATURE_CRC32 1

#ifdef __ARM_FEATURE_CRC32
// #pragma message("__ARM_FEATURE_CRC32")
#define Z7_CRC_HW_FORCE
#endif

// #define Z7_CRC_DEBUG_BE
#ifdef Z7_CRC_DEBUG_BE
#undef MY_CPU_LE
#define MY_CPU_BE
#endif

#ifdef Z7_CRC_HW_FORCE
  #define Z7_CRC_NUM_TABLES_USE  1
#else
#ifdef Z7_CRC_NUM_TABLES
  #define Z7_CRC_NUM_TABLES_USE  Z7_CRC_NUM_TABLES
#else
  #define Z7_CRC_NUM_TABLES_USE  12
#endif
#endif

#if Z7_CRC_NUM_TABLES_USE < 1
  #error Stop_Compiling_Bad_Z7_CRC_NUM_TABLES
#endif

#if defined(MY_CPU_LE) || (Z7_CRC_NUM_TABLES_USE == 1)
  #define Z7_CRC_NUM_TABLES_TOTAL  Z7_CRC_NUM_TABLES_USE
#else
  #define Z7_CRC_NUM_TABLES_TOTAL  (Z7_CRC_NUM_TABLES_USE + 1)
#endif

#ifndef Z7_CRC_HW_FORCE

#if Z7_CRC_NUM_TABLES_USE == 1 \
   || (!defined(MY_CPU_LE) && !defined(MY_CPU_BE))
#define CRC_UPDATE_BYTE_2(crc, b)   (table[((crc) ^ (b)) & 0xFF] ^ ((crc) >> 8))
#define Z7_CRC_UPDATE_T1_FUNC_NAME  CrcUpdateGT1
static UInt32 Z7_FASTCALL Z7_CRC_UPDATE_T1_FUNC_NAME(UInt32 v, const void *data, size_t size)
{
  const UInt32 *table = g_CrcTable;
  const Byte *p = (const Byte *)data;
  const Byte *lim = p + size;
  for (; p != lim; p++)
    v = CRC_UPDATE_BYTE_2(v, *p);
  return v;
}
#endif


#if Z7_CRC_NUM_TABLES_USE != 1
#ifndef MY_CPU_BE
  #define FUNC_NAME_LE_2(s)   CrcUpdateT ## s
  #define FUNC_NAME_LE_1(s)   FUNC_NAME_LE_2(s)
  #define FUNC_NAME_LE        FUNC_NAME_LE_1(Z7_CRC_NUM_TABLES_USE)
  UInt32 Z7_FASTCALL FUNC_NAME_LE (UInt32 v, const void *data, size_t size, const UInt32 *table);
#endif
#ifndef MY_CPU_LE
  #define FUNC_NAME_BE_2(s)   CrcUpdateT1_BeT ## s
  #define FUNC_NAME_BE_1(s)   FUNC_NAME_BE_2(s)
  #define FUNC_NAME_BE        FUNC_NAME_BE_1(Z7_CRC_NUM_TABLES_USE)
  UInt32 Z7_FASTCALL FUNC_NAME_BE (UInt32 v, const void *data, size_t size, const UInt32 *table);
#endif
#endif

#endif // Z7_CRC_HW_FORCE

/* ---------- hardware CRC ---------- */

#ifdef MY_CPU_LE

#if defined(MY_CPU_ARM_OR_ARM64)
// #pragma message("ARM*")

  #if (defined(__clang__) && (__clang_major__ >= 3)) \
     || defined(__GNUC__) && (__GNUC__ >= 6) && defined(MY_CPU_ARM64) \
     || defined(__GNUC__) && (__GNUC__ >= 8)
      #if !defined(__ARM_FEATURE_CRC32)
//        #pragma message("!defined(__ARM_FEATURE_CRC32)")
Z7_DIAGNOSTIC_IGNORE_BEGIN_RESERVED_MACRO_IDENTIFIER
        #define __ARM_FEATURE_CRC32 1
Z7_DIAGNOSTIC_IGNORE_END_RESERVED_MACRO_IDENTIFIER
        #define Z7_ARM_FEATURE_CRC32_WAS_SET
        #if defined(__clang__)
          #if defined(MY_CPU_ARM64)
            #define ATTRIB_CRC __attribute__((__target__("crc")))
          #else
            #define ATTRIB_CRC __attribute__((__target__("armv8-a,crc")))
          #endif
        #else
          #if defined(MY_CPU_ARM64)
#if !defined(Z7_GCC_VERSION) || (Z7_GCC_VERSION >= 60000)
            #define ATTRIB_CRC __attribute__((__target__("+crc")))
#endif
          #else
#if !defined(Z7_GCC_VERSION) || (__GNUC__  >= 8)
#if defined(__ARM_FP) && __GNUC__ >= 8
// for -mfloat-abi=hard: similar to <arm_acle.h>
            #define ATTRIB_CRC __attribute__((__target__("arch=armv8-a+crc+simd")))
#else
            #define ATTRIB_CRC __attribute__((__target__("arch=armv8-a+crc")))
#endif
#endif
          #endif
        #endif
      #endif
      #if defined(__ARM_FEATURE_CRC32)
      // #pragma message("<arm_acle.h>")
/*
arm_acle.h (GGC):
    before Nov 17, 2017:
#ifdef __ARM_FEATURE_CRC32

    Nov 17, 2017: gcc10.0  (gcc 9.2.0) checked"
#if __ARM_ARCH >= 8
#pragma GCC target ("arch=armv8-a+crc")

    Aug 22, 2019: GCC 8.4?, 9.2.1, 10.1:
#ifdef __ARM_FEATURE_CRC32
#ifdef __ARM_FP
#pragma GCC target ("arch=armv8-a+crc+simd")
#else
#pragma GCC target ("arch=armv8-a+crc")
#endif
*/
#if defined(__ARM_ARCH) && __ARM_ARCH < 8
#if defined(Z7_GCC_VERSION) && (__GNUC__ ==   8) && (Z7_GCC_VERSION <  80400) \
 || defined(Z7_GCC_VERSION) && (__GNUC__ ==   9) && (Z7_GCC_VERSION <  90201) \
 || defined(Z7_GCC_VERSION) && (__GNUC__ ==  10) && (Z7_GCC_VERSION < 100100)
Z7_DIAGNOSTIC_IGNORE_BEGIN_RESERVED_MACRO_IDENTIFIER
// #pragma message("#define __ARM_ARCH 8")
#undef  __ARM_ARCH
#define __ARM_ARCH 8
Z7_DIAGNOSTIC_IGNORE_END_RESERVED_MACRO_IDENTIFIER
#endif
#endif
        #define Z7_CRC_HW_USE
        #include <arm_acle.h>
      #endif
  #elif defined(_MSC_VER)
    #if defined(MY_CPU_ARM64)
    #if (_MSC_VER >= 1910)
    #ifdef __clang__
       // #define Z7_CRC_HW_USE
       // #include <arm_acle.h>
    #else
       #define Z7_CRC_HW_USE
       #include <intrin.h>
    #endif
    #endif
    #endif
  #endif

#else // non-ARM*

// #define Z7_CRC_HW_USE // for debug : we can test HW-branch of code
#ifdef Z7_CRC_HW_USE
#include "7zCrcEmu.h"
#endif

#endif // non-ARM*



#if defined(Z7_CRC_HW_USE)

// #pragma message("USE ARM HW CRC")

#ifdef MY_CPU_64BIT
  #define CRC_HW_WORD_TYPE  UInt64
  #define CRC_HW_WORD_FUNC  __crc32d
#else
  #define CRC_HW_WORD_TYPE  UInt32
  #define CRC_HW_WORD_FUNC  __crc32w
#endif

#define CRC_HW_UNROLL_BYTES (sizeof(CRC_HW_WORD_TYPE) * 4)

#ifdef ATTRIB_CRC
  ATTRIB_CRC
#endif
Z7_NO_INLINE
#ifdef Z7_CRC_HW_FORCE
         UInt32 Z7_FASTCALL CrcUpdate
#else
  static UInt32 Z7_FASTCALL CrcUpdate_HW
#endif
    (UInt32 v, const void *data, size_t size)
{
  const Byte *p = (const Byte *)data;
  for (; size != 0 && ((unsigned)(ptrdiff_t)p & (CRC_HW_UNROLL_BYTES - 1)) != 0; size--)
    v = __crc32b(v, *p++);
  if (size >= CRC_HW_UNROLL_BYTES)
  {
    const Byte *lim = p + size;
    size &= CRC_HW_UNROLL_BYTES - 1;
    lim -= size;
    do
    {
      v = CRC_HW_WORD_FUNC(v, *(const CRC_HW_WORD_TYPE *)(const void *)(p));
      v = CRC_HW_WORD_FUNC(v, *(const CRC_HW_WORD_TYPE *)(const void *)(p + sizeof(CRC_HW_WORD_TYPE)));
      p += 2 * sizeof(CRC_HW_WORD_TYPE);
      v = CRC_HW_WORD_FUNC(v, *(const CRC_HW_WORD_TYPE *)(const void *)(p));
      v = CRC_HW_WORD_FUNC(v, *(const CRC_HW_WORD_TYPE *)(const void *)(p + sizeof(CRC_HW_WORD_TYPE)));
      p += 2 * sizeof(CRC_HW_WORD_TYPE);
    }
    while (p != lim);
  }
  
  for (; size != 0; size--)
    v = __crc32b(v, *p++);

  return v;
}

#ifdef Z7_ARM_FEATURE_CRC32_WAS_SET
Z7_DIAGNOSTIC_IGNORE_BEGIN_RESERVED_MACRO_IDENTIFIER
#undef __ARM_FEATURE_CRC32
Z7_DIAGNOSTIC_IGNORE_END_RESERVED_MACRO_IDENTIFIER
#undef Z7_ARM_FEATURE_CRC32_WAS_SET
#endif

#endif // defined(Z7_CRC_HW_USE)
#endif // MY_CPU_LE



#ifndef Z7_CRC_HW_FORCE

#if defined(Z7_CRC_HW_USE) || defined(Z7_CRC_UPDATE_T1_FUNC_NAME)
/*
typedef UInt32 (Z7_FASTCALL *Z7_CRC_UPDATE_WITH_TABLE_FUNC)
    (UInt32 v, const void *data, size_t size, const UInt32 *table);
Z7_CRC_UPDATE_WITH_TABLE_FUNC g_CrcUpdate;
*/
static unsigned g_Crc_Algo;
#if (!defined(MY_CPU_LE) && !defined(MY_CPU_BE))
static unsigned g_Crc_Be;
#endif
#endif // defined(Z7_CRC_HW_USE) || defined(Z7_CRC_UPDATE_T1_FUNC_NAME)



Z7_NO_INLINE
#ifdef Z7_CRC_HW_USE
  static UInt32 Z7_FASTCALL CrcUpdate_Base
#else
         UInt32 Z7_FASTCALL CrcUpdate
#endif
    (UInt32 crc, const void *data, size_t size)
{
#if Z7_CRC_NUM_TABLES_USE == 1
    return Z7_CRC_UPDATE_T1_FUNC_NAME(crc, data, size);
#else // Z7_CRC_NUM_TABLES_USE != 1
#ifdef Z7_CRC_UPDATE_T1_FUNC_NAME
  if (g_Crc_Algo == 1)
    return Z7_CRC_UPDATE_T1_FUNC_NAME(crc, data, size);
#endif

#ifdef MY_CPU_LE
    return FUNC_NAME_LE(crc, data, size, g_CrcTable);
#elif defined(MY_CPU_BE)
    return FUNC_NAME_BE(crc, data, size, g_CrcTable);
#else
  if (g_Crc_Be)
    return FUNC_NAME_BE(crc, data, size, g_CrcTable);
  else
    return FUNC_NAME_LE(crc, data, size, g_CrcTable);
#endif
#endif // Z7_CRC_NUM_TABLES_USE != 1
}


#ifdef Z7_CRC_HW_USE
Z7_NO_INLINE
UInt32 Z7_FASTCALL CrcUpdate(UInt32 crc, const void *data, size_t size)
{
  if (g_Crc_Algo == 0)
    return CrcUpdate_HW(crc, data, size);
  return CrcUpdate_Base(crc, data, size);
}
#endif

#endif // !defined(Z7_CRC_HW_FORCE)



UInt32 Z7_FASTCALL CrcCalc(const void *data, size_t size)
{
  return CrcUpdate(CRC_INIT_VAL, data, size) ^ CRC_INIT_VAL;
}


MY_ALIGN(64)
UInt32 g_CrcTable[256 * Z7_CRC_NUM_TABLES_TOTAL];


void Z7_FASTCALL CrcGenerateTable(void)
{
  UInt32 i;
  for (i = 0; i < 256; i++)
  {
#if defined(Z7_CRC_HW_FORCE)
    g_CrcTable[i] = __crc32b(i, 0);
#else
    #define kCrcPoly 0xEDB88320
    UInt32 r = i;
    unsigned j;
    for (j = 0; j < 8; j++)
      r = (r >> 1) ^ (kCrcPoly & ((UInt32)0 - (r & 1)));
    g_CrcTable[i] = r;
#endif
  }
  for (i = 256; i < 256 * Z7_CRC_NUM_TABLES_USE; i++)
  {
    const UInt32 r = g_CrcTable[(size_t)i - 256];
    g_CrcTable[i] = g_CrcTable[r & 0xFF] ^ (r >> 8);
  }

#if !defined(Z7_CRC_HW_FORCE) && \
    (defined(Z7_CRC_HW_USE) || defined(Z7_CRC_UPDATE_T1_FUNC_NAME) || defined(MY_CPU_BE))

#if Z7_CRC_NUM_TABLES_USE <= 1
    g_Crc_Algo = 1;
#else // Z7_CRC_NUM_TABLES_USE <= 1

#if defined(MY_CPU_LE)
    g_Crc_Algo = Z7_CRC_NUM_TABLES_USE;
#else // !defined(MY_CPU_LE)
  {
#ifndef MY_CPU_BE
    UInt32 k = 0x01020304;
    const Byte *p = (const Byte *)&k;
    if (p[0] == 4 && p[1] == 3)
      g_Crc_Algo = Z7_CRC_NUM_TABLES_USE;
    else if (p[0] != 1 || p[1] != 2)
      g_Crc_Algo = 1;
    else
#endif // MY_CPU_BE
    {
      for (i = 256 * Z7_CRC_NUM_TABLES_TOTAL - 1; i >= 256; i--)
      {
        const UInt32 x = g_CrcTable[(size_t)i - 256];
        g_CrcTable[i] = Z7_BSWAP32(x);
      }
#if defined(Z7_CRC_UPDATE_T1_FUNC_NAME)
      g_Crc_Algo = Z7_CRC_NUM_TABLES_USE;
#endif
#if (!defined(MY_CPU_LE) && !defined(MY_CPU_BE))
      g_Crc_Be = 1;
#endif
    }
  }
#endif  // !defined(MY_CPU_LE)

#ifdef MY_CPU_LE
#ifdef Z7_CRC_HW_USE
  if (CPU_IsSupported_CRC32())
    g_Crc_Algo = 0;
#endif // Z7_CRC_HW_USE
#endif // MY_CPU_LE

#endif // Z7_CRC_NUM_TABLES_USE <= 1
#endif // g_Crc_Algo was declared
}

Z7_CRC_UPDATE_FUNC z7_GetFunc_CrcUpdate(unsigned algo)
{
  if (algo == 0)
    return &CrcUpdate;

#if defined(Z7_CRC_HW_USE)
  if (algo == sizeof(CRC_HW_WORD_TYPE) * 8)
  {
#ifdef Z7_CRC_HW_FORCE
    return &CrcUpdate;
#else
    if (g_Crc_Algo == 0)
      return &CrcUpdate_HW;
#endif
  }
#endif

#ifndef Z7_CRC_HW_FORCE
  if (algo == Z7_CRC_NUM_TABLES_USE)
    return
  #ifdef Z7_CRC_HW_USE
      &CrcUpdate_Base;
  #else
      &CrcUpdate;
  #endif
#endif

  return NULL;
}

#undef kCrcPoly
#undef Z7_CRC_NUM_TABLES_USE
#undef Z7_CRC_NUM_TABLES_TOTAL
#undef CRC_UPDATE_BYTE_2
#undef FUNC_NAME_LE_2
#undef FUNC_NAME_LE_1
#undef FUNC_NAME_LE
#undef FUNC_NAME_BE_2
#undef FUNC_NAME_BE_1
#undef FUNC_NAME_BE

#undef CRC_HW_UNROLL_BYTES
#undef CRC_HW_WORD_FUNC
#undef CRC_HW_WORD_TYPE

/* ==== 7zCrcOpt.c ==== */
/* 7zCrcOpt.c -- CRC32 calculation (optimized functions)
2023-12-07 : Igor Pavlov : Public domain */

#include "Precomp.h"

#include "CpuArch.h"

#if !defined(Z7_CRC_NUM_TABLES) || Z7_CRC_NUM_TABLES > 1

// for debug only : define Z7_CRC_DEBUG_BE to test big-endian code in little-endian cpu
// #define Z7_CRC_DEBUG_BE
#ifdef Z7_CRC_DEBUG_BE
#undef MY_CPU_LE
#define MY_CPU_BE
#endif

// the value Z7_CRC_NUM_TABLES_USE must be defined to same value as in 7zCrc.c
#ifdef Z7_CRC_NUM_TABLES
#define Z7_CRC_NUM_TABLES_USE  Z7_CRC_NUM_TABLES
#else
#define Z7_CRC_NUM_TABLES_USE  12
#endif

#if Z7_CRC_NUM_TABLES_USE % 4     || \
    Z7_CRC_NUM_TABLES_USE < 4 * 1 || \
    Z7_CRC_NUM_TABLES_USE > 4 * 6
  #error Stop_Compiling_Bad_Z7_CRC_NUM_TABLES
#endif


#ifndef MY_CPU_BE

#define CRC_UPDATE_BYTE_2(crc, b)  (table[((crc) ^ (b)) & 0xFF] ^ ((crc) >> 8))

#define Q(n, d) \
    ( (table + ((n) * 4 + 3) * 0x100)[(Byte)(d)] \
    ^ (table + ((n) * 4 + 2) * 0x100)[((d) >> 1 * 8) & 0xFF] \
    ^ (table + ((n) * 4 + 1) * 0x100)[((d) >> 2 * 8) & 0xFF] \
    ^ (table + ((n) * 4 + 0) * 0x100)[((d) >> 3 * 8)] )

#define R(a)  *((const UInt32 *)(const void *)p + (a))

#define CRC_FUNC_PRE_LE2(step) \
UInt32 Z7_FASTCALL CrcUpdateT ## step (UInt32 v, const void *data, size_t size, const UInt32 *table)

#define CRC_FUNC_PRE_LE(step)   \
        CRC_FUNC_PRE_LE2(step); \
        CRC_FUNC_PRE_LE2(step)

CRC_FUNC_PRE_LE(Z7_CRC_NUM_TABLES_USE)
{
  const Byte *p = (const Byte *)data;
  const Byte *lim;
  for (; size && ((unsigned)(ptrdiff_t)p & (7 - (Z7_CRC_NUM_TABLES_USE & 4))) != 0; size--, p++)
    v = CRC_UPDATE_BYTE_2(v, *p);
  lim = p + size;
  if (size >= Z7_CRC_NUM_TABLES_USE)
  {
    lim -= Z7_CRC_NUM_TABLES_USE;
    do
    {
      v ^= R(0);
      {
#if Z7_CRC_NUM_TABLES_USE == 1 * 4
        v = Q(0, v);
#else
#define U2(r, op) \
        { d = R(r);  x op Q(Z7_CRC_NUM_TABLES_USE / 4 - 1 - (r), d); }
        UInt32 d, x;
        U2(1, =)
#if Z7_CRC_NUM_TABLES_USE >= 3 * 4
#define U(r)  U2(r, ^=)
        U(2)
#if Z7_CRC_NUM_TABLES_USE >= 4 * 4
        U(3)
#if Z7_CRC_NUM_TABLES_USE >= 5 * 4
        U(4)
#if Z7_CRC_NUM_TABLES_USE >= 6 * 4
        U(5)
#if Z7_CRC_NUM_TABLES_USE >= 7 * 4
#error Stop_Compiling_Bad_Z7_CRC_NUM_TABLES
#endif
#endif
#endif
#endif
#endif
#undef U
#undef U2
        v = x ^ Q(Z7_CRC_NUM_TABLES_USE / 4 - 1, v);
#endif
      }
      p += Z7_CRC_NUM_TABLES_USE;
    }
    while (p <= lim);
    lim += Z7_CRC_NUM_TABLES_USE;
  }
  for (; p < lim; p++)
    v = CRC_UPDATE_BYTE_2(v, *p);
  return v;
}

#undef CRC_UPDATE_BYTE_2
#undef R
#undef Q
#undef CRC_FUNC_PRE_LE
#undef CRC_FUNC_PRE_LE2

#endif




#ifndef MY_CPU_LE

#define CRC_UPDATE_BYTE_2_BE(crc, b)  (table[((crc) >> 24) ^ (b)] ^ ((crc) << 8))

#define Q(n, d) \
    ( (table + ((n) * 4 + 0) * 0x100)[((d)) & 0xFF] \
    ^ (table + ((n) * 4 + 1) * 0x100)[((d) >> 1 * 8) & 0xFF] \
    ^ (table + ((n) * 4 + 2) * 0x100)[((d) >> 2 * 8) & 0xFF] \
    ^ (table + ((n) * 4 + 3) * 0x100)[((d) >> 3 * 8)] )

#ifdef Z7_CRC_DEBUG_BE
  #define R(a)  GetBe32a((const UInt32 *)(const void *)p + (a))
#else
  #define R(a)         *((const UInt32 *)(const void *)p + (a))
#endif


#define CRC_FUNC_PRE_BE2(step) \
UInt32 Z7_FASTCALL CrcUpdateT1_BeT ## step (UInt32 v, const void *data, size_t size, const UInt32 *table)

#define CRC_FUNC_PRE_BE(step)   \
        CRC_FUNC_PRE_BE2(step); \
        CRC_FUNC_PRE_BE2(step)

CRC_FUNC_PRE_BE(Z7_CRC_NUM_TABLES_USE)
{
  const Byte *p = (const Byte *)data;
  const Byte *lim;
  table += 0x100;
  v = Z7_BSWAP32(v);
  for (; size && ((unsigned)(ptrdiff_t)p & (7 - (Z7_CRC_NUM_TABLES_USE & 4))) != 0; size--, p++)
    v = CRC_UPDATE_BYTE_2_BE(v, *p);
  lim = p + size;
  if (size >= Z7_CRC_NUM_TABLES_USE)
  {
    lim -= Z7_CRC_NUM_TABLES_USE;
    do
    {
      v ^= R(0);
      {
#if Z7_CRC_NUM_TABLES_USE == 1 * 4
        v = Q(0, v);
#else
#define U2(r, op) \
        { d = R(r);  x op Q(Z7_CRC_NUM_TABLES_USE / 4 - 1 - (r), d); }
        UInt32 d, x;
        U2(1, =)
#if Z7_CRC_NUM_TABLES_USE >= 3 * 4
#define U(r)  U2(r, ^=)
        U(2)
#if Z7_CRC_NUM_TABLES_USE >= 4 * 4
        U(3)
#if Z7_CRC_NUM_TABLES_USE >= 5 * 4
        U(4)
#if Z7_CRC_NUM_TABLES_USE >= 6 * 4
        U(5)
#if Z7_CRC_NUM_TABLES_USE >= 7 * 4
#error Stop_Compiling_Bad_Z7_CRC_NUM_TABLES
#endif
#endif
#endif
#endif
#endif
#undef U
#undef U2
        v = x ^ Q(Z7_CRC_NUM_TABLES_USE / 4 - 1, v);
#endif
      }
      p += Z7_CRC_NUM_TABLES_USE;
    }
    while (p <= lim);
    lim += Z7_CRC_NUM_TABLES_USE;
  }
  for (; p < lim; p++)
    v = CRC_UPDATE_BYTE_2_BE(v, *p);
  return Z7_BSWAP32(v);
}

#undef CRC_UPDATE_BYTE_2_BE
#undef R
#undef Q
#undef CRC_FUNC_PRE_BE
#undef CRC_FUNC_PRE_BE2

#endif
#undef Z7_CRC_NUM_TABLES_USE
#endif

/* ==== 7zStream.c ==== */
/* 7zStream.c -- 7z Stream functions
2023-04-02 : Igor Pavlov : Public domain */

#include "Precomp.h"

#include <string.h>

#include "7zTypes.h"


SRes SeqInStream_ReadMax(ISeqInStreamPtr stream, void *buf, size_t *processedSize)
{
  size_t size = *processedSize;
  *processedSize = 0;
  while (size != 0)
  {
    size_t cur = size;
    const SRes res = ISeqInStream_Read(stream, buf, &cur);
    *processedSize += cur;
    buf = (void *)((Byte *)buf + cur);
    size -= cur;
    if (res != SZ_OK)
      return res;
    if (cur == 0)
      return SZ_OK;
  }
  return SZ_OK;
}

/*
SRes SeqInStream_Read2(ISeqInStreamPtr stream, void *buf, size_t size, SRes errorType)
{
  while (size != 0)
  {
    size_t processed = size;
    RINOK(ISeqInStream_Read(stream, buf, &processed))
    if (processed == 0)
      return errorType;
    buf = (void *)((Byte *)buf + processed);
    size -= processed;
  }
  return SZ_OK;
}

SRes SeqInStream_Read(ISeqInStreamPtr stream, void *buf, size_t size)
{
  return SeqInStream_Read2(stream, buf, size, SZ_ERROR_INPUT_EOF);
}
*/


SRes SeqInStream_ReadByte(ISeqInStreamPtr stream, Byte *buf)
{
  size_t processed = 1;
  RINOK(ISeqInStream_Read(stream, buf, &processed))
  return (processed == 1) ? SZ_OK : SZ_ERROR_INPUT_EOF;
}



SRes LookInStream_SeekTo(ILookInStreamPtr stream, UInt64 offset)
{
  Int64 t = (Int64)offset;
  return ILookInStream_Seek(stream, &t, SZ_SEEK_SET);
}

SRes LookInStream_LookRead(ILookInStreamPtr stream, void *buf, size_t *size)
{
  const void *lookBuf;
  if (*size == 0)
    return SZ_OK;
  RINOK(ILookInStream_Look(stream, &lookBuf, size))
  memcpy(buf, lookBuf, *size);
  return ILookInStream_Skip(stream, *size);
}

SRes LookInStream_Read2(ILookInStreamPtr stream, void *buf, size_t size, SRes errorType)
{
  while (size != 0)
  {
    size_t processed = size;
    RINOK(ILookInStream_Read(stream, buf, &processed))
    if (processed == 0)
      return errorType;
    buf = (void *)((Byte *)buf + processed);
    size -= processed;
  }
  return SZ_OK;
}

SRes LookInStream_Read(ILookInStreamPtr stream, void *buf, size_t size)
{
  return LookInStream_Read2(stream, buf, size, SZ_ERROR_INPUT_EOF);
}



#define GET_LookToRead2  Z7_CONTAINER_FROM_VTBL_TO_DECL_VAR_pp_vt_p(CLookToRead2)

static SRes LookToRead2_Look_Lookahead(ILookInStreamPtr pp, const void **buf, size_t *size)
{
  SRes res = SZ_OK;
  GET_LookToRead2
  size_t size2 = p->size - p->pos;
  if (size2 == 0 && *size != 0)
  {
    p->pos = 0;
    p->size = 0;
    size2 = p->bufSize;
    res = ISeekInStream_Read(p->realStream, p->buf, &size2);
    p->size = size2;
  }
  if (*size > size2)
    *size = size2;
  *buf = p->buf + p->pos;
  return res;
}

static SRes LookToRead2_Look_Exact(ILookInStreamPtr pp, const void **buf, size_t *size)
{
  SRes res = SZ_OK;
  GET_LookToRead2
  size_t size2 = p->size - p->pos;
  if (size2 == 0 && *size != 0)
  {
    p->pos = 0;
    p->size = 0;
    if (*size > p->bufSize)
      *size = p->bufSize;
    res = ISeekInStream_Read(p->realStream, p->buf, size);
    size2 = p->size = *size;
  }
  if (*size > size2)
    *size = size2;
  *buf = p->buf + p->pos;
  return res;
}

static SRes LookToRead2_Skip(ILookInStreamPtr pp, size_t offset)
{
  GET_LookToRead2
  p->pos += offset;
  return SZ_OK;
}

static SRes LookToRead2_Read(ILookInStreamPtr pp, void *buf, size_t *size)
{
  GET_LookToRead2
  size_t rem = p->size - p->pos;
  if (rem == 0)
    return ISeekInStream_Read(p->realStream, buf, size);
  if (rem > *size)
    rem = *size;
  memcpy(buf, p->buf + p->pos, rem);
  p->pos += rem;
  *size = rem;
  return SZ_OK;
}

static SRes LookToRead2_Seek(ILookInStreamPtr pp, Int64 *pos, ESzSeek origin)
{
  GET_LookToRead2
  p->pos = p->size = 0;
  return ISeekInStream_Seek(p->realStream, pos, origin);
}

void LookToRead2_CreateVTable(CLookToRead2 *p, int lookahead)
{
  p->vt.Look = lookahead ?
      LookToRead2_Look_Lookahead :
      LookToRead2_Look_Exact;
  p->vt.Skip = LookToRead2_Skip;
  p->vt.Read = LookToRead2_Read;
  p->vt.Seek = LookToRead2_Seek;
}



static SRes SecToLook_Read(ISeqInStreamPtr pp, void *buf, size_t *size)
{
  Z7_CONTAINER_FROM_VTBL_TO_DECL_VAR_pp_vt_p(CSecToLook)
  return LookInStream_LookRead(p->realStream, buf, size);
}

void SecToLook_CreateVTable(CSecToLook *p)
{
  p->vt.Read = SecToLook_Read;
}

static SRes SecToRead_Read(ISeqInStreamPtr pp, void *buf, size_t *size)
{
  Z7_CONTAINER_FROM_VTBL_TO_DECL_VAR_pp_vt_p(CSecToRead)
  return ILookInStream_Read(p->realStream, buf, size);
}

void SecToRead_CreateVTable(CSecToRead *p)
{
  p->vt.Read = SecToRead_Read;
}

/* ==== LzmaDec.c ==== */
/* LzmaDec.c -- LZMA Decoder
: Igor Pavlov : Public domain */

#include "Precomp.h"

#include <string.h>

/* #include "CpuArch.h" */
#include "LzmaDec.h"

// #define kNumTopBits 24
#define kTopValue ((UInt32)1 << 24)

#define kNumBitModelTotalBits 11
#define kBitModelTotal (1 << kNumBitModelTotalBits)

#define RC_INIT_SIZE 5

#ifndef Z7_LZMA_DEC_OPT

#define kNumMoveBits 5
#define NORMALIZE if (range < kTopValue) { range <<= 8; code = (code << 8) | (*buf++); }

#define IF_BIT_0(p) ttt = *(p); NORMALIZE; bound = (range >> kNumBitModelTotalBits) * (UInt32)ttt; if (code < bound)
#define UPDATE_0(p) range = bound; *(p) = (CLzmaProb)(ttt + ((kBitModelTotal - ttt) >> kNumMoveBits));
#define UPDATE_1(p) range -= bound; code -= bound; *(p) = (CLzmaProb)(ttt - (ttt >> kNumMoveBits));
#define GET_BIT2(p, i, A0, A1) IF_BIT_0(p) \
  { UPDATE_0(p)  i = (i + i); A0; } else \
  { UPDATE_1(p)  i = (i + i) + 1; A1; }

#define TREE_GET_BIT(probs, i) { GET_BIT2(probs + i, i, ;, ;); }

#define REV_BIT(p, i, A0, A1) IF_BIT_0(p + i) \
  { UPDATE_0(p + i)  A0; } else \
  { UPDATE_1(p + i)  A1; }
#define REV_BIT_VAR(  p, i, m) REV_BIT(p, i, i += m; m += m, m += m; i += m; )
#define REV_BIT_CONST(p, i, m) REV_BIT(p, i, i += m;       , i += m * 2; )
#define REV_BIT_LAST( p, i, m) REV_BIT(p, i, i -= m        , ; )

#define TREE_DECODE(probs, limit, i) \
  { i = 1; do { TREE_GET_BIT(probs, i); } while (i < limit); i -= limit; }

/* #define Z7_LZMA_SIZE_OPT */

#ifdef Z7_LZMA_SIZE_OPT
#define TREE_6_DECODE(probs, i) TREE_DECODE(probs, (1 << 6), i)
#else
#define TREE_6_DECODE(probs, i) \
  { i = 1; \
  TREE_GET_BIT(probs, i) \
  TREE_GET_BIT(probs, i) \
  TREE_GET_BIT(probs, i) \
  TREE_GET_BIT(probs, i) \
  TREE_GET_BIT(probs, i) \
  TREE_GET_BIT(probs, i) \
  i -= 0x40; }
#endif

#define NORMAL_LITER_DEC TREE_GET_BIT(prob, symbol)
#define MATCHED_LITER_DEC \
  matchByte += matchByte; \
  bit = offs; \
  offs &= matchByte; \
  probLit = prob + (offs + bit + symbol); \
  GET_BIT2(probLit, symbol, offs ^= bit; , ;)

#endif // Z7_LZMA_DEC_OPT


#define NORMALIZE_CHECK if (range < kTopValue) { if (buf >= bufLimit) return DUMMY_INPUT_EOF; range <<= 8; code = (code << 8) | (*buf++); }

#define IF_BIT_0_CHECK(p) ttt = *(p); NORMALIZE_CHECK bound = (range >> kNumBitModelTotalBits) * (UInt32)ttt; if (code < bound)
#define UPDATE_0_CHECK range = bound;
#define UPDATE_1_CHECK range -= bound; code -= bound;
#define GET_BIT2_CHECK(p, i, A0, A1) IF_BIT_0_CHECK(p) \
  { UPDATE_0_CHECK  i = (i + i); A0; } else \
  { UPDATE_1_CHECK  i = (i + i) + 1; A1; }
#define GET_BIT_CHECK(p, i) GET_BIT2_CHECK(p, i, ; , ;)
#define TREE_DECODE_CHECK(probs, limit, i) \
  { i = 1; do { GET_BIT_CHECK(probs + i, i) } while (i < limit); i -= limit; }


#define REV_BIT_CHECK(p, i, m) IF_BIT_0_CHECK(p + i) \
  { UPDATE_0_CHECK  i += m; m += m; } else \
  { UPDATE_1_CHECK  m += m; i += m; }


#define kNumPosBitsMax 4
#define kNumPosStatesMax (1 << kNumPosBitsMax)

#define kLenNumLowBits 3
#define kLenNumLowSymbols (1 << kLenNumLowBits)
#define kLenNumHighBits 8
#define kLenNumHighSymbols (1 << kLenNumHighBits)

#define LenLow 0
#define LenHigh (LenLow + 2 * (kNumPosStatesMax << kLenNumLowBits))
#define kNumLenProbs (LenHigh + kLenNumHighSymbols)

#define LenChoice LenLow
#define LenChoice2 (LenLow + (1 << kLenNumLowBits))

#define kNumStates 12
#define kNumStates2 16
#define kNumLitStates 7

#define kStartPosModelIndex 4
#define kEndPosModelIndex 14
#define kNumFullDistances (1 << (kEndPosModelIndex >> 1))

#define kNumPosSlotBits 6
#define kNumLenToPosStates 4

#define kNumAlignBits 4
#define kAlignTableSize (1 << kNumAlignBits)

#define kMatchMinLen 2
#define kMatchSpecLenStart (kMatchMinLen + kLenNumLowSymbols * 2 + kLenNumHighSymbols)

#define kMatchSpecLen_Error_Data (1 << 9)
#define kMatchSpecLen_Error_Fail (kMatchSpecLen_Error_Data - 1)

/* External ASM code needs same CLzmaProb array layout. So don't change it. */

/* (probs_1664) is faster and better for code size at some platforms */
/*
#ifdef MY_CPU_X86_OR_AMD64
*/
#define kStartOffset 1664
#define GET_PROBS p->probs_1664
/*
#define GET_PROBS p->probs + kStartOffset
#else
#define kStartOffset 0
#define GET_PROBS p->probs
#endif
*/

#define SpecPos (-kStartOffset)
#define IsRep0Long (SpecPos + kNumFullDistances)
#define RepLenCoder (IsRep0Long + (kNumStates2 << kNumPosBitsMax))
#define LenCoder (RepLenCoder + kNumLenProbs)
#define IsMatch (LenCoder + kNumLenProbs)
#define Align (IsMatch + (kNumStates2 << kNumPosBitsMax))
#define IsRep (Align + kAlignTableSize)
#define IsRepG0 (IsRep + kNumStates)
#define IsRepG1 (IsRepG0 + kNumStates)
#define IsRepG2 (IsRepG1 + kNumStates)
#define PosSlot (IsRepG2 + kNumStates)
#define Literal (PosSlot + (kNumLenToPosStates << kNumPosSlotBits))
#define NUM_BASE_PROBS (Literal + kStartOffset)

#if Align != 0 && kStartOffset != 0
  #error Stop_Compiling_Bad_LZMA_kAlign
#endif

#if NUM_BASE_PROBS != 1984
  #error Stop_Compiling_Bad_LZMA_PROBS
#endif


#define LZMA_LIT_SIZE 0x300

#define LzmaProps_GetNumProbs(p) (NUM_BASE_PROBS + ((UInt32)LZMA_LIT_SIZE << ((p)->lc + (p)->lp)))


#define CALC_POS_STATE(processedPos, pbMask) (((processedPos) & (pbMask)) << 4)
#define COMBINED_PS_STATE (posState + state)
#define GET_LEN_STATE (posState)

#define LZMA_DIC_MIN (1 << 12)

/*
p->remainLen : shows status of LZMA decoder:
    < kMatchSpecLenStart  : the number of bytes to be copied with (p->rep0) offset
    = kMatchSpecLenStart  : the LZMA stream was finished with end mark
    = kMatchSpecLenStart + 1  : need init range coder
    = kMatchSpecLenStart + 2  : need init range coder and state
    = kMatchSpecLen_Error_Fail                : Internal Code Failure
    = kMatchSpecLen_Error_Data + [0 ... 273]  : LZMA Data Error
*/

/* ---------- LZMA_DECODE_REAL ---------- */
/*
LzmaDec_DecodeReal_3() can be implemented in external ASM file.
3 - is the code compatibility version of that function for check at link time.
*/

#define LZMA_DECODE_REAL LzmaDec_DecodeReal_3

/*
LZMA_DECODE_REAL()
In:
  RangeCoder is normalized
  if (p->dicPos == limit)
  {
    LzmaDec_TryDummy() was called before to exclude LITERAL and MATCH-REP cases.
    So first symbol can be only MATCH-NON-REP. And if that MATCH-NON-REP symbol
    is not END_OF_PAYALOAD_MARKER, then the function doesn't write any byte to dictionary,
    the function returns SZ_OK, and the caller can use (p->remainLen) and (p->reps[0]) later.
  }

Processing:
  The first LZMA symbol will be decoded in any case.
  All main checks for limits are at the end of main loop,
  It decodes additional LZMA-symbols while (p->buf < bufLimit && dicPos < limit),
  RangeCoder is still without last normalization when (p->buf < bufLimit) is being checked.
  But if (p->buf < bufLimit), the caller provided at least (LZMA_REQUIRED_INPUT_MAX + 1) bytes for
  next iteration  before limit (bufLimit + LZMA_REQUIRED_INPUT_MAX),
  that is enough for worst case LZMA symbol with one additional RangeCoder normalization for one bit.
  So that function never reads bufLimit [LZMA_REQUIRED_INPUT_MAX] byte.

Out:
  RangeCoder is normalized
  Result:
    SZ_OK - OK
      p->remainLen:
        < kMatchSpecLenStart : the number of bytes to be copied with (p->reps[0]) offset
        = kMatchSpecLenStart : the LZMA stream was finished with end mark

    SZ_ERROR_DATA - error, when the MATCH-Symbol refers out of dictionary
      p->remainLen : undefined
      p->reps[*]    : undefined
*/


#ifdef Z7_LZMA_DEC_OPT

int Z7_FASTCALL LZMA_DECODE_REAL(CLzmaDec *p, SizeT limit, const Byte *bufLimit);

#else

static
int Z7_FASTCALL LZMA_DECODE_REAL(CLzmaDec *p, SizeT limit, const Byte *bufLimit)
{
  CLzmaProb *probs = GET_PROBS;
  unsigned state = (unsigned)p->state;
  UInt32 rep0 = p->reps[0], rep1 = p->reps[1], rep2 = p->reps[2], rep3 = p->reps[3];
  unsigned pbMask = ((unsigned)1 << (p->prop.pb)) - 1;
  unsigned lc = p->prop.lc;
  unsigned lpMask = ((unsigned)0x100 << p->prop.lp) - ((unsigned)0x100 >> lc);

  Byte *dic = p->dic;
  SizeT dicBufSize = p->dicBufSize;
  SizeT dicPos = p->dicPos;
  
  UInt32 processedPos = p->processedPos;
  UInt32 checkDicSize = p->checkDicSize;
  unsigned len = 0;

  const Byte *buf = p->buf;
  UInt32 range = p->range;
  UInt32 code = p->code;

  do
  {
    CLzmaProb *prob;
    UInt32 bound;
    unsigned ttt;
    unsigned posState = CALC_POS_STATE(processedPos, pbMask);

    prob = probs + IsMatch + COMBINED_PS_STATE;
    IF_BIT_0(prob)
    {
      unsigned symbol;
      UPDATE_0(prob)
      prob = probs + Literal;
      if (processedPos != 0 || checkDicSize != 0)
        prob += (UInt32)3 * ((((processedPos << 8) + dic[(dicPos == 0 ? dicBufSize : dicPos) - 1]) & lpMask) << lc);
      processedPos++;

      if (state < kNumLitStates)
      {
        state -= (state < 4) ? state : 3;
        symbol = 1;
        #ifdef Z7_LZMA_SIZE_OPT
        do { NORMAL_LITER_DEC } while (symbol < 0x100);
        #else
        NORMAL_LITER_DEC
        NORMAL_LITER_DEC
        NORMAL_LITER_DEC
        NORMAL_LITER_DEC
        NORMAL_LITER_DEC
        NORMAL_LITER_DEC
        NORMAL_LITER_DEC
        NORMAL_LITER_DEC
        #endif
      }
      else
      {
        unsigned matchByte = dic[dicPos - rep0 + (dicPos < rep0 ? dicBufSize : 0)];
        unsigned offs = 0x100;
        state -= (state < 10) ? 3 : 6;
        symbol = 1;
        #ifdef Z7_LZMA_SIZE_OPT
        do
        {
          unsigned bit;
          CLzmaProb *probLit;
          MATCHED_LITER_DEC
        }
        while (symbol < 0x100);
        #else
        {
          unsigned bit;
          CLzmaProb *probLit;
          MATCHED_LITER_DEC
          MATCHED_LITER_DEC
          MATCHED_LITER_DEC
          MATCHED_LITER_DEC
          MATCHED_LITER_DEC
          MATCHED_LITER_DEC
          MATCHED_LITER_DEC
          MATCHED_LITER_DEC
        }
        #endif
      }

      dic[dicPos++] = (Byte)symbol;
      continue;
    }
    
    {
      UPDATE_1(prob)
      prob = probs + IsRep + state;
      IF_BIT_0(prob)
      {
        UPDATE_0(prob)
        state += kNumStates;
        prob = probs + LenCoder;
      }
      else
      {
        UPDATE_1(prob)
        prob = probs + IsRepG0 + state;
        IF_BIT_0(prob)
        {
          UPDATE_0(prob)
          prob = probs + IsRep0Long + COMBINED_PS_STATE;
          IF_BIT_0(prob)
          {
            UPDATE_0(prob)
  
            // that case was checked before with kBadRepCode
            // if (checkDicSize == 0 && processedPos == 0) { len = kMatchSpecLen_Error_Data + 1; break; }
            // The caller doesn't allow (dicPos == limit) case here
            // so we don't need the following check:
            // if (dicPos == limit) { state = state < kNumLitStates ? 9 : 11; len = 1; break; }
            
            dic[dicPos] = dic[dicPos - rep0 + (dicPos < rep0 ? dicBufSize : 0)];
            dicPos++;
            processedPos++;
            state = state < kNumLitStates ? 9 : 11;
            continue;
          }
          UPDATE_1(prob)
        }
        else
        {
          UInt32 distance;
          UPDATE_1(prob)
          prob = probs + IsRepG1 + state;
          IF_BIT_0(prob)
          {
            UPDATE_0(prob)
            distance = rep1;
          }
          else
          {
            UPDATE_1(prob)
            prob = probs + IsRepG2 + state;
            IF_BIT_0(prob)
            {
              UPDATE_0(prob)
              distance = rep2;
            }
            else
            {
              UPDATE_1(prob)
              distance = rep3;
              rep3 = rep2;
            }
            rep2 = rep1;
          }
          rep1 = rep0;
          rep0 = distance;
        }
        state = state < kNumLitStates ? 8 : 11;
        prob = probs + RepLenCoder;
      }
      
      #ifdef Z7_LZMA_SIZE_OPT
      {
        unsigned lim, offset;
        CLzmaProb *probLen = prob + LenChoice;
        IF_BIT_0(probLen)
        {
          UPDATE_0(probLen)
          probLen = prob + LenLow + GET_LEN_STATE;
          offset = 0;
          lim = (1 << kLenNumLowBits);
        }
        else
        {
          UPDATE_1(probLen)
          probLen = prob + LenChoice2;
          IF_BIT_0(probLen)
          {
            UPDATE_0(probLen)
            probLen = prob + LenLow + GET_LEN_STATE + (1 << kLenNumLowBits);
            offset = kLenNumLowSymbols;
            lim = (1 << kLenNumLowBits);
          }
          else
          {
            UPDATE_1(probLen)
            probLen = prob + LenHigh;
            offset = kLenNumLowSymbols * 2;
            lim = (1 << kLenNumHighBits);
          }
        }
        TREE_DECODE(probLen, lim, len)
        len += offset;
      }
      #else
      {
        CLzmaProb *probLen = prob + LenChoice;
        IF_BIT_0(probLen)
        {
          UPDATE_0(probLen)
          probLen = prob + LenLow + GET_LEN_STATE;
          len = 1;
          TREE_GET_BIT(probLen, len)
          TREE_GET_BIT(probLen, len)
          TREE_GET_BIT(probLen, len)
          len -= 8;
        }
        else
        {
          UPDATE_1(probLen)
          probLen = prob + LenChoice2;
          IF_BIT_0(probLen)
          {
            UPDATE_0(probLen)
            probLen = prob + LenLow + GET_LEN_STATE + (1 << kLenNumLowBits);
            len = 1;
            TREE_GET_BIT(probLen, len)
            TREE_GET_BIT(probLen, len)
            TREE_GET_BIT(probLen, len)
          }
          else
          {
            UPDATE_1(probLen)
            probLen = prob + LenHigh;
            TREE_DECODE(probLen, (1 << kLenNumHighBits), len)
            len += kLenNumLowSymbols * 2;
          }
        }
      }
      #endif

      if (state >= kNumStates)
      {
        UInt32 distance;
        {
          const unsigned temp = (len < kNumLenToPosStates ? len : kNumLenToPosStates - 1) << kNumPosSlotBits;
          prob = probs + PosSlot + temp;
        }
        TREE_6_DECODE(prob, distance)
        if (distance >= kStartPosModelIndex)
        {
          unsigned posSlot = (unsigned)distance;
          unsigned numDirectBits = (unsigned)(((distance >> 1) - 1));
          distance = (2 | (distance & 1));
          if (posSlot < kEndPosModelIndex)
          {
            distance <<= numDirectBits;
            prob = probs + SpecPos;
            {
              UInt32 m = 1;
              distance++;
              do
              {
                REV_BIT_VAR(prob, distance, m)
              }
              while (--numDirectBits);
              distance -= m;
            }
          }
          else
          {
            numDirectBits -= kNumAlignBits;
            do
            {
              NORMALIZE
              range >>= 1;
              
              {
                UInt32 t;
                code -= range;
                t = (0 - ((UInt32)code >> 31)); /* (UInt32)((Int32)code >> 31) */
                distance = (distance << 1) + (t + 1);
                code += range & t;
              }
              /*
              distance <<= 1;
              if (code >= range)
              {
                code -= range;
                distance |= 1;
              }
              */
            }
            while (--numDirectBits);
            prob = probs + Align;
            distance <<= kNumAlignBits;
            {
              unsigned i = 1;
              REV_BIT_CONST(prob, i, 1)
              REV_BIT_CONST(prob, i, 2)
              REV_BIT_CONST(prob, i, 4)
              REV_BIT_LAST (prob, i, 8)
              distance |= i;
            }
            if (distance == (UInt32)0xFFFFFFFF)
            {
              len = kMatchSpecLenStart;
              state -= kNumStates;
              break;
            }
          }
        }
        
        rep3 = rep2;
        rep2 = rep1;
        rep1 = rep0;
        rep0 = distance + 1;
        state = (state < kNumStates + kNumLitStates) ? kNumLitStates : kNumLitStates + 3;
        if (distance >= (checkDicSize == 0 ? processedPos: checkDicSize))
        {
          len += kMatchSpecLen_Error_Data + kMatchMinLen;
          // len = kMatchSpecLen_Error_Data;
          // len += kMatchMinLen;
          break;
        }
      }

      len += kMatchMinLen;

      {
        SizeT rem;
        unsigned curLen;
        SizeT pos;
        
        if ((rem = limit - dicPos) == 0)
        {
          /*
          We stop decoding and return SZ_OK, and we can resume decoding later.
          Any error conditions can be tested later in caller code.
          For more strict mode we can stop decoding with error
          // len += kMatchSpecLen_Error_Data;
          */
          break;
        }
        
        curLen = ((rem < len) ? (unsigned)rem : len);
        pos = dicPos - rep0 + (dicPos < rep0 ? dicBufSize : 0);

        processedPos += (UInt32)curLen;

        len -= curLen;
        if (curLen <= dicBufSize - pos)
        {
          Byte *dest = dic + dicPos;
          ptrdiff_t src = (ptrdiff_t)pos - (ptrdiff_t)dicPos;
          const Byte *lim = dest + curLen;
          dicPos += (SizeT)curLen;
          do
            *(dest) = (Byte)*(dest + src);
          while (++dest != lim);
        }
        else
        {
          do
          {
            dic[dicPos++] = dic[pos];
            if (++pos == dicBufSize)
              pos = 0;
          }
          while (--curLen != 0);
        }
      }
    }
  }
  while (dicPos < limit && buf < bufLimit);

  NORMALIZE
  
  p->buf = buf;
  p->range = range;
  p->code = code;
  p->remainLen = (UInt32)len; // & (kMatchSpecLen_Error_Data - 1); // we can write real length for error matches too.
  p->dicPos = dicPos;
  p->processedPos = processedPos;
  p->reps[0] = rep0;
  p->reps[1] = rep1;
  p->reps[2] = rep2;
  p->reps[3] = rep3;
  p->state = (UInt32)state;
  if (len >= kMatchSpecLen_Error_Data)
    return SZ_ERROR_DATA;
  return SZ_OK;
}
#endif



static void Z7_FASTCALL LzmaDec_WriteRem(CLzmaDec *p, SizeT limit)
{
  unsigned len = (unsigned)p->remainLen;
  if (len == 0 /* || len >= kMatchSpecLenStart */)
    return;
  {
    SizeT dicPos = p->dicPos;
    Byte *dic;
    SizeT dicBufSize;
    SizeT rep0;   /* we use SizeT to avoid the BUG of VC14 for AMD64 */
    {
      SizeT rem = limit - dicPos;
      if (rem < len)
      {
        len = (unsigned)(rem);
        if (len == 0)
          return;
      }
    }

    if (p->checkDicSize == 0 && p->prop.dicSize - p->processedPos <= len)
      p->checkDicSize = p->prop.dicSize;

    p->processedPos += (UInt32)len;
    p->remainLen -= (UInt32)len;
    dic = p->dic;
    rep0 = p->reps[0];
    dicBufSize = p->dicBufSize;
    do
    {
      dic[dicPos] = dic[dicPos - rep0 + (dicPos < rep0 ? dicBufSize : 0)];
      dicPos++;
    }
    while (--len);
    p->dicPos = dicPos;
  }
}


/*
At staring of new stream we have one of the following symbols:
  - Literal        - is allowed
  - Non-Rep-Match  - is allowed only if it's end marker symbol
  - Rep-Match      - is not allowed
We use early check of (RangeCoder:Code) over kBadRepCode to simplify main decoding code
*/

#define kRange0 0xFFFFFFFF
#define kBound0 ((kRange0 >> kNumBitModelTotalBits) << (kNumBitModelTotalBits - 1))
#define kBadRepCode (kBound0 + (((kRange0 - kBound0) >> kNumBitModelTotalBits) << (kNumBitModelTotalBits - 1)))
#if kBadRepCode != (0xC0000000 - 0x400)
  #error Stop_Compiling_Bad_LZMA_Check
#endif


/*
LzmaDec_DecodeReal2():
  It calls LZMA_DECODE_REAL() and it adjusts limit according (p->checkDicSize).

We correct (p->checkDicSize) after LZMA_DECODE_REAL() and in LzmaDec_WriteRem(),
and we support the following state of (p->checkDicSize):
  if (total_processed < p->prop.dicSize) then
  {
    (total_processed == p->processedPos)
    (p->checkDicSize == 0)
  }
  else
    (p->checkDicSize == p->prop.dicSize)
*/

static int Z7_FASTCALL LzmaDec_DecodeReal2(CLzmaDec *p, SizeT limit, const Byte *bufLimit)
{
  if (p->checkDicSize == 0)
  {
    UInt32 rem = p->prop.dicSize - p->processedPos;
    if (limit - p->dicPos > rem)
      limit = p->dicPos + rem;
  }
  {
    int res = LZMA_DECODE_REAL(p, limit, bufLimit);
    if (p->checkDicSize == 0 && p->processedPos >= p->prop.dicSize)
      p->checkDicSize = p->prop.dicSize;
    return res;
  }
}



typedef enum
{
  DUMMY_INPUT_EOF, /* need more input data */
  DUMMY_LIT,
  DUMMY_MATCH,
  DUMMY_REP
} ELzmaDummy;


#define IS_DUMMY_END_MARKER_POSSIBLE(dummyRes) ((dummyRes) == DUMMY_MATCH)

static ELzmaDummy LzmaDec_TryDummy(const CLzmaDec *p, const Byte *buf, const Byte **bufOut)
{
  UInt32 range = p->range;
  UInt32 code = p->code;
  const Byte *bufLimit = *bufOut;
  const CLzmaProb *probs = GET_PROBS;
  unsigned state = (unsigned)p->state;
  ELzmaDummy res;

  for (;;)
  {
    const CLzmaProb *prob;
    UInt32 bound;
    unsigned ttt;
    unsigned posState = CALC_POS_STATE(p->processedPos, ((unsigned)1 << p->prop.pb) - 1);

    prob = probs + IsMatch + COMBINED_PS_STATE;
    IF_BIT_0_CHECK(prob)
    {
      UPDATE_0_CHECK

      prob = probs + Literal;
      if (p->checkDicSize != 0 || p->processedPos != 0)
        prob += ((UInt32)LZMA_LIT_SIZE *
            ((((p->processedPos) & (((unsigned)1 << (p->prop.lp)) - 1)) << p->prop.lc) +
            ((unsigned)p->dic[(p->dicPos == 0 ? p->dicBufSize : p->dicPos) - 1] >> (8 - p->prop.lc))));

      if (state < kNumLitStates)
      {
        unsigned symbol = 1;
        do { GET_BIT_CHECK(prob + symbol, symbol) } while (symbol < 0x100);
      }
      else
      {
        unsigned matchByte = p->dic[p->dicPos - p->reps[0] +
            (p->dicPos < p->reps[0] ? p->dicBufSize : 0)];
        unsigned offs = 0x100;
        unsigned symbol = 1;
        do
        {
          unsigned bit;
          const CLzmaProb *probLit;
          matchByte += matchByte;
          bit = offs;
          offs &= matchByte;
          probLit = prob + (offs + bit + symbol);
          GET_BIT2_CHECK(probLit, symbol, offs ^= bit; , ; )
        }
        while (symbol < 0x100);
      }
      res = DUMMY_LIT;
    }
    else
    {
      unsigned len;
      UPDATE_1_CHECK

      prob = probs + IsRep + state;
      IF_BIT_0_CHECK(prob)
      {
        UPDATE_0_CHECK
        state = 0;
        prob = probs + LenCoder;
        res = DUMMY_MATCH;
      }
      else
      {
        UPDATE_1_CHECK
        res = DUMMY_REP;
        prob = probs + IsRepG0 + state;
        IF_BIT_0_CHECK(prob)
        {
          UPDATE_0_CHECK
          prob = probs + IsRep0Long + COMBINED_PS_STATE;
          IF_BIT_0_CHECK(prob)
          {
            UPDATE_0_CHECK
            break;
          }
          else
          {
            UPDATE_1_CHECK
          }
        }
        else
        {
          UPDATE_1_CHECK
          prob = probs + IsRepG1 + state;
          IF_BIT_0_CHECK(prob)
          {
            UPDATE_0_CHECK
          }
          else
          {
            UPDATE_1_CHECK
            prob = probs + IsRepG2 + state;
            IF_BIT_0_CHECK(prob)
            {
              UPDATE_0_CHECK
            }
            else
            {
              UPDATE_1_CHECK
            }
          }
        }
        state = kNumStates;
        prob = probs + RepLenCoder;
      }
      {
        unsigned limit, offset;
        const CLzmaProb *probLen = prob + LenChoice;
        IF_BIT_0_CHECK(probLen)
        {
          UPDATE_0_CHECK
          probLen = prob + LenLow + GET_LEN_STATE;
          offset = 0;
          limit = 1 << kLenNumLowBits;
        }
        else
        {
          UPDATE_1_CHECK
          probLen = prob + LenChoice2;
          IF_BIT_0_CHECK(probLen)
          {
            UPDATE_0_CHECK
            probLen = prob + LenLow + GET_LEN_STATE + (1 << kLenNumLowBits);
            offset = kLenNumLowSymbols;
            limit = 1 << kLenNumLowBits;
          }
          else
          {
            UPDATE_1_CHECK
            probLen = prob + LenHigh;
            offset = kLenNumLowSymbols * 2;
            limit = 1 << kLenNumHighBits;
          }
        }
        TREE_DECODE_CHECK(probLen, limit, len)
        len += offset;
      }

      if (state < 4)
      {
        unsigned posSlot;
        {
          const unsigned temp = (len < kNumLenToPosStates - 1 ? len : kNumLenToPosStates - 1) << kNumPosSlotBits;
          prob = probs + PosSlot + temp;
        }
        TREE_DECODE_CHECK(prob, 1 << kNumPosSlotBits, posSlot)
        if (posSlot >= kStartPosModelIndex)
        {
          unsigned numDirectBits = ((posSlot >> 1) - 1);

          if (posSlot < kEndPosModelIndex)
          {
            const unsigned temp = (2 | (posSlot & 1)) << numDirectBits;
            prob = probs + SpecPos + temp;
          }
          else
          {
            numDirectBits -= kNumAlignBits;
            do
            {
              NORMALIZE_CHECK
              range >>= 1;
              code -= range & (((code - range) >> 31) - 1);
              /* if (code >= range) code -= range; */
            }
            while (--numDirectBits);
            prob = probs + Align;
            numDirectBits = kNumAlignBits;
          }
          {
            unsigned i = 1;
            unsigned m = 1;
            do
            {
              REV_BIT_CHECK(prob, i, m)
            }
            while (--numDirectBits);
          }
        }
      }
    }
    break;
  }
  NORMALIZE_CHECK

  *bufOut = buf;
  return res;
}

void LzmaDec_InitDicAndState(CLzmaDec *p, BoolInt initDic, BoolInt initState);
void LzmaDec_InitDicAndState(CLzmaDec *p, BoolInt initDic, BoolInt initState)
{
  p->remainLen = kMatchSpecLenStart + 1;
  p->tempBufSize = 0;

  if (initDic)
  {
    p->processedPos = 0;
    p->checkDicSize = 0;
    p->remainLen = kMatchSpecLenStart + 2;
  }
  if (initState)
    p->remainLen = kMatchSpecLenStart + 2;
}

void LzmaDec_Init(CLzmaDec *p)
{
  p->dicPos = 0;
  LzmaDec_InitDicAndState(p, True, True);
}


/*
LZMA supports optional end_marker.
So the decoder can lookahead for one additional LZMA-Symbol to check end_marker.
That additional LZMA-Symbol can require up to LZMA_REQUIRED_INPUT_MAX bytes in input stream.
When the decoder reaches dicLimit, it looks (finishMode) parameter:
  if (finishMode == LZMA_FINISH_ANY), the decoder doesn't lookahead
  if (finishMode != LZMA_FINISH_ANY), the decoder lookahead, if end_marker is possible for current position

When the decoder lookahead, and the lookahead symbol is not end_marker, we have two ways:
  1) Strict mode (default) : the decoder returns SZ_ERROR_DATA.
  2) The relaxed mode (alternative mode) : we could return SZ_OK, and the caller
     must check (status) value. The caller can show the error,
     if the end of stream is expected, and the (status) is noit
     LZMA_STATUS_FINISHED_WITH_MARK or LZMA_STATUS_MAYBE_FINISHED_WITHOUT_MARK.
*/


#define RETURN_NOT_FINISHED_FOR_FINISH \
  *status = LZMA_STATUS_NOT_FINISHED; \
  return SZ_ERROR_DATA; // for strict mode
  // return SZ_OK; // for relaxed mode


SRes LzmaDec_DecodeToDic(CLzmaDec *p, SizeT dicLimit, const Byte *src, SizeT *srcLen,
    ELzmaFinishMode finishMode, ELzmaStatus *status)
{
  SizeT inSize = *srcLen;
  (*srcLen) = 0;
  *status = LZMA_STATUS_NOT_SPECIFIED;

  if (p->remainLen > kMatchSpecLenStart)
  {
    if (p->remainLen > kMatchSpecLenStart + 2)
      return p->remainLen == kMatchSpecLen_Error_Fail ? SZ_ERROR_FAIL : SZ_ERROR_DATA;

    for (; inSize > 0 && p->tempBufSize < RC_INIT_SIZE; (*srcLen)++, inSize--)
      p->tempBuf[p->tempBufSize++] = *src++;
    if (p->tempBufSize != 0 && p->tempBuf[0] != 0)
      return SZ_ERROR_DATA;
    if (p->tempBufSize < RC_INIT_SIZE)
    {
      *status = LZMA_STATUS_NEEDS_MORE_INPUT;
      return SZ_OK;
    }
    p->code =
        ((UInt32)p->tempBuf[1] << 24)
      | ((UInt32)p->tempBuf[2] << 16)
      | ((UInt32)p->tempBuf[3] << 8)
      | ((UInt32)p->tempBuf[4]);

    if (p->checkDicSize == 0
        && p->processedPos == 0
        && p->code >= kBadRepCode)
      return SZ_ERROR_DATA;

    p->range = 0xFFFFFFFF;
    p->tempBufSize = 0;

    if (p->remainLen > kMatchSpecLenStart + 1)
    {
      SizeT numProbs = LzmaProps_GetNumProbs(&p->prop);
      SizeT i;
      CLzmaProb *probs = p->probs;
      for (i = 0; i < numProbs; i++)
        probs[i] = kBitModelTotal >> 1;
      p->reps[0] = p->reps[1] = p->reps[2] = p->reps[3] = 1;
      p->state = 0;
    }

    p->remainLen = 0;
  }

  for (;;)
  {
    if (p->remainLen == kMatchSpecLenStart)
    {
      if (p->code != 0)
        return SZ_ERROR_DATA;
      *status = LZMA_STATUS_FINISHED_WITH_MARK;
      return SZ_OK;
    }

    LzmaDec_WriteRem(p, dicLimit);

    {
      // (p->remainLen == 0 || p->dicPos == dicLimit)

      int checkEndMarkNow = 0;

      if (p->dicPos >= dicLimit)
      {
        if (p->remainLen == 0 && p->code == 0)
        {
          *status = LZMA_STATUS_MAYBE_FINISHED_WITHOUT_MARK;
          return SZ_OK;
        }
        if (finishMode == LZMA_FINISH_ANY)
        {
          *status = LZMA_STATUS_NOT_FINISHED;
          return SZ_OK;
        }
        if (p->remainLen != 0)
        {
          RETURN_NOT_FINISHED_FOR_FINISH
        }
        checkEndMarkNow = 1;
      }

      // (p->remainLen == 0)

      if (p->tempBufSize == 0)
      {
        const Byte *bufLimit;
        int dummyProcessed = -1;
        
        if (inSize < LZMA_REQUIRED_INPUT_MAX || checkEndMarkNow)
        {
          const Byte *bufOut = src + inSize;
          
          ELzmaDummy dummyRes = LzmaDec_TryDummy(p, src, &bufOut);
          
          if (dummyRes == DUMMY_INPUT_EOF)
          {
            size_t i;
            if (inSize >= LZMA_REQUIRED_INPUT_MAX)
              break;
            (*srcLen) += inSize;
            p->tempBufSize = (unsigned)inSize;
            for (i = 0; i < inSize; i++)
              p->tempBuf[i] = src[i];
            *status = LZMA_STATUS_NEEDS_MORE_INPUT;
            return SZ_OK;
          }
 
          dummyProcessed = (int)(bufOut - src);
          if ((unsigned)dummyProcessed > LZMA_REQUIRED_INPUT_MAX)
            break;
          
          if (checkEndMarkNow && !IS_DUMMY_END_MARKER_POSSIBLE(dummyRes))
          {
            unsigned i;
            (*srcLen) += (unsigned)dummyProcessed;
            p->tempBufSize = (unsigned)dummyProcessed;
            for (i = 0; i < (unsigned)dummyProcessed; i++)
              p->tempBuf[i] = src[i];
            // p->remainLen = kMatchSpecLen_Error_Data;
            RETURN_NOT_FINISHED_FOR_FINISH
          }
          
          bufLimit = src;
          // we will decode only one iteration
        }
        else
          bufLimit = src + inSize - LZMA_REQUIRED_INPUT_MAX;

        p->buf = src;
        
        {
          int res = LzmaDec_DecodeReal2(p, dicLimit, bufLimit);
          
          SizeT processed = (SizeT)(p->buf - src);

          if (dummyProcessed < 0)
          {
            if (processed > inSize)
              break;
          }
          else if ((unsigned)dummyProcessed != processed)
            break;

          src += processed;
          inSize -= processed;
          (*srcLen) += processed;

          if (res != SZ_OK)
          {
            p->remainLen = kMatchSpecLen_Error_Data;
            return SZ_ERROR_DATA;
          }
        }
        continue;
      }

      {
        // we have some data in (p->tempBuf)
        // in strict mode: tempBufSize is not enough for one Symbol decoding.
        // in relaxed mode: tempBufSize not larger than required for one Symbol decoding.

        unsigned rem = p->tempBufSize;
        unsigned ahead = 0;
        int dummyProcessed = -1;
        
        while (rem < LZMA_REQUIRED_INPUT_MAX && ahead < inSize)
          p->tempBuf[rem++] = src[ahead++];
        
        // ahead - the size of new data copied from (src) to (p->tempBuf)
        // rem   - the size of temp buffer including new data from (src)
        
        if (rem < LZMA_REQUIRED_INPUT_MAX || checkEndMarkNow)
        {
          const Byte *bufOut = p->tempBuf + rem;
        
          ELzmaDummy dummyRes = LzmaDec_TryDummy(p, p->tempBuf, &bufOut);
          
          if (dummyRes == DUMMY_INPUT_EOF)
          {
            if (rem >= LZMA_REQUIRED_INPUT_MAX)
              break;
            p->tempBufSize = rem;
            (*srcLen) += (SizeT)ahead;
            *status = LZMA_STATUS_NEEDS_MORE_INPUT;
            return SZ_OK;
          }
          
          dummyProcessed = (int)(bufOut - p->tempBuf);

          if ((unsigned)dummyProcessed < p->tempBufSize)
            break;

          if (checkEndMarkNow && !IS_DUMMY_END_MARKER_POSSIBLE(dummyRes))
          {
            (*srcLen) += (unsigned)dummyProcessed - p->tempBufSize;
            p->tempBufSize = (unsigned)dummyProcessed;
            // p->remainLen = kMatchSpecLen_Error_Data;
            RETURN_NOT_FINISHED_FOR_FINISH
          }
        }

        p->buf = p->tempBuf;
        
        {
          // we decode one symbol from (p->tempBuf) here, so the (bufLimit) is equal to (p->buf)
          int res = LzmaDec_DecodeReal2(p, dicLimit, p->buf);

          SizeT processed = (SizeT)(p->buf - p->tempBuf);
          rem = p->tempBufSize;
          
          if (dummyProcessed < 0)
          {
            if (processed > LZMA_REQUIRED_INPUT_MAX)
              break;
            if (processed < rem)
              break;
          }
          else if ((unsigned)dummyProcessed != processed)
            break;
          
          processed -= rem;

          src += processed;
          inSize -= processed;
          (*srcLen) += processed;
          p->tempBufSize = 0;
          
          if (res != SZ_OK)
          {
            p->remainLen = kMatchSpecLen_Error_Data;
            return SZ_ERROR_DATA;
          }
        }
      }
    }
  }

  /*  Some unexpected error: internal error of code, memory corruption or hardware failure */
  p->remainLen = kMatchSpecLen_Error_Fail;
  return SZ_ERROR_FAIL;
}



SRes LzmaDec_DecodeToBuf(CLzmaDec *p, Byte *dest, SizeT *destLen, const Byte *src, SizeT *srcLen, ELzmaFinishMode finishMode, ELzmaStatus *status)
{
  SizeT outSize = *destLen;
  SizeT inSize = *srcLen;
  *srcLen = *destLen = 0;
  for (;;)
  {
    SizeT inSizeCur = inSize, outSizeCur, dicPos;
    ELzmaFinishMode curFinishMode;
    SRes res;
    if (p->dicPos == p->dicBufSize)
      p->dicPos = 0;
    dicPos = p->dicPos;
    if (outSize > p->dicBufSize - dicPos)
    {
      outSizeCur = p->dicBufSize;
      curFinishMode = LZMA_FINISH_ANY;
    }
    else
    {
      outSizeCur = dicPos + outSize;
      curFinishMode = finishMode;
    }

    res = LzmaDec_DecodeToDic(p, outSizeCur, src, &inSizeCur, curFinishMode, status);
    src += inSizeCur;
    inSize -= inSizeCur;
    *srcLen += inSizeCur;
    outSizeCur = p->dicPos - dicPos;
    memcpy(dest, p->dic + dicPos, outSizeCur);
    dest += outSizeCur;
    outSize -= outSizeCur;
    *destLen += outSizeCur;
    if (res != 0)
      return res;
    if (outSizeCur == 0 || outSize == 0)
      return SZ_OK;
  }
}

void LzmaDec_FreeProbs(CLzmaDec *p, ISzAllocPtr alloc)
{
  ISzAlloc_Free(alloc, p->probs);
  p->probs = NULL;
}

static void LzmaDec_FreeDict(CLzmaDec *p, ISzAllocPtr alloc)
{
  ISzAlloc_Free(alloc, p->dic);
  p->dic = NULL;
}

void LzmaDec_Free(CLzmaDec *p, ISzAllocPtr alloc)
{
  LzmaDec_FreeProbs(p, alloc);
  LzmaDec_FreeDict(p, alloc);
}

SRes LzmaProps_Decode(CLzmaProps *p, const Byte *data, unsigned size)
{
  UInt32 dicSize;
  Byte d;
  
  if (size < LZMA_PROPS_SIZE)
    return SZ_ERROR_UNSUPPORTED;
  else
    dicSize = data[1] | ((UInt32)data[2] << 8) | ((UInt32)data[3] << 16) | ((UInt32)data[4] << 24);
 
  if (dicSize < LZMA_DIC_MIN)
    dicSize = LZMA_DIC_MIN;
  p->dicSize = dicSize;

  d = data[0];
  if (d >= (9 * 5 * 5))
    return SZ_ERROR_UNSUPPORTED;

  p->lc = (Byte)(d % 9);
  d /= 9;
  p->pb = (Byte)(d / 5);
  p->lp = (Byte)(d % 5);

  return SZ_OK;
}

static SRes LzmaDec_AllocateProbs2(CLzmaDec *p, const CLzmaProps *propNew, ISzAllocPtr alloc)
{
  UInt32 numProbs = LzmaProps_GetNumProbs(propNew);
  if (!p->probs || numProbs != p->numProbs)
  {
    LzmaDec_FreeProbs(p, alloc);
    p->probs = (CLzmaProb *)ISzAlloc_Alloc(alloc, numProbs * sizeof(CLzmaProb));
    if (!p->probs)
      return SZ_ERROR_MEM;
    p->probs_1664 = p->probs + 1664;
    p->numProbs = numProbs;
  }
  return SZ_OK;
}

SRes LzmaDec_AllocateProbs(CLzmaDec *p, const Byte *props, unsigned propsSize, ISzAllocPtr alloc)
{
  CLzmaProps propNew;
  RINOK(LzmaProps_Decode(&propNew, props, propsSize))
  RINOK(LzmaDec_AllocateProbs2(p, &propNew, alloc))
  p->prop = propNew;
  return SZ_OK;
}

SRes LzmaDec_Allocate(CLzmaDec *p, const Byte *props, unsigned propsSize, ISzAllocPtr alloc)
{
  CLzmaProps propNew;
  SizeT dicBufSize;
  RINOK(LzmaProps_Decode(&propNew, props, propsSize))
  RINOK(LzmaDec_AllocateProbs2(p, &propNew, alloc))

  {
    UInt32 dictSize = propNew.dicSize;
    SizeT mask = ((UInt32)1 << 12) - 1;
         if (dictSize >= ((UInt32)1 << 30)) mask = ((UInt32)1 << 22) - 1;
    else if (dictSize >= ((UInt32)1 << 22)) mask = ((UInt32)1 << 20) - 1;
    dicBufSize = ((SizeT)dictSize + mask) & ~mask;
    if (dicBufSize < dictSize)
      dicBufSize = dictSize;
  }

  if (!p->dic || dicBufSize != p->dicBufSize)
  {
    LzmaDec_FreeDict(p, alloc);
    p->dic = (Byte *)ISzAlloc_Alloc(alloc, dicBufSize);
    if (!p->dic)
    {
      LzmaDec_FreeProbs(p, alloc);
      return SZ_ERROR_MEM;
    }
  }
  p->dicBufSize = dicBufSize;
  p->prop = propNew;
  return SZ_OK;
}

SRes LzmaDecode(Byte *dest, SizeT *destLen, const Byte *src, SizeT *srcLen,
    const Byte *propData, unsigned propSize, ELzmaFinishMode finishMode,
    ELzmaStatus *status, ISzAllocPtr alloc)
{
  CLzmaDec p;
  SRes res;
  SizeT outSize = *destLen, inSize = *srcLen;
  *destLen = *srcLen = 0;
  *status = LZMA_STATUS_NOT_SPECIFIED;
  if (inSize < RC_INIT_SIZE)
    return SZ_ERROR_INPUT_EOF;
  LzmaDec_CONSTRUCT(&p)
  RINOK(LzmaDec_AllocateProbs(&p, propData, propSize, alloc))
  p.dic = dest;
  p.dicBufSize = outSize;
  LzmaDec_Init(&p);
  *srcLen = inSize;
  res = LzmaDec_DecodeToDic(&p, outSize, src, srcLen, finishMode, status);
  *destLen = p.dicPos;
  if (res == SZ_OK && *status == LZMA_STATUS_NEEDS_MORE_INPUT)
    res = SZ_ERROR_INPUT_EOF;
  LzmaDec_FreeProbs(&p, alloc);
  return res;
}

/* ==== Lzma2Dec.c ==== */
/* Lzma2Dec.c -- LZMA2 Decoder
2024-03-01 : Igor Pavlov : Public domain */

/* #define SHOW_DEBUG_INFO */

#include "Precomp.h"

#ifdef SHOW_DEBUG_INFO
#include <stdio.h>
#endif

#include <string.h>

#include "Lzma2Dec.h"

/*
00000000  -  End of data
00000001 U U  -  Uncompressed, reset dic, need reset state and set new prop
00000010 U U  -  Uncompressed, no reset
100uuuuu U U P P  -  LZMA, no reset
101uuuuu U U P P  -  LZMA, reset state
110uuuuu U U P P S  -  LZMA, reset state + set new prop
111uuuuu U U P P S  -  LZMA, reset state + set new prop, reset dic

  u, U - Unpack Size
  P - Pack Size
  S - Props
*/

#define LZMA2_CONTROL_COPY_RESET_DIC 1

#define LZMA2_IS_UNCOMPRESSED_STATE(p) (((p)->control & (1 << 7)) == 0)

#define LZMA2_LCLP_MAX 4
#define LZMA2_DIC_SIZE_FROM_PROP(p) (((UInt32)2 | ((p) & 1)) << ((p) / 2 + 11))

#ifdef SHOW_DEBUG_INFO
#define PRF(x) x
#else
#define PRF(x)
#endif

typedef enum
{
  LZMA2_STATE_CONTROL,
  LZMA2_STATE_UNPACK0,
  LZMA2_STATE_UNPACK1,
  LZMA2_STATE_PACK0,
  LZMA2_STATE_PACK1,
  LZMA2_STATE_PROP,
  LZMA2_STATE_DATA,
  LZMA2_STATE_DATA_CONT,
  LZMA2_STATE_FINISHED,
  LZMA2_STATE_ERROR
} ELzma2State;

static SRes Lzma2Dec_GetOldProps(Byte prop, Byte *props)
{
  UInt32 dicSize;
  if (prop > 40)
    return SZ_ERROR_UNSUPPORTED;
  dicSize = (prop == 40) ? 0xFFFFFFFF : LZMA2_DIC_SIZE_FROM_PROP(prop);
  props[0] = (Byte)LZMA2_LCLP_MAX;
  props[1] = (Byte)(dicSize);
  props[2] = (Byte)(dicSize >> 8);
  props[3] = (Byte)(dicSize >> 16);
  props[4] = (Byte)(dicSize >> 24);
  return SZ_OK;
}

SRes Lzma2Dec_AllocateProbs(CLzma2Dec *p, Byte prop, ISzAllocPtr alloc)
{
  Byte props[LZMA_PROPS_SIZE];
  RINOK(Lzma2Dec_GetOldProps(prop, props))
  return LzmaDec_AllocateProbs(&p->decoder, props, LZMA_PROPS_SIZE, alloc);
}

SRes Lzma2Dec_Allocate(CLzma2Dec *p, Byte prop, ISzAllocPtr alloc)
{
  Byte props[LZMA_PROPS_SIZE];
  RINOK(Lzma2Dec_GetOldProps(prop, props))
  return LzmaDec_Allocate(&p->decoder, props, LZMA_PROPS_SIZE, alloc);
}

void Lzma2Dec_Init(CLzma2Dec *p)
{
  p->state = LZMA2_STATE_CONTROL;
  p->needInitLevel = 0xE0;
  p->isExtraMode = False;
  p->unpackSize = 0;
  
  // p->decoder.dicPos = 0; // we can use it instead of full init
  LzmaDec_Init(&p->decoder);
}

// ELzma2State
static unsigned Lzma2Dec_UpdateState(CLzma2Dec *p, Byte b)
{
  switch (p->state)
  {
    case LZMA2_STATE_CONTROL:
      p->isExtraMode = False;
      p->control = b;
      PRF(printf("\n %8X", (unsigned)p->decoder.dicPos));
      PRF(printf(" %02X", (unsigned)b));
      if (b == 0)
        return LZMA2_STATE_FINISHED;
      if (LZMA2_IS_UNCOMPRESSED_STATE(p))
      {
        if (b == LZMA2_CONTROL_COPY_RESET_DIC)
          p->needInitLevel = 0xC0;
        else if (b > 2 || p->needInitLevel == 0xE0)
          return LZMA2_STATE_ERROR;
      }
      else
      {
        if (b < p->needInitLevel)
          return LZMA2_STATE_ERROR;
        p->needInitLevel = 0;
        p->unpackSize = (UInt32)(b & 0x1F) << 16;
      }
      return LZMA2_STATE_UNPACK0;
    
    case LZMA2_STATE_UNPACK0:
      p->unpackSize |= (UInt32)b << 8;
      return LZMA2_STATE_UNPACK1;
    
    case LZMA2_STATE_UNPACK1:
      p->unpackSize |= (UInt32)b;
      p->unpackSize++;
      PRF(printf(" %7u", (unsigned)p->unpackSize));
      return LZMA2_IS_UNCOMPRESSED_STATE(p) ? LZMA2_STATE_DATA : LZMA2_STATE_PACK0;
    
    case LZMA2_STATE_PACK0:
      p->packSize = (UInt32)b << 8;
      return LZMA2_STATE_PACK1;

    case LZMA2_STATE_PACK1:
      p->packSize |= (UInt32)b;
      p->packSize++;
      // if (p->packSize < 5) return LZMA2_STATE_ERROR;
      PRF(printf(" %5u", (unsigned)p->packSize));
      return (p->control & 0x40) ? LZMA2_STATE_PROP : LZMA2_STATE_DATA;

    case LZMA2_STATE_PROP:
    {
      unsigned lc, lp;
      if (b >= (9 * 5 * 5))
        return LZMA2_STATE_ERROR;
      lc = b % 9;
      b /= 9;
      p->decoder.prop.pb = (Byte)(b / 5);
      lp = b % 5;
      if (lc + lp > LZMA2_LCLP_MAX)
        return LZMA2_STATE_ERROR;
      p->decoder.prop.lc = (Byte)lc;
      p->decoder.prop.lp = (Byte)lp;
      return LZMA2_STATE_DATA;
    }
    
    default:
      return LZMA2_STATE_ERROR;
  }
}

static void LzmaDec_UpdateWithUncompressed(CLzmaDec *p, const Byte *src, SizeT size)
{
  memcpy(p->dic + p->dicPos, src, size);
  p->dicPos += size;
  if (p->checkDicSize == 0 && p->prop.dicSize - p->processedPos <= size)
    p->checkDicSize = p->prop.dicSize;
  p->processedPos += (UInt32)size;
}

void LzmaDec_InitDicAndState(CLzmaDec *p, BoolInt initDic, BoolInt initState);


SRes Lzma2Dec_DecodeToDic(CLzma2Dec *p, SizeT dicLimit,
    const Byte *src, SizeT *srcLen, ELzmaFinishMode finishMode, ELzmaStatus *status)
{
  SizeT inSize = *srcLen;
  *srcLen = 0;
  *status = LZMA_STATUS_NOT_SPECIFIED;

  while (p->state != LZMA2_STATE_ERROR)
  {
    SizeT dicPos;

    if (p->state == LZMA2_STATE_FINISHED)
    {
      *status = LZMA_STATUS_FINISHED_WITH_MARK;
      return SZ_OK;
    }
    
    dicPos = p->decoder.dicPos;
    
    if (dicPos == dicLimit && finishMode == LZMA_FINISH_ANY)
    {
      *status = LZMA_STATUS_NOT_FINISHED;
      return SZ_OK;
    }

    if (p->state != LZMA2_STATE_DATA && p->state != LZMA2_STATE_DATA_CONT)
    {
      if (*srcLen == inSize)
      {
        *status = LZMA_STATUS_NEEDS_MORE_INPUT;
        return SZ_OK;
      }
      (*srcLen)++;
      p->state = Lzma2Dec_UpdateState(p, *src++);
      if (dicPos == dicLimit && p->state != LZMA2_STATE_FINISHED)
        break;
      continue;
    }
    
    {
      SizeT inCur = inSize - *srcLen;
      SizeT outCur = dicLimit - dicPos;
      ELzmaFinishMode curFinishMode = LZMA_FINISH_ANY;
      
      if (outCur >= p->unpackSize)
      {
        outCur = (SizeT)p->unpackSize;
        curFinishMode = LZMA_FINISH_END;
      }

      if (LZMA2_IS_UNCOMPRESSED_STATE(p))
      {
        if (inCur == 0)
        {
          *status = LZMA_STATUS_NEEDS_MORE_INPUT;
          return SZ_OK;
        }

        if (p->state == LZMA2_STATE_DATA)
        {
          BoolInt initDic = (p->control == LZMA2_CONTROL_COPY_RESET_DIC);
          LzmaDec_InitDicAndState(&p->decoder, initDic, False);
        }

        if (inCur > outCur)
          inCur = outCur;
        if (inCur == 0)
          break;

        LzmaDec_UpdateWithUncompressed(&p->decoder, src, inCur);

        src += inCur;
        *srcLen += inCur;
        p->unpackSize -= (UInt32)inCur;
        p->state = (p->unpackSize == 0) ? LZMA2_STATE_CONTROL : LZMA2_STATE_DATA_CONT;
      }
      else
      {
        SRes res;

        if (p->state == LZMA2_STATE_DATA)
        {
          BoolInt initDic = (p->control >= 0xE0);
          BoolInt initState = (p->control >= 0xA0);
          LzmaDec_InitDicAndState(&p->decoder, initDic, initState);
          p->state = LZMA2_STATE_DATA_CONT;
        }
  
        if (inCur > p->packSize)
          inCur = (SizeT)p->packSize;
        
        res = LzmaDec_DecodeToDic(&p->decoder, dicPos + outCur, src, &inCur, curFinishMode, status);

        src += inCur;
        *srcLen += inCur;
        p->packSize -= (UInt32)inCur;
        outCur = p->decoder.dicPos - dicPos;
        p->unpackSize -= (UInt32)outCur;

        if (res != 0)
          break;
        
        if (*status == LZMA_STATUS_NEEDS_MORE_INPUT)
        {
          if (p->packSize == 0)
            break;
          return SZ_OK;
        }

        if (inCur == 0 && outCur == 0)
        {
          if (*status != LZMA_STATUS_MAYBE_FINISHED_WITHOUT_MARK
              || p->unpackSize != 0
              || p->packSize != 0)
            break;
          p->state = LZMA2_STATE_CONTROL;
        }
        
        *status = LZMA_STATUS_NOT_SPECIFIED;
      }
    }
  }
  
  *status = LZMA_STATUS_NOT_SPECIFIED;
  p->state = LZMA2_STATE_ERROR;
  return SZ_ERROR_DATA;
}




ELzma2ParseStatus Lzma2Dec_Parse(CLzma2Dec *p,
    SizeT outSize,
    const Byte *src, SizeT *srcLen,
    int checkFinishBlock)
{
  SizeT inSize = *srcLen;
  *srcLen = 0;

  while (p->state != LZMA2_STATE_ERROR)
  {
    if (p->state == LZMA2_STATE_FINISHED)
      return (ELzma2ParseStatus)LZMA_STATUS_FINISHED_WITH_MARK;

    if (outSize == 0 && !checkFinishBlock)
      return (ELzma2ParseStatus)LZMA_STATUS_NOT_FINISHED;
    
    if (p->state != LZMA2_STATE_DATA && p->state != LZMA2_STATE_DATA_CONT)
    {
      if (*srcLen == inSize)
        return (ELzma2ParseStatus)LZMA_STATUS_NEEDS_MORE_INPUT;
      (*srcLen)++;

      p->state = Lzma2Dec_UpdateState(p, *src++);

      if (p->state == LZMA2_STATE_UNPACK0)
      {
        // if (p->decoder.dicPos != 0)
        if (p->control == LZMA2_CONTROL_COPY_RESET_DIC || p->control >= 0xE0)
          return LZMA2_PARSE_STATUS_NEW_BLOCK;
        // if (outSize == 0) return LZMA_STATUS_NOT_FINISHED;
      }

      // The following code can be commented.
      // It's not big problem, if we read additional input bytes.
      // It will be stopped later in LZMA2_STATE_DATA / LZMA2_STATE_DATA_CONT state.

      if (outSize == 0 && p->state != LZMA2_STATE_FINISHED)
      {
        // checkFinishBlock is true. So we expect that block must be finished,
        // We can return LZMA_STATUS_NOT_SPECIFIED or LZMA_STATUS_NOT_FINISHED here
        // break;
        return (ELzma2ParseStatus)LZMA_STATUS_NOT_FINISHED;
      }

      if (p->state == LZMA2_STATE_DATA)
        return LZMA2_PARSE_STATUS_NEW_CHUNK;

      continue;
    }

    if (outSize == 0)
      return (ELzma2ParseStatus)LZMA_STATUS_NOT_FINISHED;

    {
      SizeT inCur = inSize - *srcLen;

      if (LZMA2_IS_UNCOMPRESSED_STATE(p))
      {
        if (inCur == 0)
          return (ELzma2ParseStatus)LZMA_STATUS_NEEDS_MORE_INPUT;
        if (inCur > p->unpackSize)
          inCur = p->unpackSize;
        if (inCur > outSize)
          inCur = outSize;
        p->decoder.dicPos += inCur;
        src += inCur;
        *srcLen += inCur;
        outSize -= inCur;
        p->unpackSize -= (UInt32)inCur;
        p->state = (p->unpackSize == 0) ? LZMA2_STATE_CONTROL : LZMA2_STATE_DATA_CONT;
      }
      else
      {
        p->isExtraMode = True;

        if (inCur == 0)
        {
          if (p->packSize != 0)
            return (ELzma2ParseStatus)LZMA_STATUS_NEEDS_MORE_INPUT;
        }
        else if (p->state == LZMA2_STATE_DATA)
        {
          p->state = LZMA2_STATE_DATA_CONT;
          if (*src != 0)
          {
            // first byte of lzma chunk must be Zero
            *srcLen += 1;
            p->packSize--;
            break;
          }
        }
  
        if (inCur > p->packSize)
          inCur = (SizeT)p->packSize;

        src += inCur;
        *srcLen += inCur;
        p->packSize -= (UInt32)inCur;

        if (p->packSize == 0)
        {
          SizeT rem = outSize;
          if (rem > p->unpackSize)
            rem = p->unpackSize;
          p->decoder.dicPos += rem;
          p->unpackSize -= (UInt32)rem;
          outSize -= rem;
          if (p->unpackSize == 0)
            p->state = LZMA2_STATE_CONTROL;
        }
      }
    }
  }
  
  p->state = LZMA2_STATE_ERROR;
  return (ELzma2ParseStatus)LZMA_STATUS_NOT_SPECIFIED;
}




SRes Lzma2Dec_DecodeToBuf(CLzma2Dec *p, Byte *dest, SizeT *destLen, const Byte *src, SizeT *srcLen, ELzmaFinishMode finishMode, ELzmaStatus *status)
{
  SizeT outSize = *destLen, inSize = *srcLen;
  *srcLen = *destLen = 0;
  
  for (;;)
  {
    SizeT inCur = inSize, outCur, dicPos;
    ELzmaFinishMode curFinishMode;
    SRes res;
    
    if (p->decoder.dicPos == p->decoder.dicBufSize)
      p->decoder.dicPos = 0;
    dicPos = p->decoder.dicPos;
    curFinishMode = LZMA_FINISH_ANY;
    outCur = p->decoder.dicBufSize - dicPos;
    
    if (outCur >= outSize)
    {
      outCur = outSize;
      curFinishMode = finishMode;
    }

    res = Lzma2Dec_DecodeToDic(p, dicPos + outCur, src, &inCur, curFinishMode, status);
    
    src += inCur;
    inSize -= inCur;
    *srcLen += inCur;
    outCur = p->decoder.dicPos - dicPos;
    memcpy(dest, p->decoder.dic + dicPos, outCur);
    dest += outCur;
    outSize -= outCur;
    *destLen += outCur;
    if (res != 0)
      return res;
    if (outCur == 0 || outSize == 0)
      return SZ_OK;
  }
}


SRes Lzma2Decode(Byte *dest, SizeT *destLen, const Byte *src, SizeT *srcLen,
    Byte prop, ELzmaFinishMode finishMode, ELzmaStatus *status, ISzAllocPtr alloc)
{
  CLzma2Dec p;
  SRes res;
  SizeT outSize = *destLen, inSize = *srcLen;
  *destLen = *srcLen = 0;
  *status = LZMA_STATUS_NOT_SPECIFIED;
  Lzma2Dec_CONSTRUCT(&p)
  RINOK(Lzma2Dec_AllocateProbs(&p, prop, alloc))
  p.decoder.dic = dest;
  p.decoder.dicBufSize = outSize;
  Lzma2Dec_Init(&p);
  *srcLen = inSize;
  res = Lzma2Dec_DecodeToDic(&p, outSize, src, srcLen, finishMode, status);
  *destLen = p.decoder.dicPos;
  if (res == SZ_OK && *status == LZMA_STATUS_NEEDS_MORE_INPUT)
    res = SZ_ERROR_INPUT_EOF;
  Lzma2Dec_FreeProbs(&p, alloc);
  return res;
}

#undef PRF

/* ==== Bra.c ==== */
/* Bra.c -- Branch converters for RISC code
2024-01-20 : Igor Pavlov : Public domain */

#include "Precomp.h"

#include "Bra.h"
#include "RotateDefs.h"
#include "CpuArch.h"

#if defined(MY_CPU_SIZEOF_POINTER) \
    && ( MY_CPU_SIZEOF_POINTER == 4 \
      || MY_CPU_SIZEOF_POINTER == 8)
  #define BR_CONV_USE_OPT_PC_PTR
#endif

#ifdef BR_CONV_USE_OPT_PC_PTR
#define BR_PC_INIT  pc -= (UInt32)(SizeT)p;
#define BR_PC_GET   (pc + (UInt32)(SizeT)p)
#else
#define BR_PC_INIT  pc += (UInt32)size;
#define BR_PC_GET   (pc - (UInt32)(SizeT)(lim - p))
// #define BR_PC_INIT
// #define BR_PC_GET   (pc + (UInt32)(SizeT)(p - data))
#endif

#define BR_CONVERT_VAL(v, c) if (encoding) v += c; else v -= c;
// #define BR_CONVERT_VAL(v, c) if (!encoding) c = (UInt32)0 - c; v += c;

#define Z7_BRANCH_CONV(name) z7_ ## name

#define Z7_BRANCH_FUNC_MAIN(name) \
static \
Z7_FORCE_INLINE \
Z7_ATTRIB_NO_VECTOR \
Byte *Z7_BRANCH_CONV(name)(Byte *p, SizeT size, UInt32 pc, int encoding)

#define Z7_BRANCH_FUNC_IMP(name, m, encoding) \
Z7_NO_INLINE \
Z7_ATTRIB_NO_VECTOR \
Byte *m(name)(Byte *data, SizeT size, UInt32 pc) \
  { return Z7_BRANCH_CONV(name)(data, size, pc, encoding); } \

#ifdef Z7_EXTRACT_ONLY
#define Z7_BRANCH_FUNCS_IMP(name) \
  Z7_BRANCH_FUNC_IMP(name, Z7_BRANCH_CONV_DEC_2, 0)
#else
#define Z7_BRANCH_FUNCS_IMP(name) \
  Z7_BRANCH_FUNC_IMP(name, Z7_BRANCH_CONV_DEC_2, 0) \
  Z7_BRANCH_FUNC_IMP(name, Z7_BRANCH_CONV_ENC_2, 1)
#endif

#if defined(__clang__)
#define BR_EXTERNAL_FOR
#define BR_NEXT_ITERATION  continue;
#else
#define BR_EXTERNAL_FOR    for (;;)
#define BR_NEXT_ITERATION  break;
#endif

#if defined(__clang__) && (__clang_major__ >= 8) \
  || defined(__GNUC__) && (__GNUC__ >= 1000) \
  // GCC is not good for __builtin_expect() here
  /* || defined(_MSC_VER) && (_MSC_VER >= 1920) */
  // #define Z7_unlikely [[unlikely]]
  // #define Z7_LIKELY(x)   (__builtin_expect((x), 1))
  #define Z7_UNLIKELY(x) (__builtin_expect((x), 0))
  // #define Z7_likely [[likely]]
#else
  // #define Z7_LIKELY(x)   (x)
  #define Z7_UNLIKELY(x) (x)
  // #define Z7_likely
#endif


Z7_BRANCH_FUNC_MAIN(BranchConv_ARM64)
{
  // Byte *p = data;
  const Byte *lim;
  const UInt32 flag = (UInt32)1 << (24 - 4);
  const UInt32 mask = ((UInt32)1 << 24) - (flag << 1);
  size &= ~(SizeT)3;
  // if (size == 0) return p;
  lim = p + size;
  BR_PC_INIT
  pc -= 4;  // because (p) will point to next instruction
  
  BR_EXTERNAL_FOR
  {
    // Z7_PRAGMA_OPT_DISABLE_LOOP_UNROLL_VECTORIZE
    for (;;)
    {
      UInt32 v;
      if Z7_UNLIKELY(p == lim)
        return p;
      v = GetUi32a(p);
      p += 4;
      if Z7_UNLIKELY(((v - 0x94000000) & 0xfc000000) == 0)
      {
        UInt32 c = BR_PC_GET >> 2;
        BR_CONVERT_VAL(v, c)
        v &= 0x03ffffff;
        v |= 0x94000000;
        SetUi32a(p - 4, v)
        BR_NEXT_ITERATION
      }
      // v = rotlFixed(v, 8);  v += (flag << 8) - 0x90;  if Z7_UNLIKELY((v & ((mask << 8) + 0x9f)) == 0)
      v -= 0x90000000;  if Z7_UNLIKELY((v & 0x9f000000) == 0)
      {
        UInt32 z, c;
        // v = rotrFixed(v, 8);
        v += flag; if Z7_UNLIKELY(v & mask) continue;
        z = (v & 0xffffffe0) | (v >> 26);
        c = (BR_PC_GET >> (12 - 3)) & ~(UInt32)7;
        BR_CONVERT_VAL(z, c)
        v &= 0x1f;
        v |= 0x90000000;
        v |= z << 26;
        v |= 0x00ffffe0 & ((z & (((flag << 1) - 1))) - flag);
        SetUi32a(p - 4, v)
      }
    }
  }
}
Z7_BRANCH_FUNCS_IMP(BranchConv_ARM64)


Z7_BRANCH_FUNC_MAIN(BranchConv_ARM)
{
  // Byte *p = data;
  const Byte *lim;
  size &= ~(SizeT)3;
  lim = p + size;
  BR_PC_INIT
  /* in ARM: branch offset is relative to the +2 instructions from current instruction.
     (p) will point to next instruction */
  pc += 8 - 4;
  
  for (;;)
  {
    for (;;)
    {
      if Z7_UNLIKELY(p >= lim) { return p; }  p += 4;  if Z7_UNLIKELY(p[-1] == 0xeb) break;
      if Z7_UNLIKELY(p >= lim) { return p; }  p += 4;  if Z7_UNLIKELY(p[-1] == 0xeb) break;
    }
    {
      UInt32 v = GetUi32a(p - 4);
      UInt32 c = BR_PC_GET >> 2;
      BR_CONVERT_VAL(v, c)
      v &= 0x00ffffff;
      v |= 0xeb000000;
      SetUi32a(p - 4, v)
    }
  }
}
Z7_BRANCH_FUNCS_IMP(BranchConv_ARM)


Z7_BRANCH_FUNC_MAIN(BranchConv_PPC)
{
  // Byte *p = data;
  const Byte *lim;
  size &= ~(SizeT)3;
  lim = p + size;
  BR_PC_INIT
  pc -= 4;  // because (p) will point to next instruction
  
  for (;;)
  {
    UInt32 v;
    for (;;)
    {
      if Z7_UNLIKELY(p == lim)
        return p;
      // v = GetBe32a(p);
      v = *(UInt32 *)(void *)p;
      p += 4;
      // if ((v & 0xfc000003) == 0x48000001) break;
      // if ((p[-4] & 0xFC) == 0x48 && (p[-1] & 3) == 1) break;
      if Z7_UNLIKELY(
          ((v - Z7_CONV_BE_TO_NATIVE_CONST32(0x48000001))
              & Z7_CONV_BE_TO_NATIVE_CONST32(0xfc000003)) == 0) break;
    }
    {
      v = Z7_CONV_NATIVE_TO_BE_32(v);
      {
        UInt32 c = BR_PC_GET;
        BR_CONVERT_VAL(v, c)
      }
      v &= 0x03ffffff;
      v |= 0x48000000;
      SetBe32a(p - 4, v)
    }
  }
}
Z7_BRANCH_FUNCS_IMP(BranchConv_PPC)


#ifdef Z7_CPU_FAST_ROTATE_SUPPORTED
#define BR_SPARC_USE_ROTATE
#endif

Z7_BRANCH_FUNC_MAIN(BranchConv_SPARC)
{
  // Byte *p = data;
  const Byte *lim;
  const UInt32 flag = (UInt32)1 << 22;
  size &= ~(SizeT)3;
  lim = p + size;
  BR_PC_INIT
  pc -= 4;  // because (p) will point to next instruction
  for (;;)
  {
    UInt32 v;
    for (;;)
    {
      if Z7_UNLIKELY(p == lim)
        return p;
      /* // the code without GetBe32a():
      { const UInt32 v = GetUi16a(p) & 0xc0ff; p += 4; if (v == 0x40 || v == 0xc07f) break; }
      */
      v = GetBe32a(p);
      p += 4;
    #ifdef BR_SPARC_USE_ROTATE
      v = rotlFixed(v, 2);
      v += (flag << 2) - 1;
      if Z7_UNLIKELY((v & (3 - (flag << 3))) == 0)
    #else
      v += (UInt32)5 << 29;
      v ^= (UInt32)7 << 29;
      v += flag;
      if Z7_UNLIKELY((v & (0 - (flag << 1))) == 0)
    #endif
        break;
    }
    {
      // UInt32 v = GetBe32a(p - 4);
    #ifndef BR_SPARC_USE_ROTATE
      v <<= 2;
    #endif
      {
        UInt32 c = BR_PC_GET;
        BR_CONVERT_VAL(v, c)
      }
      v &= (flag << 3) - 1;
    #ifdef BR_SPARC_USE_ROTATE
      v -= (flag << 2) - 1;
      v = rotrFixed(v, 2);
    #else
      v -= (flag << 2);
      v >>= 2;
      v |= (UInt32)1 << 30;
    #endif
      SetBe32a(p - 4, v)
    }
  }
}
Z7_BRANCH_FUNCS_IMP(BranchConv_SPARC)


Z7_BRANCH_FUNC_MAIN(BranchConv_ARMT)
{
  // Byte *p = data;
  Byte *lim;
  size &= ~(SizeT)1;
  // if (size == 0) return p;
  if (size <= 2) return p;
  size -= 2;
  lim = p + size;
  BR_PC_INIT
  /* in ARM: branch offset is relative to the +2 instructions from current instruction.
     (p) will point to the +2 instructions from current instruction */
  // pc += 4 - 4;
  // if (encoding) pc -= 0xf800 << 1; else pc += 0xf800 << 1;
  // #define ARMT_TAIL_PROC { goto armt_tail; }
  #define ARMT_TAIL_PROC { return p; }
  
  do
  {
    /* in MSVC 32-bit x86 compilers:
       UInt32 version : it loads value from memory with movzx
       Byte   version : it loads value to 8-bit register (AL/CL)
       movzx version is slightly faster in some cpus
    */
    unsigned b1;
    // Byte / unsigned
    b1 = p[1];
    // optimized version to reduce one (p >= lim) check:
    // unsigned a1 = p[1];  b1 = p[3];  p += 2;  if Z7_LIKELY((b1 & (a1 ^ 8)) < 0xf8)
    for (;;)
    {
      unsigned b3; // Byte / UInt32
      /* (Byte)(b3) normalization can use low byte computations in MSVC.
         It gives smaller code, and no loss of speed in some compilers/cpus.
         But new MSVC 32-bit x86 compilers use more slow load
         from memory to low byte register in that case.
         So we try to use full 32-bit computations for faster code.
      */
      // if (p >= lim) { ARMT_TAIL_PROC }  b3 = b1 + 8;  b1 = p[3];  p += 2;  if ((b3 & b1) >= 0xf8) break;
      if Z7_UNLIKELY(p >= lim) { ARMT_TAIL_PROC }  b3 = p[3];  p += 2;  if Z7_UNLIKELY((b3 & (b1 ^ 8)) >= 0xf8) break;
      if Z7_UNLIKELY(p >= lim) { ARMT_TAIL_PROC }  b1 = p[3];  p += 2;  if Z7_UNLIKELY((b1 & (b3 ^ 8)) >= 0xf8) break;
    }
    {
      /* we can adjust pc for (0xf800) to rid of (& 0x7FF) operation.
         But gcc/clang for arm64 can use bfi instruction for full code here */
      UInt32 v =
          ((UInt32)GetUi16a(p - 2) << 11) |
          ((UInt32)GetUi16a(p) & 0x7FF);
      /*
      UInt32 v =
            ((UInt32)p[1 - 2] << 19)
          + (((UInt32)p[1] & 0x7) << 8)
          + (((UInt32)p[-2] << 11))
          + (p[0]);
      */
      p += 2;
      {
        UInt32 c = BR_PC_GET >> 1;
        BR_CONVERT_VAL(v, c)
      }
      SetUi16a(p - 4, (UInt16)(((v >> 11) & 0x7ff) | 0xf000))
      SetUi16a(p - 2, (UInt16)(v | 0xf800))
      /*
      p[-4] = (Byte)(v >> 11);
      p[-3] = (Byte)(0xf0 | ((v >> 19) & 0x7));
      p[-2] = (Byte)v;
      p[-1] = (Byte)(0xf8 | (v >> 8));
      */
    }
  }
  while (p < lim);
  return p;
  // armt_tail:
  // if ((Byte)((lim[1] & 0xf8)) != 0xf0) { lim += 2; }  return lim;
  // return (Byte *)(lim + ((Byte)((lim[1] ^ 0xf0) & 0xf8) == 0 ? 0 : 2));
  // return (Byte *)(lim + (((lim[1] ^ ~0xfu) & ~7u) == 0 ? 0 : 2));
  // return (Byte *)(lim + 2 - (((((unsigned)lim[1] ^ 8) + 8) >> 7) & 2));
}
Z7_BRANCH_FUNCS_IMP(BranchConv_ARMT)


// #define BR_IA64_NO_INLINE

Z7_BRANCH_FUNC_MAIN(BranchConv_IA64)
{
  // Byte *p = data;
  const Byte *lim;
  size &= ~(SizeT)15;
  lim = p + size;
  pc -= 1 << 4;
  pc >>= 4 - 1;
  // pc -= 1 << 1;
  
  for (;;)
  {
    unsigned m;
    for (;;)
    {
      if Z7_UNLIKELY(p == lim)
        return p;
      m = (unsigned)((UInt32)0x334b0000 >> (*p & 0x1e));
      p += 16;
      pc += 1 << 1;
      if (m &= 3)
        break;
    }
    {
      p += (ptrdiff_t)m * 5 - 20; // negative value is expected here.
      do
      {
        const UInt32 t =
          #if defined(MY_CPU_X86_OR_AMD64)
            // we use 32-bit load here to reduce code size on x86:
            GetUi32(p);
          #else
            GetUi16(p);
          #endif
        UInt32 z = GetUi32(p + 1) >> m;
        p += 5;
        if (((t >> m) & (0x70 << 1)) == 0
            && ((z - (0x5000000 << 1)) & (0xf000000 << 1)) == 0)
        {
          UInt32 v = (UInt32)((0x8fffff << 1) | 1) & z;
          z ^= v;
        #ifdef BR_IA64_NO_INLINE
          v |= (v & ((UInt32)1 << (23 + 1))) >> 3;
          {
            UInt32 c = pc;
            BR_CONVERT_VAL(v, c)
          }
          v &= (0x1fffff << 1) | 1;
        #else
          {
            if (encoding)
            {
              // pc &= ~(0xc00000 << 1); // we just need to clear at least 2 bits
              pc &= (0x1fffff << 1) | 1;
              v += pc;
            }
            else
            {
              // pc |= 0xc00000 << 1; // we need to set at least 2 bits
              pc |= ~(UInt32)((0x1fffff << 1) | 1);
              v -= pc;
            }
          }
          v &= ~(UInt32)(0x600000 << 1);
        #endif
          v += (0x700000 << 1);
          v &= (0x8fffff << 1) | 1;
          z |= v;
          z <<= m;
          SetUi32(p + 1 - 5, z)
        }
        m++;
      }
      while (m &= 3); // while (m < 4);
    }
  }
}
Z7_BRANCH_FUNCS_IMP(BranchConv_IA64)


#define BR_CONVERT_VAL_ENC(v)  v += BR_PC_GET;
#define BR_CONVERT_VAL_DEC(v)  v -= BR_PC_GET;

#if 1 && defined(MY_CPU_LE_UNALIGN)
  #define RISCV_USE_UNALIGNED_LOAD
#endif

#ifdef RISCV_USE_UNALIGNED_LOAD
  #define RISCV_GET_UI32(p)      GetUi32(p)
  #define RISCV_SET_UI32(p, v)   { SetUi32(p, v) }
#else
  #define RISCV_GET_UI32(p) \
    ((UInt32)GetUi16a(p) + \
    ((UInt32)GetUi16a((p) + 2) << 16))
  #define RISCV_SET_UI32(p, v) { \
    SetUi16a(p, (UInt16)(v)) \
    SetUi16a((p) + 2, (UInt16)(v >> 16)) }
#endif

#if 1 && defined(MY_CPU_LE)
  #define RISCV_USE_16BIT_LOAD
#endif

#ifdef RISCV_USE_16BIT_LOAD
  #define RISCV_LOAD_VAL(p)  GetUi16a(p)
#else
  #define RISCV_LOAD_VAL(p)  (*(p))
#endif

#define RISCV_INSTR_SIZE  2
#define RISCV_STEP_1      (4 + RISCV_INSTR_SIZE)
#define RISCV_STEP_2      4
#define RISCV_REG_VAL     (2 << 7)
#define RISCV_CMD_VAL     3
#if 1
  // for code size optimization:
  #define RISCV_DELTA_7F  0x7f
#else
  #define RISCV_DELTA_7F  0
#endif

#define RISCV_CHECK_1(v, b) \
    (((((b) - RISCV_CMD_VAL) ^ ((v) << 8)) & (0xf8000 + RISCV_CMD_VAL)) == 0)

#if 1
  #define RISCV_CHECK_2(v, r) \
    ((((v) - ((RISCV_CMD_VAL << 12) | RISCV_REG_VAL | 8)) \
           << 18) \
     < ((r) & 0x1d))
#else
  // this branch gives larger code, because
  // compilers generate larger code for big constants.
  #define RISCV_CHECK_2(v, r) \
    ((((v) - ((RISCV_CMD_VAL << 12) | RISCV_REG_VAL)) \
           & ((RISCV_CMD_VAL << 12) | RISCV_REG_VAL)) \
     < ((r) & 0x1d))
#endif


#define RISCV_SCAN_LOOP \
  Byte *lim; \
  size &= ~(SizeT)(RISCV_INSTR_SIZE - 1); \
  if (size <= 6) return p; \
  size -= 6; \
  lim = p + size; \
  BR_PC_INIT \
  for (;;) \
  { \
    UInt32 a, v; \
    /* Z7_PRAGMA_OPT_DISABLE_LOOP_UNROLL_VECTORIZE */ \
    for (;;) \
    { \
      if Z7_UNLIKELY(p >= lim) { return p; } \
      a = (RISCV_LOAD_VAL(p) ^ 0x10u) + 1; \
      if ((a & 0x77) == 0) break; \
      a = (RISCV_LOAD_VAL(p + RISCV_INSTR_SIZE) ^ 0x10u) + 1; \
      p += RISCV_INSTR_SIZE * 2; \
      if ((a & 0x77) == 0) \
      { \
        p -= RISCV_INSTR_SIZE; \
        if Z7_UNLIKELY(p >= lim) { return p; } \
        break; \
      } \
    }
// (xx6f ^ 10) + 1 = xx7f + 1 = xx80       : JAL
// (xxef ^ 10) + 1 = xxff + 1 = xx00 + 100 : JAL
// (xx17 ^ 10) + 1 = xx07 + 1 = xx08       : AUIPC
// (xx97 ^ 10) + 1 = xx87 + 1 = xx88       : AUIPC

Byte * Z7_BRANCH_CONV_ENC(RISCV)(Byte *p, SizeT size, UInt32 pc)
{
  RISCV_SCAN_LOOP
    v = a;
    a = RISCV_GET_UI32(p);
#ifndef RISCV_USE_16BIT_LOAD
    v += (UInt32)p[1] << 8;
#endif

    if ((v & 8) == 0) // JAL
    {
      if ((v - (0x100 /* - RISCV_DELTA_7F */)) & 0xd80)
      {
        p += RISCV_INSTR_SIZE;
        continue;
      }
      {
        v = ((a &    1u << 31) >> 11)
          | ((a & 0x3ff << 21) >> 20)
          | ((a &     1 << 20) >> 9)
          |  (a &  0xff << 12);
        BR_CONVERT_VAL_ENC(v)
        // ((v & 1) == 0)
        // v: bits [1 : 20] contain offset bits
#if 0 && defined(RISCV_USE_UNALIGNED_LOAD)
        a &= 0xfff;
        a |= ((UInt32)(v << 23))
          |  ((UInt32)(v <<  7) & ((UInt32)0xff << 16))
          |  ((UInt32)(v >>  5) & ((UInt32)0xf0 << 8));
        RISCV_SET_UI32(p, a)
#else // aligned
#if 0
        SetUi16a(p, (UInt16)(((v >> 5) & 0xf000) | (a & 0xfff)))
#else
        p[1] = (Byte)(((v >> 13) & 0xf0) | ((a >> 8) & 0xf));
#endif

#if 1 && defined(Z7_CPU_FAST_BSWAP_SUPPORTED) && defined(MY_CPU_LE)
        v <<= 15;
        v = Z7_BSWAP32(v);
        SetUi16a(p + 2, (UInt16)v)
#else
        p[2] = (Byte)(v >> 9);
        p[3] = (Byte)(v >> 1);
#endif
#endif // aligned
      }
      p += 4;
      continue;
    } // JAL

    {
      // AUIPC
      if (v & 0xe80)  // (not x0) and (not x2)
      {
        const UInt32 b = RISCV_GET_UI32(p + 4);
        if (RISCV_CHECK_1(v, b))
        {
          {
            const UInt32 temp = (b << 12) | (0x17 + RISCV_REG_VAL);
            RISCV_SET_UI32(p, temp)
          }
          a &= 0xfffff000;
          {
#if 1
          const int t = -1 >> 1;
          if (t != -1)
            a += (b >> 20) - ((b >> 19) & 0x1000); // arithmetic right shift emulation
          else
#endif
            a += (UInt32)((Int32)b >> 20); // arithmetic right shift (sign-extension).
          }
          BR_CONVERT_VAL_ENC(a)
#if 1 && defined(Z7_CPU_FAST_BSWAP_SUPPORTED) && defined(MY_CPU_LE)
          a = Z7_BSWAP32(a);
          RISCV_SET_UI32(p + 4, a)
#else
          SetBe32(p + 4, a)
#endif
          p += 8;
        }
        else
          p += RISCV_STEP_1;
      }
      else
      {
        UInt32 r = a >> 27;
        if (RISCV_CHECK_2(v, r))
        {
          v = RISCV_GET_UI32(p + 4);
          r = (r << 7) + 0x17 + (v & 0xfffff000);
          a = (a >> 12) | (v << 20);
          RISCV_SET_UI32(p, r)
          RISCV_SET_UI32(p + 4, a)
          p += 8;
        }
        else
          p += RISCV_STEP_2;
      }
    }
  } // for
}


Byte * Z7_BRANCH_CONV_DEC(RISCV)(Byte *p, SizeT size, UInt32 pc)
{
  RISCV_SCAN_LOOP
#ifdef RISCV_USE_16BIT_LOAD
    if ((a & 8) == 0)
    {
#else
    v = a;
    a += (UInt32)p[1] << 8;
    if ((v & 8) == 0)
    {
#endif
      // JAL
      a -= 0x100 - RISCV_DELTA_7F;
      if (a & 0xd80)
      {
        p += RISCV_INSTR_SIZE;
        continue;
      }
      {
        const UInt32 a_old = (a + (0xef - RISCV_DELTA_7F)) & 0xfff;
#if 0 // unaligned
        a = GetUi32(p);
        v = (UInt32)(a >> 23) & ((UInt32)0xff << 1)
          | (UInt32)(a >>  7) & ((UInt32)0xff << 9)
#elif 1 && defined(Z7_CPU_FAST_BSWAP_SUPPORTED) && defined(MY_CPU_LE)
        v = GetUi16a(p + 2);
        v = Z7_BSWAP32(v) >> 15
#else
        v = (UInt32)p[3] << 1
          | (UInt32)p[2] << 9
#endif
          | (UInt32)((a & 0xf000) << 5);
        BR_CONVERT_VAL_DEC(v)
        a = a_old
          | (v << 11 &    1u << 31)
          | (v << 20 & 0x3ff << 21)
          | (v <<  9 &     1 << 20)
          | (v       &  0xff << 12);
        RISCV_SET_UI32(p, a)
      }
      p += 4;
      continue;
    } // JAL

    {
      // AUIPC
      v = a;
#if 1 && defined(RISCV_USE_UNALIGNED_LOAD)
      a = GetUi32(p);
#else
      a |= (UInt32)GetUi16a(p + 2) << 16;
#endif
      if ((v & 0xe80) == 0)  // x0/x2
      {
        const UInt32 r = a >> 27;
        if (RISCV_CHECK_2(v, r))
        {
          UInt32 b;
#if 1 && defined(Z7_CPU_FAST_BSWAP_SUPPORTED) && defined(MY_CPU_LE)
          b = RISCV_GET_UI32(p + 4);
          b = Z7_BSWAP32(b);
#else
          b = GetBe32(p + 4);
#endif
          v = a >> 12;
          BR_CONVERT_VAL_DEC(b)
          a = (r << 7) + 0x17;
          a += (b + 0x800) & 0xfffff000;
          v |= b << 20;
          RISCV_SET_UI32(p, a)
          RISCV_SET_UI32(p + 4, v)
          p += 8;
        }
        else
          p += RISCV_STEP_2;
      }
      else
      {
        const UInt32 b = RISCV_GET_UI32(p + 4);
        if (!RISCV_CHECK_1(v, b))
          p += RISCV_STEP_1;
        else
        {
          v = (a & 0xfffff000) | (b >> 20);
          a = (b << 12) | (0x17 + RISCV_REG_VAL);
          RISCV_SET_UI32(p, a)
          RISCV_SET_UI32(p + 4, v)
          p += 8;
        }
      }
    }
  } // for
}

/* ==== Bra86.c ==== */
/* Bra86.c -- Branch converter for X86 code (BCJ)
2023-04-02 : Igor Pavlov : Public domain */

#include "Precomp.h"

#include "Bra.h"
#include "CpuArch.h"


#if defined(MY_CPU_SIZEOF_POINTER) \
    && ( MY_CPU_SIZEOF_POINTER == 4 \
      || MY_CPU_SIZEOF_POINTER == 8)
  #define BR_CONV_USE_OPT_PC_PTR
#endif

#ifdef BR_CONV_USE_OPT_PC_PTR
#define BR_PC_INIT  pc -= (UInt32)(SizeT)p; // (MY_uintptr_t)
#define BR_PC_GET   (pc + (UInt32)(SizeT)p)
#else
#define BR_PC_INIT  pc += (UInt32)size;
#define BR_PC_GET   (pc - (UInt32)(SizeT)(lim - p))
// #define BR_PC_INIT
// #define BR_PC_GET   (pc + (UInt32)(SizeT)(p - data))
#endif

#define BR_CONVERT_VAL(v, c) if (encoding) v += c; else v -= c;
// #define BR_CONVERT_VAL(v, c) if (!encoding) c = (UInt32)0 - c; v += c;

#define Z7_BRANCH_CONV_ST(name) z7_BranchConvSt_ ## name

#define BR86_NEED_CONV_FOR_MS_BYTE(b) ((((b) + 1) & 0xfe) == 0)

#ifdef MY_CPU_LE_UNALIGN
  #define BR86_PREPARE_BCJ_SCAN  const UInt32 v = GetUi32(p) ^ 0xe8e8e8e8;
  #define BR86_IS_BCJ_BYTE(n)    ((v & ((UInt32)0xfe << (n) * 8)) == 0)
#else
  #define BR86_PREPARE_BCJ_SCAN
  // bad for MSVC X86 (partial write to byte reg):
  #define BR86_IS_BCJ_BYTE(n)    ((p[n - 4] & 0xfe) == 0xe8)
  // bad for old MSVC (partial write to byte reg):
  // #define BR86_IS_BCJ_BYTE(n)    (((*p ^ 0xe8) & 0xfe) == 0)
#endif
 
static
Z7_FORCE_INLINE
Z7_ATTRIB_NO_VECTOR
Byte *Z7_BRANCH_CONV_ST(X86)(Byte *p, SizeT size, UInt32 pc, UInt32 *state, int encoding)
{
  if (size < 5)
    return p;
 {
  // Byte *p = data;
  const Byte *lim = p + size - 4;
  unsigned mask = (unsigned)*state;  // & 7;
#ifdef BR_CONV_USE_OPT_PC_PTR
  /* if BR_CONV_USE_OPT_PC_PTR is defined: we need to adjust (pc) for (+4),
        because call/jump offset is relative to the next instruction.
     if BR_CONV_USE_OPT_PC_PTR is not defined : we don't need to adjust (pc) for (+4),
         because  BR_PC_GET uses (pc - (lim - p)), and lim was adjusted for (-4) before.
  */
  pc += 4;
#endif
  BR_PC_INIT
  goto start;

  for (;; mask |= 4)
  {
    // cont: mask |= 4;
  start:
    if (p >= lim)
      goto fin;
    {
      BR86_PREPARE_BCJ_SCAN
      p += 4;
      if (BR86_IS_BCJ_BYTE(0))  { goto m0; }  mask >>= 1;
      if (BR86_IS_BCJ_BYTE(1))  { goto m1; }  mask >>= 1;
      if (BR86_IS_BCJ_BYTE(2))  { goto m2; }  mask = 0;
      if (BR86_IS_BCJ_BYTE(3))  { goto a3; }
    }
    goto main_loop;

  m0: p--;
  m1: p--;
  m2: p--;
    if (mask == 0)
      goto a3;
    if (p > lim)
      goto fin_p;
   
    // if (((0x17u >> mask) & 1) == 0)
    if (mask > 4 || mask == 3)
    {
      mask >>= 1;
      continue; // goto cont;
    }
    mask >>= 1;
    if (BR86_NEED_CONV_FOR_MS_BYTE(p[mask]))
      continue; // goto cont;
    // if (!BR86_NEED_CONV_FOR_MS_BYTE(p[3])) continue; // goto cont;
    {
      UInt32 v = GetUi32(p);
      UInt32 c;
      v += (1 << 24);  if (v & 0xfe000000) continue; // goto cont;
      c = BR_PC_GET;
      BR_CONVERT_VAL(v, c)
      {
        mask <<= 3;
        if (BR86_NEED_CONV_FOR_MS_BYTE(v >> mask))
        {
          v ^= (((UInt32)0x100 << mask) - 1);
          #ifdef MY_CPU_X86
          // for X86 : we can recalculate (c) to reduce register pressure
            c = BR_PC_GET;
          #endif
          BR_CONVERT_VAL(v, c)
        }
        mask = 0;
      }
      // v = (v & ((1 << 24) - 1)) - (v & (1 << 24));
      v &= (1 << 25) - 1;  v -= (1 << 24);
      SetUi32(p, v)
      p += 4;
      goto main_loop;
    }

  main_loop:
    if (p >= lim)
      goto fin;
    for (;;)
    {
      BR86_PREPARE_BCJ_SCAN
      p += 4;
      if (BR86_IS_BCJ_BYTE(0))  { goto a0; }
      if (BR86_IS_BCJ_BYTE(1))  { goto a1; }
      if (BR86_IS_BCJ_BYTE(2))  { goto a2; }
      if (BR86_IS_BCJ_BYTE(3))  { goto a3; }
      if (p >= lim)
        goto fin;
    }
  
  a0: p--;
  a1: p--;
  a2: p--;
  a3:
    if (p > lim)
      goto fin_p;
    // if (!BR86_NEED_CONV_FOR_MS_BYTE(p[3])) continue; // goto cont;
    {
      UInt32 v = GetUi32(p);
      UInt32 c;
      v += (1 << 24);  if (v & 0xfe000000) continue; // goto cont;
      c = BR_PC_GET;
      BR_CONVERT_VAL(v, c)
      // v = (v & ((1 << 24) - 1)) - (v & (1 << 24));
      v &= (1 << 25) - 1;  v -= (1 << 24);
      SetUi32(p, v)
      p += 4;
      goto main_loop;
    }
  }

fin_p:
  p--;
fin:
  // the following processing for tail is optional and can be commented
  /*
  lim += 4;
  for (; p < lim; p++, mask >>= 1)
    if ((*p & 0xfe) == 0xe8)
      break;
  */
  *state = (UInt32)mask;
  return p;
 }
}


#define Z7_BRANCH_CONV_ST_FUNC_IMP(name, m, encoding) \
Z7_NO_INLINE \
Z7_ATTRIB_NO_VECTOR \
Byte *m(name)(Byte *data, SizeT size, UInt32 pc, UInt32 *state) \
  { return Z7_BRANCH_CONV_ST(name)(data, size, pc, state, encoding); }

Z7_BRANCH_CONV_ST_FUNC_IMP(X86, Z7_BRANCH_CONV_ST_DEC, 0)
#ifndef Z7_EXTRACT_ONLY
Z7_BRANCH_CONV_ST_FUNC_IMP(X86, Z7_BRANCH_CONV_ST_ENC, 1)
#endif

/* ==== BraIA64.c ==== */
/* BraIA64.c -- Converter for IA-64 code
2023-02-20 : Igor Pavlov : Public domain */

#include "Precomp.h"

// the code was moved to Bra.c

#ifdef _MSC_VER
#pragma warning(disable : 4206) // nonstandard extension used : translation unit is empty
#endif

#if defined(__clang__)
#pragma GCC diagnostic ignored "-Wempty-translation-unit"
#endif

/* ==== Delta.c ==== */
/* Delta.c -- Delta converter
2021-02-09 : Igor Pavlov : Public domain */

#include "Precomp.h"

#include "Delta.h"

void Delta_Init(Byte *state)
{
  unsigned i;
  for (i = 0; i < DELTA_STATE_SIZE; i++)
    state[i] = 0;
}


void Delta_Encode(Byte *state, unsigned delta, Byte *data, SizeT size)
{
  Byte temp[DELTA_STATE_SIZE];

  if (size == 0)
    return;

  {
    unsigned i = 0;
    do
      temp[i] = state[i];
    while (++i != delta);
  }

  if (size <= delta)
  {
    unsigned i = 0, k;
    do
    {
      Byte b = *data;
      *data++ = (Byte)(b - temp[i]);
      temp[i] = b;
    }
    while (++i != size);
    
    k = 0;
    
    do
    {
      if (i == delta)
        i = 0;
      state[k] = temp[i++];
    }
    while (++k != delta);
    
    return;
  }
    
  {
    Byte *p = data + size - delta;
    {
      unsigned i = 0;
      do
        state[i] = *p++;
      while (++i != delta);
    }
    {
      const Byte *lim = data + delta;
      ptrdiff_t dif = -(ptrdiff_t)delta;
      
      if (((ptrdiff_t)size + dif) & 1)
      {
        --p;  *p = (Byte)(*p - p[dif]);
      }

      while (p != lim)
      {
        --p;  *p = (Byte)(*p - p[dif]);
        --p;  *p = (Byte)(*p - p[dif]);
      }
      
      dif = -dif;
      
      do
      {
        --p;  *p = (Byte)(*p - temp[--dif]);
      }
      while (dif != 0);
    }
  }
}


void Delta_Decode(Byte *state, unsigned delta, Byte *data, SizeT size)
{
  unsigned i;
  const Byte *lim;

  if (size == 0)
    return;
  
  i = 0;
  lim = data + size;
  
  if (size <= delta)
  {
    do
      *data = (Byte)(*data + state[i++]);
    while (++data != lim);

    for (; delta != i; state++, delta--)
      *state = state[i];
    data -= i;
  }
  else
  {
    /*
    #define B(n) b ## n
    #define I(n) Byte B(n) = state[n];
    #define U(n) { B(n) = (Byte)((B(n)) + *data++); data[-1] = (B(n)); }
    #define F(n) if (data != lim) { U(n) }

    if (delta == 1)
    {
      I(0)
      if ((lim - data) & 1) { U(0) }
      while (data != lim) { U(0) U(0) }
      data -= 1;
    }
    else if (delta == 2)
    {
      I(0) I(1)
      lim -= 1; while (data < lim) { U(0) U(1) }
      lim += 1; F(0)
      data -= 2;
    }
    else if (delta == 3)
    {
      I(0) I(1) I(2)
      lim -= 2; while (data < lim) { U(0) U(1) U(2) }
      lim += 2; F(0) F(1)
      data -= 3;
    }
    else if (delta == 4)
    {
      I(0) I(1) I(2) I(3)
      lim -= 3; while (data < lim) { U(0) U(1) U(2) U(3) }
      lim += 3; F(0) F(1) F(2)
      data -= 4;
    }
    else
    */
    {
      do
      {
        *data = (Byte)(*data + state[i++]);
        data++;
      }
      while (i != delta);
  
      {
        ptrdiff_t dif = -(ptrdiff_t)delta;
        do
          *data = (Byte)(*data + data[dif]);
        while (++data != lim);
        data += dif;
      }
    }
  }

  do
    *state++ = *data;
  while (++data != lim);
}

/* ==== Bcj2.c ==== */
/* Bcj2.c -- BCJ2 Decoder (Converter for x86 code)
2023-03-01 : Igor Pavlov : Public domain */

#include "Precomp.h"

#include "Bcj2.h"
#include "CpuArch.h"

#define kTopValue ((UInt32)1 << 24)
#define kNumBitModelTotalBits 11
#define kBitModelTotal (1 << kNumBitModelTotalBits)
#define kNumMoveBits 5

// UInt32 bcj2_stats[256 + 2][2];

void Bcj2Dec_Init(CBcj2Dec *p)
{
  unsigned i;
  p->state = BCJ2_STREAM_RC; // BCJ2_DEC_STATE_OK;
  p->ip = 0;
  p->temp = 0;
  p->range = 0;
  p->code = 0;
  for (i = 0; i < sizeof(p->probs) / sizeof(p->probs[0]); i++)
    p->probs[i] = kBitModelTotal >> 1;
}

SRes Bcj2Dec_Decode(CBcj2Dec *p)
{
  UInt32 v = p->temp;
  // const Byte *src;
  if (p->range <= 5)
  {
    UInt32 code = p->code;
    p->state = BCJ2_DEC_STATE_ERROR; /* for case if we return SZ_ERROR_DATA; */
    for (; p->range != 5; p->range++)
    {
      if (p->range == 1 && code != 0)
        return SZ_ERROR_DATA;
      if (p->bufs[BCJ2_STREAM_RC] == p->lims[BCJ2_STREAM_RC])
      {
        p->state = BCJ2_STREAM_RC;
        return SZ_OK;
      }
      code = (code << 8) | *(p->bufs[BCJ2_STREAM_RC])++;
      p->code = code;
    }
    if (code == 0xffffffff)
      return SZ_ERROR_DATA;
    p->range = 0xffffffff;
  }
  // else
  {
    unsigned state = p->state;
    // we check BCJ2_IS_32BIT_STREAM() here instead of check in the main loop
    if (BCJ2_IS_32BIT_STREAM(state))
    {
      const Byte *cur = p->bufs[state];
      if (cur == p->lims[state])
        return SZ_OK;
      p->bufs[state] = cur + 4;
      {
        const UInt32 ip = p->ip + 4;
        v = GetBe32a(cur) - ip;
        p->ip = ip;
      }
      state = BCJ2_DEC_STATE_ORIG_0;
    }
    if ((unsigned)(state - BCJ2_DEC_STATE_ORIG_0) < 4)
    {
      Byte *dest = p->dest;
      for (;;)
      {
        if (dest == p->destLim)
        {
          p->state = state;
          p->temp = v;
          return SZ_OK;
        }
        *dest++ = (Byte)v;
        p->dest = dest;
        if (++state == BCJ2_DEC_STATE_ORIG_3 + 1)
          break;
        v >>= 8;
      }
    }
  }

  // src = p->bufs[BCJ2_STREAM_MAIN];
  for (;;)
  {
    /*
    if (BCJ2_IS_32BIT_STREAM(p->state))
      p->state = BCJ2_DEC_STATE_OK;
    else
    */
    {
      if (p->range < kTopValue)
      {
        if (p->bufs[BCJ2_STREAM_RC] == p->lims[BCJ2_STREAM_RC])
        {
          p->state = BCJ2_STREAM_RC;
          p->temp = v;
          return SZ_OK;
        }
        p->range <<= 8;
        p->code = (p->code << 8) | *(p->bufs[BCJ2_STREAM_RC])++;
      }
      {
        const Byte *src = p->bufs[BCJ2_STREAM_MAIN];
        const Byte *srcLim;
        Byte *dest = p->dest;
        {
          const SizeT rem = (SizeT)(p->lims[BCJ2_STREAM_MAIN] - src);
          SizeT num = (SizeT)(p->destLim - dest);
          if (num >= rem)
            num = rem;
        #define NUM_ITERS 4
        #if (NUM_ITERS & (NUM_ITERS - 1)) == 0
          num &= ~((SizeT)NUM_ITERS - 1);   // if (NUM_ITERS == (1 << x))
        #else
          num -= num % NUM_ITERS; // if (NUM_ITERS != (1 << x))
        #endif
          srcLim = src + num;
        }

        #define NUM_SHIFT_BITS  24
        #define ONE_ITER(indx) { \
          const unsigned b = src[indx]; \
          *dest++ = (Byte)b; \
          v = (v << NUM_SHIFT_BITS) | b; \
          if (((b + (0x100 - 0xe8)) & 0xfe) == 0) break; \
          if (((v - (((UInt32)0x0f << (NUM_SHIFT_BITS)) + 0x80)) & \
              ((((UInt32)1 << (4 + NUM_SHIFT_BITS)) - 0x1) << 4)) == 0) break; \
            /* ++dest */; /* v = b; */ }
          
        if (src != srcLim)
        for (;;)
        {
            /* The dependency chain of 2-cycle for (v) calculation is not big problem here.
               But we can remove dependency chain with v = b in the end of loop. */
          ONE_ITER(0)
          #if (NUM_ITERS > 1)
            ONE_ITER(1)
          #if (NUM_ITERS > 2)
            ONE_ITER(2)
          #if (NUM_ITERS > 3)
            ONE_ITER(3)
          #if (NUM_ITERS > 4)
            ONE_ITER(4)
          #if (NUM_ITERS > 5)
            ONE_ITER(5)
          #if (NUM_ITERS > 6)
            ONE_ITER(6)
          #if (NUM_ITERS > 7)
            ONE_ITER(7)
          #endif
          #endif
          #endif
          #endif
          #endif
          #endif
          #endif
          
          src += NUM_ITERS;
          if (src == srcLim)
            break;
        }

        if (src == srcLim)
      #if (NUM_ITERS > 1)
        for (;;)
      #endif
        {
        #if (NUM_ITERS > 1)
          if (src == p->lims[BCJ2_STREAM_MAIN] || dest == p->destLim)
        #endif
          {
            const SizeT num = (SizeT)(src - p->bufs[BCJ2_STREAM_MAIN]);
            p->bufs[BCJ2_STREAM_MAIN] = src;
            p->dest = dest;
            p->ip += (UInt32)num;
            /* state BCJ2_STREAM_MAIN has more priority than BCJ2_STATE_ORIG */
            p->state =
              src == p->lims[BCJ2_STREAM_MAIN] ?
                (unsigned)BCJ2_STREAM_MAIN :
                (unsigned)BCJ2_DEC_STATE_ORIG;
            p->temp = v;
            return SZ_OK;
          }
        #if (NUM_ITERS > 1)
          ONE_ITER(0)
          src++;
        #endif
        }

        {
          const SizeT num = (SizeT)(dest - p->dest);
          p->dest = dest; // p->dest += num;
          p->bufs[BCJ2_STREAM_MAIN] += num; // = src;
          p->ip += (UInt32)num;
        }
        {
          UInt32 bound, ttt;
          CBcj2Prob *prob; // unsigned index;
          /*
          prob = p->probs + (unsigned)((Byte)v == 0xe8 ?
              2 + (Byte)(v >> 8) :
              ((v >> 5) & 1));  // ((Byte)v < 0xe8 ? 0 : 1));
          */
          {
            const unsigned c = ((v + 0x17) >> 6) & 1;
            prob = p->probs + (unsigned)
                (((0 - c) & (Byte)(v >> NUM_SHIFT_BITS)) + c + ((v >> 5) & 1));
                // (Byte)
                // 8x->0     : e9->1     : xxe8->xx+2
                // 8x->0x100 : e9->0x101 : xxe8->xx
                // (((0x100 - (e & ~v)) & (0x100 | (v >> 8))) + (e & v));
                // (((0x101 + (~e | v)) & (0x100 | (v >> 8))) + (e & v));
          }
          ttt = *prob;
          bound = (p->range >> kNumBitModelTotalBits) * ttt;
          if (p->code < bound)
          {
            // bcj2_stats[prob - p->probs][0]++;
            p->range = bound;
            *prob = (CBcj2Prob)(ttt + ((kBitModelTotal - ttt) >> kNumMoveBits));
            continue;
          }
          {
            // bcj2_stats[prob - p->probs][1]++;
            p->range -= bound;
            p->code -= bound;
            *prob = (CBcj2Prob)(ttt - (ttt >> kNumMoveBits));
          }
        }
      }
    }
    {
      /* (v == 0xe8 ? 0 : 1) uses setcc instruction with additional zero register usage in x64 MSVC. */
      // const unsigned cj = ((Byte)v == 0xe8) ? BCJ2_STREAM_CALL : BCJ2_STREAM_JUMP;
      const unsigned cj = (((v + 0x57) >> 6) & 1) + BCJ2_STREAM_CALL;
      const Byte *cur = p->bufs[cj];
      Byte *dest;
      SizeT rem;
      if (cur == p->lims[cj])
      {
        p->state = cj;
        break;
      }
      v = GetBe32a(cur);
      p->bufs[cj] = cur + 4;
      {
        const UInt32 ip = p->ip + 4;
        v -= ip;
        p->ip = ip;
      }
      dest = p->dest;
      rem = (SizeT)(p->destLim - dest);
      if (rem < 4)
      {
        if ((unsigned)rem > 0) { dest[0] = (Byte)v;  v >>= 8;
        if ((unsigned)rem > 1) { dest[1] = (Byte)v;  v >>= 8;
        if ((unsigned)rem > 2) { dest[2] = (Byte)v;  v >>= 8; }}}
        p->temp = v;
        p->dest = dest + rem;
        p->state = BCJ2_DEC_STATE_ORIG_0 + (unsigned)rem;
        break;
      }
      SetUi32(dest, v)
      v >>= 24;
      p->dest = dest + 4;
    }
  }

  if (p->range < kTopValue && p->bufs[BCJ2_STREAM_RC] != p->lims[BCJ2_STREAM_RC])
  {
    p->range <<= 8;
    p->code = (p->code << 8) | *(p->bufs[BCJ2_STREAM_RC])++;
  }
  return SZ_OK;
}

#undef NUM_ITERS
#undef ONE_ITER
#undef NUM_SHIFT_BITS
#undef kTopValue
#undef kNumBitModelTotalBits
#undef kBitModelTotal
#undef kNumMoveBits

/* ==== 7zArcIn.c ==== */
/* 7zArcIn.c -- 7z Input functions
: Igor Pavlov : Public domain */

#include "Precomp.h"

#include <string.h>

#include "7z.h"
#include "7zBuf.h"
#include "7zCrc.h"
#include "CpuArch.h"

#define MY_ALLOC(T, p, size, alloc) \
  { if ((p = (T *)ISzAlloc_Alloc(alloc, (size_t)(size) * sizeof(T))) == NULL) return SZ_ERROR_MEM; }

#define MY_ALLOC_ZE(T, p, size, alloc) \
  { if ((size) == 0) p = NULL; else MY_ALLOC(T, p, size, alloc) }

#define MY_ALLOC_AND_CPY(to, size, from, alloc) \
  { MY_ALLOC(Byte, to, size, alloc); memcpy(to, from, size); }

#define MY_ALLOC_ZE_AND_CPY(to, size, from, alloc) \
  { if ((size) == 0) to = NULL; else { MY_ALLOC_AND_CPY(to, size, from, alloc) } }

enum EIdEnum
{
  k7zIdEnd,
  k7zIdHeader,
  k7zIdArchiveProperties,
  k7zIdAdditionalStreamsInfo,
  k7zIdMainStreamsInfo,
  k7zIdFilesInfo,
  k7zIdPackInfo,
  k7zIdUnpackInfo,
  k7zIdSubStreamsInfo,
  k7zIdSize,
  k7zIdCRC,
  k7zIdFolder,
  k7zIdCodersUnpackSize,
  k7zIdNumUnpackStream,
  k7zIdEmptyStream,
  k7zIdEmptyFile,
  k7zIdAnti,
  k7zIdName,
  k7zIdCTime,
  k7zIdATime,
  k7zIdMTime,
  k7zIdWinAttrib,
  k7zIdComment,
  k7zIdEncodedHeader,
  k7zIdStartPos,
  k7zIdDummy
};

const Byte k7zSignature[k7zSignatureSize] = {'7', 'z', 0xBC, 0xAF, 0x27, 0x1C};

#define SzBitUi32s_INIT(p) { (p)->Defs = NULL; (p)->Vals = NULL; }

Z7_FORCE_INLINE
static SRes SzBitUi32s_Alloc(CSzBitUi32s *p, size_t num, ISzAllocPtr alloc)
{
  if (num == 0)
  {
    p->Defs = NULL;
    p->Vals = NULL;
  }
  else
  {
    MY_ALLOC(Byte, p->Defs, (num + 7) >> 3, alloc)
    MY_ALLOC(UInt32, p->Vals, num, alloc)
  }
  return SZ_OK;
}

Z7_FORCE_INLINE
static void SzBitUi32s_Free(CSzBitUi32s *p, ISzAllocPtr alloc)
{
  ISzAlloc_Free(alloc, p->Defs); p->Defs = NULL;
  ISzAlloc_Free(alloc, p->Vals); p->Vals = NULL;
}

#define SzBitUi64s_INIT(p) { (p)->Defs = NULL; (p)->Vals = NULL; }

Z7_FORCE_INLINE
static void SzBitUi64s_Free(CSzBitUi64s *p, ISzAllocPtr alloc)
{
  ISzAlloc_Free(alloc, p->Defs); p->Defs = NULL;
  ISzAlloc_Free(alloc, p->Vals); p->Vals = NULL;
}

Z7_NO_INLINE
static void SzAr_Init(CSzAr *p)
{
  p->NumPackStreams = 0;
  p->NumFolders = 0;
  
  p->PackPositions = NULL;
  SzBitUi32s_INIT(&p->FolderCRCs)

  p->FoCodersOffsets = NULL;
  p->FoStartPackStreamIndex = NULL;
  p->FoToCoderUnpackSizes = NULL;
  p->FoToMainUnpackSizeIndex = NULL;
  p->CoderUnpackSizes = NULL;

  p->CodersData = NULL;

  p->RangeLimit = 0;
}

Z7_NO_INLINE
static void SzAr_Free(CSzAr *p, ISzAllocPtr alloc)
{
  ISzAlloc_Free(alloc, p->PackPositions);
  SzBitUi32s_Free(&p->FolderCRCs, alloc);
  ISzAlloc_Free(alloc, p->FoCodersOffsets);
  ISzAlloc_Free(alloc, p->FoStartPackStreamIndex);
  ISzAlloc_Free(alloc, p->FoToCoderUnpackSizes);
  ISzAlloc_Free(alloc, p->FoToMainUnpackSizeIndex);
  ISzAlloc_Free(alloc, p->CoderUnpackSizes);
  ISzAlloc_Free(alloc, p->CodersData);

  SzAr_Init(p);
}

Z7_NO_INLINE
void SzArEx_Init(CSzArEx *p)
{
  SzAr_Init(&p->db);
  
  p->NumFiles = 0;
  p->dataPos = 0;
  
  p->UnpackPositions = NULL;
  p->IsDirs = NULL;
  
  p->FolderToFile = NULL;
  p->FileToFolder = NULL;
  
  p->FileNameOffsets = NULL;
  p->FileNames = NULL;
  
  SzBitUi32s_INIT(&p->CRCs)
  SzBitUi32s_INIT(&p->Attribs)
  // SzBitUi32s_INIT(&p->Parents)
  SzBitUi64s_INIT(&p->MTime)
  SzBitUi64s_INIT(&p->CTime)
}

Z7_NO_INLINE
void SzArEx_Free(CSzArEx *p, ISzAllocPtr alloc)
{
  ISzAlloc_Free(alloc, p->UnpackPositions);
  ISzAlloc_Free(alloc, p->IsDirs);

  ISzAlloc_Free(alloc, p->FolderToFile);
  ISzAlloc_Free(alloc, p->FileToFolder);

  ISzAlloc_Free(alloc, p->FileNameOffsets);
  ISzAlloc_Free(alloc, p->FileNames);

  SzBitUi32s_Free(&p->CRCs, alloc);
  SzBitUi32s_Free(&p->Attribs, alloc);
  // SzBitUi32s_Free(&p->Parents, alloc);
  SzBitUi64s_Free(&p->MTime, alloc);
  SzBitUi64s_Free(&p->CTime, alloc);
  
  SzAr_Free(&p->db, alloc);
  SzArEx_Init(p);
}


#define SzData_CLEAR(p) { (p)->Data = NULL; (p)->Size = 0; }

#define SZ_READ_BYTE_SD_NOCHECK(_sd_, dest) \
    (_sd_)->Size--; dest = *(_sd_)->Data++;

#define SZ_READ_BYTE_SD(_sd_, dest) \
    if ((_sd_)->Size == 0) return SZ_ERROR_ARCHIVE; \
    SZ_READ_BYTE_SD_NOCHECK(_sd_, dest)

#define SZ_READ_BYTE(dest) SZ_READ_BYTE_SD(sd, dest)

#define SZ_READ_BYTE_2(dest) \
    if (sd.Size == 0) return SZ_ERROR_ARCHIVE; \
    sd.Size--; dest = *sd.Data++;

#define SKIP_DATA(sd, size) { sd->Size -= (size_t)(size); sd->Data += (size_t)(size); }
#define SKIP_DATA2(sd, size) { sd.Size -= (size_t)(size); sd.Data += (size_t)(size); }

static Z7_NO_INLINE SRes ReadNumber(CSzData *sd, UInt64 *value)
{
  unsigned firstByte, mask, i, v;

  SZ_READ_BYTE(firstByte)
  if ((firstByte & 0x80) == 0)
  {
    *value = firstByte;
    return SZ_OK;
  }
  SZ_READ_BYTE(v)
  if ((firstByte & 0x40) == 0)
  {
    *value = (((UInt32)firstByte & 0x3F) << 8) | v;
    return SZ_OK;
  }
  SZ_READ_BYTE(mask)
  *value = v | ((UInt32)mask << 8);
  mask = 0x20;
  for (i = 2 * 8; i < 8 * 8; i += 8)
  {
    unsigned b;
    if ((firstByte & mask) == 0)
    {
      const UInt64 highPart = (unsigned)firstByte & (unsigned)(mask - 1);
      *value |= (highPart << i);
      return SZ_OK;
    }
    mask >>= 1;
    SZ_READ_BYTE(b)
    *value |= ((UInt64)b << i);
  }
  return SZ_OK;
}


static Z7_NO_INLINE SRes SzReadNumber32(CSzData *sd, UInt32 *value)
{
  unsigned firstByte;
  UInt64 value64;
  if (sd->Size == 0)
    return SZ_ERROR_ARCHIVE;
  firstByte = *sd->Data;
  if ((firstByte & 0x80) == 0)
  {
    *value = firstByte;
    sd->Data++;
    sd->Size--;
    return SZ_OK;
  }
  RINOK(ReadNumber(sd, &value64))
  if (value64 >= (UInt32)0x80000000 - 1)
    return SZ_ERROR_UNSUPPORTED;
  if (value64 >= ((UInt64)(1) << ((sizeof(size_t) - 1) * 8 + 4)))
    return SZ_ERROR_UNSUPPORTED;
  *value = (UInt32)value64;
  return SZ_OK;
}

#define ReadID(sd, value) ReadNumber(sd, value)

Z7_NO_INLINE
static SRes SkipData(CSzData *sd)
{
  UInt64 size;
  RINOK(ReadNumber(sd, &size))
  if (size > sd->Size)
    return SZ_ERROR_ARCHIVE;
  SKIP_DATA(sd, size)
  return SZ_OK;
}

Z7_NO_INLINE
static SRes WaitId(CSzData *sd, UInt32 id)
{
  for (;;)
  {
    UInt64 type;
    RINOK(ReadID(sd, &type))
    if (type == id)
      return SZ_OK;
    if (type == k7zIdEnd)
      return SZ_ERROR_ARCHIVE;
    RINOK(SkipData(sd))
  }
}


Z7_NO_INLINE
static UInt32 CountDefinedBits(const Byte *bits, UInt32 numItems)
{
  unsigned b = 0;
  unsigned m = 0;
  UInt32 sum = 0;
  for (; numItems != 0; numItems--)
  {
    if (m == 0)
    {
      b = *bits++;
      m = 8;
    }
    m--;
    sum += (UInt32)((b >> m) & 1);
  }
  return sum;
}

static Z7_NO_INLINE SRes ReadBitVector(CSzData *sd, size_t numItems, Byte **v, ISzAllocPtr alloc)
{
  Byte allAreDefined;
  Byte *v2;
  const size_t numBytes = (numItems + 7) >> 3;
  *v = NULL;
  SZ_READ_BYTE(allAreDefined)
  if (numBytes == 0)
    return SZ_OK;
  if (allAreDefined == 0)
  {
    if (numBytes > sd->Size)
      return SZ_ERROR_ARCHIVE;
    MY_ALLOC_AND_CPY(*v, numBytes, sd->Data, alloc)
    SKIP_DATA(sd, numBytes)
    return SZ_OK;
  }
  MY_ALLOC(Byte, *v, numBytes, alloc)
  v2 = *v;
  memset(v2, 0xFF, (size_t)numBytes);
  {
    const unsigned numBits = (unsigned)numItems & 7;
    if (numBits)
      v2[(size_t)numBytes - 1] = (Byte)(0xff00u >> numBits);
  }
  return SZ_OK;
}

static Z7_NO_INLINE SRes ReadUi32s(CSzData *sd2, size_t numItems, CSzBitUi32s *crcs, ISzAllocPtr alloc)
{
  size_t i;
  const Byte *data;
  size_t size;
  UInt32 *vals;
  const Byte *defs;
  MY_ALLOC_ZE(UInt32, vals, numItems, alloc)
  crcs->Vals = vals;
  defs = crcs->Defs;
  data = sd2->Data;
  size = sd2->Size;
  for (i = 0; i < numItems; i++)
    if (SzBitArray_Check(defs, i))
    {
      if (size < 4)
        return SZ_ERROR_ARCHIVE;
      size -= 4;
      vals[i] = GetUi32(data);
      data += 4;
    }
    else
      vals[i] = 0;
  sd2->Data = data;
  sd2->Size = size;
  return SZ_OK;
}

static SRes ReadBitUi32s(CSzData *sd, size_t numItems, CSzBitUi32s *crcs, ISzAllocPtr alloc)
{
  if (crcs->Defs)
    return SZ_ERROR_ARCHIVE;
  RINOK(ReadBitVector(sd, numItems, &crcs->Defs, alloc))
  return ReadUi32s(sd, numItems, crcs, alloc);
}

Z7_NO_INLINE
static SRes SkipBitUi32s(CSzData *sd, UInt32 numItems)
{
  Byte allAreDefined;
  UInt32 numDefined = numItems;
  SZ_READ_BYTE(allAreDefined)
  if (!allAreDefined)
  {
    const size_t numBytes = (numItems + 7) >> 3;
    if (numBytes > sd->Size)
      return SZ_ERROR_ARCHIVE;
    numDefined = CountDefinedBits(sd->Data, numItems);
    SKIP_DATA(sd, numBytes)
  }
  if (numDefined > (sd->Size >> 2))
    return SZ_ERROR_ARCHIVE;
  SKIP_DATA(sd, (size_t)numDefined * 4)
  return SZ_OK;
}

static SRes ReadPackInfo(CSzAr *p, CSzData *sd, ISzAllocPtr alloc)
{
  RINOK(SzReadNumber32(sd, &p->NumPackStreams))
  RINOK(WaitId(sd, k7zIdSize))
  {
    UInt64 *packPositions;
    UInt64 sum;
    size_t num = (size_t)p->NumPackStreams + 1;
    MY_ALLOC(UInt64, packPositions, num, alloc)
    p->PackPositions = packPositions;
    sum = 0;
    for (;;)
    {
      UInt64 packSize;
      *packPositions++ = sum;
      if (--num == 0)
        break;
      RINOK(ReadNumber(sd, &packSize))
      sum += packSize;
      if (sum < packSize)
        return SZ_ERROR_ARCHIVE;
    }
  }
  for (;;)
  {
    UInt64 type;
    RINOK(ReadID(sd, &type))
    if (type == k7zIdEnd)
      return SZ_OK;
    if (type == k7zIdCRC)
    {
      /* CRC of packed streams is unused now */
      RINOK(SkipBitUi32s(sd, p->NumPackStreams))
      continue;
    }
    RINOK(SkipData(sd))
  }
}


#define k_NumCodersStreams_in_Folder_MAX (SZ_NUM_BONDS_IN_FOLDER_MAX + SZ_NUM_PACK_STREAMS_IN_FOLDER_MAX)

SRes SzGetNextFolderItem(CSzFolder *f, CSzData *sd)
{
  UInt32 numCoders, i, numInStreams = 0;
  const Byte * const dataStart = sd->Data;

  // f->NumCoders = 0;
  // f->NumBonds = 0;
  // f->NumPackStreams = 0;
  f->UnpackStream = 0;
  
  RINOK(SzReadNumber32(sd, &numCoders))
  if (numCoders == 0 || numCoders > SZ_NUM_CODERS_IN_FOLDER_MAX)
    return SZ_ERROR_UNSUPPORTED;
  f->NumCoders = numCoders;
  
  for (i = 0; i < numCoders; i++)
  {
    Byte mainByte;
    CSzCoderInfo *coder;
    {
      unsigned idSize, j;
      UInt64 id;
      const Byte *data;
      
      if (!sd->Size)
        return SZ_ERROR_ARCHIVE;
      sd->Size--;
      data = sd->Data;
      mainByte = *data++;
      if (mainByte & 0xC0)
        return SZ_ERROR_UNSUPPORTED;
      idSize = mainByte & 0xF;
      if (idSize > sizeof(id))
        return SZ_ERROR_UNSUPPORTED;
      if (idSize > sd->Size)
        return SZ_ERROR_ARCHIVE;
      sd->Size -= idSize;
      id = 0;
      for (j = 0; j < idSize; j++)
        id = (id << 8) | *data++;
      sd->Data = data;
      if (id > (UInt32)0xFFFFFFFF)
        return SZ_ERROR_UNSUPPORTED;
      coder = f->Coders + i;
      coder->MethodID = (UInt32)id;
    }
    
    coder->NumStreams = 1;
    coder->PropsOffset = 0;
    coder->PropsSize = 0;
    
    if (mainByte & 0x10)
    {
      UInt32 numStreams;
      RINOK(SzReadNumber32(sd, &numStreams))
      if (numStreams > k_NumCodersStreams_in_Folder_MAX)
        return SZ_ERROR_UNSUPPORTED;
      coder->NumStreams = (Byte)numStreams;
      RINOK(SzReadNumber32(sd, &numStreams))
      if (numStreams != 1)
        return SZ_ERROR_UNSUPPORTED;
    }

    numInStreams += coder->NumStreams;
    if (numInStreams > k_NumCodersStreams_in_Folder_MAX)
      return SZ_ERROR_UNSUPPORTED;

    if (mainByte & 0x20)
    {
      UInt32 propsSize;
      RINOK(SzReadNumber32(sd, &propsSize))
      if (propsSize > sd->Size)
        return SZ_ERROR_ARCHIVE;
      if (propsSize >= 0x80)
        return SZ_ERROR_UNSUPPORTED;
      coder->PropsOffset = (size_t)(sd->Data - dataStart);
      coder->PropsSize = (Byte)propsSize;
      sd->Data += (size_t)propsSize;
      sd->Size -= (size_t)propsSize;
    }
  }

  {
    Byte streamUsed[k_NumCodersStreams_in_Folder_MAX];
    UInt32 numBonds, numPackStreams;
    
    numBonds = numCoders - 1;
    if (numInStreams < numBonds)
      return SZ_ERROR_ARCHIVE;
    if (numBonds > SZ_NUM_BONDS_IN_FOLDER_MAX)
      return SZ_ERROR_UNSUPPORTED;
    f->NumBonds = numBonds;
    
    numPackStreams = numInStreams - numBonds;
    if (numPackStreams > SZ_NUM_PACK_STREAMS_IN_FOLDER_MAX)
      return SZ_ERROR_UNSUPPORTED;
    f->NumPackStreams = numPackStreams;
  
    for (i = 0; i < Z7_ARRAY_SIZE(streamUsed); i++) // numInStreams
      streamUsed[i] = False;
    
    if (numBonds)
    {
      Byte coderUsed[SZ_NUM_CODERS_IN_FOLDER_MAX];
      for (i = 0; i < Z7_ARRAY_SIZE(coderUsed); i++) // numCoders
        coderUsed[i] = False;
      
      for (i = 0; i < numBonds; i++)
      {
        CSzBond *bp = f->Bonds + i;
        
        RINOK(SzReadNumber32(sd, &bp->InIndex))
        if (bp->InIndex >= numInStreams || streamUsed[bp->InIndex])
          return SZ_ERROR_ARCHIVE;
        streamUsed[bp->InIndex] = True;
        
        RINOK(SzReadNumber32(sd, &bp->OutIndex))
        if (bp->OutIndex >= numCoders || coderUsed[bp->OutIndex])
          return SZ_ERROR_ARCHIVE;
        coderUsed[bp->OutIndex] = True;
      }
      
      for (i = 0;;)
      {
        if (!coderUsed[i])
          break;
        if (++i == numCoders)
          return SZ_ERROR_ARCHIVE;
      }
      f->UnpackStream = i;
    }
    
    if (numPackStreams == 1)
    {
      for (i = 0;; i++)
      {
        if (i == numInStreams)
          return SZ_ERROR_ARCHIVE;
        if (!streamUsed[i])
          break;
      }
      f->PackStreams[0] = i;
    }
    else
      for (i = 0; i < numPackStreams; i++)
      {
        UInt32 index;
        RINOK(SzReadNumber32(sd, &index))
        if (index >= numInStreams || streamUsed[index])
          return SZ_ERROR_ARCHIVE;
        streamUsed[index] = True;
        f->PackStreams[i] = index;
      }
  }
  return SZ_OK;
}


static SRes SkipNumbers(CSzData *sd2, UInt32 num)
{
  const Byte *data = sd2->Data;
  size_t size = sd2->Size;
  for (; num != 0; num--)
  {
    unsigned firstByte;
    unsigned i;
    if (size == 0)
      return SZ_ERROR_ARCHIVE;
    firstByte = *data;
    for (i = 1; firstByte & 0x80; i++)
      firstByte <<= 1;
    if (size < i)
      return SZ_ERROR_ARCHIVE;
    size -= i;
    data += i;
  }
  sd2->Data = data;
  sd2->Size = size;
  return SZ_OK;
}


#define k_Scan_NumCoders_MAX 64
#define k_Scan_NumCodersStreams_in_Folder_MAX 64

static SRes ReadUnpackInfo(CSzAr *p, CSzData *sd2, const UInt32 numFoldersMax,
    const CBuf *tempBufs, UInt32 numTempBufs, ISzAllocPtr alloc)
{
  CSzData sd;
  UInt32 fo, numFolders, numCodersOutStreams, packStreamIndex;
  const Byte *startBufPtr;
  Byte external;
  
  RINOK(WaitId(sd2, k7zIdFolder))
  
  RINOK(SzReadNumber32(sd2, &numFolders))
  if (numFolders > numFoldersMax)
    return SZ_ERROR_UNSUPPORTED;
  p->NumFolders = numFolders;

  SZ_READ_BYTE_SD(sd2, external)
  if (external == 0)
    sd = *sd2;
  else
  {
    UInt32 index;
    RINOK(SzReadNumber32(sd2, &index))
    if (index >= numTempBufs)
      return SZ_ERROR_ARCHIVE;
    sd.Data = tempBufs[index].data;
    sd.Size = tempBufs[index].size;
  }
  
  MY_ALLOC(size_t, p->FoCodersOffsets, (size_t)numFolders + 1, alloc)
  MY_ALLOC(UInt32, p->FoStartPackStreamIndex, (size_t)numFolders + 1, alloc)
  MY_ALLOC(UInt32, p->FoToCoderUnpackSizes, (size_t)numFolders + 1, alloc)
  MY_ALLOC_ZE(Byte, p->FoToMainUnpackSizeIndex, (size_t)numFolders, alloc)
  
  startBufPtr = sd.Data;
  
  packStreamIndex = 0;
  numCodersOutStreams = 0;

  for (fo = 0; fo < numFolders; fo++)
  {
    UInt32 numCoders, ci, numInStreams = 0;
    
    p->FoCodersOffsets[fo] = (size_t)(sd.Data - startBufPtr);
    
    RINOK(SzReadNumber32(&sd, &numCoders))
    if (numCoders == 0 || numCoders > k_Scan_NumCoders_MAX)
      return SZ_ERROR_UNSUPPORTED;
    
    for (ci = 0; ci < numCoders; ci++)
    {
      Byte mainByte;
      unsigned idSize;
      UInt32 coderInStreams;
      
      SZ_READ_BYTE_2(mainByte)
      if ((mainByte & 0xC0) != 0)
        return SZ_ERROR_UNSUPPORTED;
      idSize = (mainByte & 0xF);
      if (idSize > 8)
        return SZ_ERROR_UNSUPPORTED;
      if (idSize > sd.Size)
        return SZ_ERROR_ARCHIVE;
      SKIP_DATA2(sd, idSize)
      
      coderInStreams = 1;
      
      if ((mainByte & 0x10) != 0)
      {
        UInt32 coderOutStreams;
        RINOK(SzReadNumber32(&sd, &coderInStreams))
        RINOK(SzReadNumber32(&sd, &coderOutStreams))
        if (coderInStreams > k_Scan_NumCodersStreams_in_Folder_MAX || coderOutStreams != 1)
          return SZ_ERROR_UNSUPPORTED;
      }
      
      numInStreams += coderInStreams;

      if ((mainByte & 0x20) != 0)
      {
        UInt32 propsSize;
        RINOK(SzReadNumber32(&sd, &propsSize))
        if (propsSize > sd.Size)
          return SZ_ERROR_ARCHIVE;
        SKIP_DATA2(sd, propsSize)
      }
    }
    
    {
      UInt32 indexOfMainStream = 0;
      UInt32 numPackStreams = 1;
      
      if (numCoders != 1 || numInStreams != 1)
      {
        Byte streamUsed[k_Scan_NumCodersStreams_in_Folder_MAX];
        Byte coderUsed[k_Scan_NumCoders_MAX];
        UInt32 i;
        const UInt32 numBonds = numCoders - 1;
        if (numInStreams < numBonds)
          return SZ_ERROR_ARCHIVE;
        if (numInStreams > k_Scan_NumCodersStreams_in_Folder_MAX)
          return SZ_ERROR_UNSUPPORTED;
        for (i = 0; i < numInStreams; i++)
          streamUsed[i] = False;
        for (i = 0; i < numCoders; i++)
          coderUsed[i] = False;
        
        for (i = 0; i < numBonds; i++)
        {
          UInt32 index;
          
          RINOK(SzReadNumber32(&sd, &index))
          if (index >= numInStreams || streamUsed[index])
            return SZ_ERROR_ARCHIVE;
          streamUsed[index] = True;
          
          RINOK(SzReadNumber32(&sd, &index))
          if (index >= numCoders || coderUsed[index])
            return SZ_ERROR_ARCHIVE;
          coderUsed[index] = True;
        }
        
        numPackStreams = numInStreams - numBonds;
        
        if (numPackStreams != 1)
          for (i = 0; i < numPackStreams; i++)
          {
            UInt32 index;
            RINOK(SzReadNumber32(&sd, &index))
            if (index >= numInStreams || streamUsed[index])
              return SZ_ERROR_ARCHIVE;
            streamUsed[index] = True;
          }

        for (i = 0;;)
        {
          if (!coderUsed[i])
            break;
          if (++i == numCoders)
            return SZ_ERROR_ARCHIVE;
        }
        indexOfMainStream = i;
      }
      
      p->FoStartPackStreamIndex[fo] = packStreamIndex;
      p->FoToCoderUnpackSizes[fo] = numCodersOutStreams;
      p->FoToMainUnpackSizeIndex[fo] = (Byte)indexOfMainStream;
      numCodersOutStreams += numCoders;
      if (numCodersOutStreams < numCoders)
        return SZ_ERROR_UNSUPPORTED;
      if (numPackStreams > p->NumPackStreams - packStreamIndex)
        return SZ_ERROR_ARCHIVE;
      packStreamIndex += numPackStreams;
    }
  }

  {
    const size_t k_numCodersOutStreams_Limit = (size_t)1 << (sizeof(size_t) * 8 - 4);
    if (numCodersOutStreams >= k_numCodersOutStreams_Limit)
      return SZ_ERROR_UNSUPPORTED;
  }
  p->FoToCoderUnpackSizes[fo] = numCodersOutStreams;
  p->FoStartPackStreamIndex[fo] = packStreamIndex;
  {
    const size_t dataSize = (size_t)(sd.Data - startBufPtr);
    p->FoCodersOffsets[fo] = dataSize;
    MY_ALLOC_ZE_AND_CPY(p->CodersData, dataSize, startBufPtr, alloc)
  }
  
  if (external)
  {
    if (sd.Size)
      return SZ_ERROR_ARCHIVE;
    sd = *sd2;
  }
  
  RINOK(WaitId(&sd, k7zIdCodersUnpackSize))
  {
    UInt64 *sizes;
    MY_ALLOC_ZE(UInt64, sizes, (size_t)numCodersOutStreams, alloc)
    p->CoderUnpackSizes = sizes;
    if (numCodersOutStreams)
      do
      {
        RINOK(ReadNumber(&sd, sizes))
        sizes++;
      }
      while (--numCodersOutStreams);
  }

  for (;;)
  {
    UInt64 type;
    RINOK(ReadID(&sd, &type))
    if (type == k7zIdEnd)
      break;
    if (type == k7zIdCRC)
    {
      RINOK(ReadBitUi32s(&sd, numFolders, &p->FolderCRCs, alloc))
      continue;
    }
    RINOK(SkipData(&sd))
  }
  *sd2 = sd;
  return SZ_OK;
}


UInt64 SzAr_GetFolderUnpackSize(const CSzAr *p, UInt32 folderIndex)
{
  return p->CoderUnpackSizes[p->FoToCoderUnpackSizes[folderIndex] + p->FoToMainUnpackSizeIndex[folderIndex]];
}


typedef struct
{
  UInt32 NumTotalSubStreams;
  UInt32 NumSubDigests;
  CSzData sdNumSubStreams;
  CSzData sdSizes;
  CSzData sdCRCs;
} CSubStreamInfo;


static SRes ReadSubStreamsInfo(const CSzAr *p, CSzData *sd, CSubStreamInfo *ssi)
{
  UInt64 type = 0;
  const UInt32 numFolders = p->NumFolders;
  UInt32 numUnpackStreams = numFolders;
  UInt32 numSubDigests = numFolders;
  UInt32 numUnpackSizesInData = 0;

  for (;;)
  {
    RINOK(ReadID(sd, &type))
    if (type == k7zIdNumUnpackStream)
    {
      UInt32 i;
      if (ssi->sdNumSubStreams.Data)
        return SZ_ERROR_UNSUPPORTED;
      ssi->sdNumSubStreams.Data = sd->Data;
      numUnpackStreams = 0;
      numSubDigests = 0;
      for (i = 0; i < numFolders; i++)
      {
        UInt32 numStreams;
        RINOK(SzReadNumber32(sd, &numStreams))
        numUnpackStreams += numStreams;
        if (numUnpackStreams < numStreams)
          return SZ_ERROR_UNSUPPORTED;
        if (numStreams)
          numUnpackSizesInData += (numStreams - 1);
        if (numStreams != 1 || !SzBitWithVals_Check(&p->FolderCRCs, i))
          numSubDigests += numStreams;
      }
      ssi->sdNumSubStreams.Size = (size_t)(sd->Data - ssi->sdNumSubStreams.Data);
      continue;
    }
    if (type == k7zIdCRC || type == k7zIdSize || type == k7zIdEnd)
      break;
    RINOK(SkipData(sd))
  }

  if (!ssi->sdNumSubStreams.Data && p->FolderCRCs.Defs)
    numSubDigests = numFolders - CountDefinedBits(p->FolderCRCs.Defs, numFolders);
  
  ssi->NumTotalSubStreams = numUnpackStreams;
  ssi->NumSubDigests = numSubDigests;

  if (type == k7zIdSize)
  {
    ssi->sdSizes.Data = sd->Data;
    RINOK(SkipNumbers(sd, numUnpackSizesInData))
    ssi->sdSizes.Size = (size_t)(sd->Data - ssi->sdSizes.Data);
    RINOK(ReadID(sd, &type))
  }

  for (;;)
  {
    if (type == k7zIdEnd)
      return SZ_OK;
    if (type == k7zIdCRC)
    {
      if (ssi->sdCRCs.Data)
        return SZ_ERROR_UNSUPPORTED;
      ssi->sdCRCs.Data = sd->Data;
      RINOK(SkipBitUi32s(sd, numSubDigests))
      ssi->sdCRCs.Size = (size_t)(sd->Data - ssi->sdCRCs.Data);
    }
    else
    {
      RINOK(SkipData(sd))
    }
    RINOK(ReadID(sd, &type))
  }
}


static SRes SzReadStreamsInfo(CSzAr *p,
    CSzData *sd,
    const UInt32 numFoldersMax,
    const CBuf * const tempBufs, const UInt32 numTempBufs,
    UInt64 *dataOffset,
    CSubStreamInfo *ssi,
    ISzAllocPtr alloc)
{
  UInt64 type;

  SzData_CLEAR(&ssi->sdSizes)
  SzData_CLEAR(&ssi->sdCRCs)
  SzData_CLEAR(&ssi->sdNumSubStreams)

  *dataOffset = 0;
  RINOK(ReadID(sd, &type))
  if (type == k7zIdPackInfo)
  {
    RINOK(ReadNumber(sd, dataOffset))
    if (*dataOffset > p->RangeLimit)
      return SZ_ERROR_ARCHIVE;
    RINOK(ReadPackInfo(p, sd, alloc))
    if (p->PackPositions[p->NumPackStreams] > p->RangeLimit - *dataOffset)
      return SZ_ERROR_ARCHIVE;
    RINOK(ReadID(sd, &type))
  }
  if (type == k7zIdUnpackInfo)
  {
    RINOK(ReadUnpackInfo(p, sd, numFoldersMax, tempBufs, numTempBufs, alloc))
    RINOK(ReadID(sd, &type))
  }
  if (type == k7zIdSubStreamsInfo)
  {
    RINOK(ReadSubStreamsInfo(p, sd, ssi))
    RINOK(ReadID(sd, &type))
  }
  else
  {
    ssi->NumTotalSubStreams = p->NumFolders;
    // ssi->NumSubDigests = 0;
  }

  return (type == k7zIdEnd ? SZ_OK : SZ_ERROR_UNSUPPORTED);
}


static SRes SzReadAndDecodePackedStreams(
    ILookInStreamPtr inStream,
    CSzData *sd,
    CBuf * const tempBufs,
    const UInt32 numFoldersMax,
    const UInt64 baseOffset,
    CSzAr *p,
    ISzAllocPtr allocTemp)
{
  UInt64 dataStartPos;
  UInt32 fo;
  CSubStreamInfo ssi;

  RINOK(SzReadStreamsInfo(p, sd, numFoldersMax, NULL, 0, &dataStartPos, &ssi, allocTemp))
  
  dataStartPos += baseOffset;
  if (p->NumFolders == 0)
    return SZ_ERROR_ARCHIVE;
 
  for (fo = 0; fo < p->NumFolders; fo++)
    Buf_Init(tempBufs + fo);
  
  for (fo = 0; fo < p->NumFolders; fo++)
  {
    CBuf *tempBuf = tempBufs + fo;
    const UInt64 unpackSize = SzAr_GetFolderUnpackSize(p, fo);
    if ((size_t)unpackSize != unpackSize)
      return SZ_ERROR_MEM;
    if (!Buf_Create(tempBuf, (size_t)unpackSize, allocTemp))
      return SZ_ERROR_MEM;
  }
  
  for (fo = 0; fo < p->NumFolders; fo++)
  {
    const CBuf *tempBuf = tempBufs + fo;
    RINOK(LookInStream_SeekTo(inStream, dataStartPos))
    RINOK(SzAr_DecodeFolder(p, fo, inStream, dataStartPos, tempBuf->data, tempBuf->size, allocTemp))
  }
  
  return SZ_OK;
}


// (size & 1) == 0
// (data) is aligned for 2-bytes
static SRes SzReadFileNames(const Byte *data, size_t size, UInt32 numFiles, size_t *offsets)
{
  const Byte *p, *lim;
  *offsets++ = 0;
  if (numFiles == 0)
    return (size == 0) ? SZ_OK : SZ_ERROR_ARCHIVE;
  if (size < 2)
    return SZ_ERROR_ARCHIVE;
  lim = data + size;
  if (*(const UInt16 *)(const void *)(lim - 2))
    return SZ_ERROR_ARCHIVE;
  p = data;
  do
  {
    if (p >= lim)
      return SZ_ERROR_ARCHIVE;
    for (; *(const UInt16 *)(const void *)p; p += 2);
    p += 2;
    *offsets++ = (size_t)(p - data) >> 1;
  }
  while (--numFiles);
  return (p == lim) ? SZ_OK : SZ_ERROR_ARCHIVE;
}


static SRes ReadTime(CSzBitUi64s *p, size_t num, CSzData *sd2,
    const CBuf *tempBufs, UInt32 numTempBufs, ISzAllocPtr alloc)
{
  const Byte *data;
  size_t size;
  size_t i;
  CNtfsFileTime *vals;
  Byte *defs;
  Byte external;
  
  if (p->Defs)
    return SZ_ERROR_ARCHIVE;
  RINOK(ReadBitVector(sd2, num, &p->Defs, alloc))
  
  SZ_READ_BYTE_SD(sd2, external)
  if (external == 0)
  {
    data = sd2->Data;
    size = sd2->Size;
  }
  else
  {
    UInt32 index;
    RINOK(SzReadNumber32(sd2, &index))
    if (index >= numTempBufs || sd2->Size)
      return SZ_ERROR_ARCHIVE;
    data = tempBufs[index].data;
    size = tempBufs[index].size;
  }
  
  MY_ALLOC_ZE(CNtfsFileTime, p->Vals, num, alloc)
  vals = p->Vals;
  defs = p->Defs;
  for (i = 0; i < num; i++)
    if (SzBitArray_Check(defs, i))
    {
      if (size < 8)
        return SZ_ERROR_ARCHIVE;
      size -= 8;
      vals[i].Low = GetUi32(data);
      vals[i].High = GetUi32(data + 4);
      data += 8;
    }
    else
      vals[i].High = vals[i].Low = 0;
  
  if (size)
    return SZ_ERROR_ARCHIVE;
  return SZ_OK;
}


#define NUM_ADDITIONAL_STREAMS_MAX 8


static SRes SzReadHeader2(CSzArEx *p, CSzData *sd, ILookInStreamPtr inStream,
    CBuf * const tempBufs, ISzAllocPtr allocMain, ISzAllocPtr allocTemp)
{
  CSubStreamInfo ssi;
  UInt32 numTempBufs = 0;

{
  UInt64 type;
  
  SzData_CLEAR(&ssi.sdSizes)
  SzData_CLEAR(&ssi.sdCRCs)
  SzData_CLEAR(&ssi.sdNumSubStreams)

  ssi.NumSubDigests = 0;
  ssi.NumTotalSubStreams = 0;

  RINOK(ReadID(sd, &type))

  if (type == k7zIdArchiveProperties)
  {
    for (;;)
    {
      UInt64 type2;
      RINOK(ReadID(sd, &type2))
      if (type2 == k7zIdEnd)
        break;
      RINOK(SkipData(sd))
    }
    RINOK(ReadID(sd, &type))
  }

  if (type == k7zIdAdditionalStreamsInfo)
  {
    CSzAr tempAr;
    SRes res;
    
    SzAr_Init(&tempAr);
    tempAr.RangeLimit = p->db.RangeLimit;

    res = SzReadAndDecodePackedStreams(inStream, sd, tempBufs, NUM_ADDITIONAL_STREAMS_MAX,
        p->startPosAfterHeader, &tempAr, allocTemp);
    numTempBufs = tempAr.NumFolders;
    SzAr_Free(&tempAr, allocTemp);
    
    if (res != SZ_OK)
      return res;
    RINOK(ReadID(sd, &type))
  }

  if (type == k7zIdMainStreamsInfo)
  {
    static const UInt32 k_numFoldersMax = (UInt32)1 << 30;
    RINOK(SzReadStreamsInfo(&p->db, sd, k_numFoldersMax, tempBufs, numTempBufs,
        &p->dataPos, &ssi, allocMain))
    p->dataPos += p->startPosAfterHeader;
    RINOK(ReadID(sd, &type))
  }

  if (type == k7zIdEnd)
    return SZ_OK;
  if (type != k7zIdFilesInfo)
    return SZ_ERROR_ARCHIVE;
}

{
  UInt32 numFiles, numEmptyStreams = 0;
  const Byte *emptyStreams = NULL;
  const Byte *emptyFiles = NULL;
  
  RINOK(SzReadNumber32(sd, &numFiles))
  p->NumFiles = numFiles;

  for (;;)
  {
    CSzData sdSwitch;
    UInt64 type;
    RINOK(ReadID(sd, &type))
    if (type == k7zIdEnd)
      break;
    {
      UInt64 size;
      RINOK(ReadNumber(sd, &size))
      if (size > sd->Size)
        return SZ_ERROR_ARCHIVE;
      sdSwitch.Data = sd->Data;
      sdSwitch.Size = (size_t)size;
      SKIP_DATA(sd, size)
    }
    if (type >= 64)
      continue;
    
    switch ((unsigned)type)
    {
      case k7zIdEmptyStream:
      {
        if (emptyStreams || emptyFiles)
          return SZ_ERROR_ARCHIVE;
        if ((numFiles + 7) >> 3 != sdSwitch.Size)
          return SZ_ERROR_ARCHIVE;
        emptyStreams = sdSwitch.Data;
        numEmptyStreams = CountDefinedBits(emptyStreams, numFiles);
        break;
      }
      
      case k7zIdEmptyFile:
      {
        if (emptyFiles)
          return SZ_ERROR_ARCHIVE;
        if ((numEmptyStreams + 7) >> 3 != sdSwitch.Size)
          return SZ_ERROR_ARCHIVE;
        emptyFiles = sdSwitch.Data;
        break;
      }
      
      case k7zIdName:
      {
        size_t namesSize;
        const Byte *namesData;
        Byte external;
        if (p->FileNameOffsets)
          return SZ_ERROR_ARCHIVE;
        SZ_READ_BYTE_SD(&sdSwitch, external)
        if (external == 0)
        {
          namesData = sdSwitch.Data;
          namesSize = sdSwitch.Size;
        }
        else
        {
          UInt32 index;
          RINOK(SzReadNumber32(&sdSwitch, &index))
          if (index >= numTempBufs || sdSwitch.Size)
            return SZ_ERROR_ARCHIVE;
          namesData = tempBufs[index].data;
          namesSize = tempBufs[index].size;
        }
        if (namesSize & 1)
          return SZ_ERROR_ARCHIVE;
        MY_ALLOC(size_t, p->FileNameOffsets, numFiles + 1, allocMain)
        MY_ALLOC_ZE_AND_CPY(p->FileNames, namesSize, namesData, allocMain)
        RINOK(SzReadFileNames(p->FileNames, namesSize, numFiles, p->FileNameOffsets))
        break;
      }
      
      case k7zIdWinAttrib:
      {
        Byte external;
        if (p->Attribs.Defs)
          return SZ_ERROR_ARCHIVE;
        RINOK(ReadBitVector(&sdSwitch, numFiles, &p->Attribs.Defs, allocMain))
        SZ_READ_BYTE_SD(&sdSwitch, external)
        if (external)
        {
          UInt32 index;
          RINOK(SzReadNumber32(&sdSwitch, &index))
          if (index >= numTempBufs || sdSwitch.Size)
            return SZ_ERROR_ARCHIVE;
          sdSwitch.Data = tempBufs[index].data;
          sdSwitch.Size = tempBufs[index].size;
        }
        RINOK(ReadUi32s(&sdSwitch, numFiles, &p->Attribs, allocMain))
        if (sdSwitch.Size)
          return SZ_ERROR_ARCHIVE;
        break;
      }
      
      case k7zIdMTime:
      case k7zIdCTime:
      {
        RINOK(ReadTime((unsigned)type == k7zIdMTime ? &p->MTime: &p->CTime,
            numFiles, &sdSwitch, tempBufs, numTempBufs, allocMain))
        break;
      }
      
      default: break;
    }
  }

  if (numFiles - numEmptyStreams != ssi.NumTotalSubStreams)
    return SZ_ERROR_ARCHIVE;

  for (;;)
  {
    UInt64 type;
    RINOK(ReadID(sd, &type))
    if (type == k7zIdEnd)
      break;
    RINOK(SkipData(sd))
  }

  {
    UInt32 i;
    UInt32 emptyFileIndex = 0;
    UInt32 folderIndex = 0;
    UInt32 remSubStreams = 0;
    UInt32 numSubStreams = 0;
    UInt64 unpackPos = 0;
    const Byte *digestsDefs = NULL;
    const Byte *digestsVals = NULL;
    UInt32 digestIndex = 0;
    Byte isDirMask = 0;
    Byte crcMask = 0;
    Byte mask = 0x80;
    
    MY_ALLOC(UInt32, p->FolderToFile, p->db.NumFolders + 1, allocMain)
    MY_ALLOC_ZE(UInt32, p->FileToFolder, p->NumFiles, allocMain)
    MY_ALLOC(UInt64, p->UnpackPositions, p->NumFiles + 1, allocMain)
    MY_ALLOC_ZE(Byte, p->IsDirs, (p->NumFiles + 7) >> 3, allocMain)

    RINOK(SzBitUi32s_Alloc(&p->CRCs, p->NumFiles, allocMain))

    if (ssi.sdCRCs.Size != 0)
    {
      Byte allDigestsDefined = 0;
      SZ_READ_BYTE_SD_NOCHECK(&ssi.sdCRCs, allDigestsDefined)
      if (allDigestsDefined)
        digestsVals = ssi.sdCRCs.Data;
      else
      {
        const size_t numBytes = (ssi.NumSubDigests + 7) >> 3;
        digestsDefs = ssi.sdCRCs.Data;
        digestsVals = digestsDefs + numBytes;
      }
    }

    for (i = 0; i < numFiles; i++, mask >>= 1)
    {
      if (mask == 0)
      {
        const UInt32 byteIndex = (i - 1) >> 3;
        p->IsDirs[byteIndex] = isDirMask;
        p->CRCs.Defs[byteIndex] = crcMask;
        isDirMask = 0;
        crcMask = 0;
        mask = 0x80;
      }

      p->UnpackPositions[i] = unpackPos;
      p->CRCs.Vals[i] = 0;
      
      if (emptyStreams && SzBitArray_Check(emptyStreams, i))
      {
        if (emptyFiles)
        {
          if (!SzBitArray_Check(emptyFiles, emptyFileIndex))
            isDirMask |= mask;
          emptyFileIndex++;
        }
        else
          isDirMask |= mask;
        if (remSubStreams == 0)
        {
          p->FileToFolder[i] = (UInt32)-1;
          continue;
        }
      }
      
      if (remSubStreams == 0)
      {
        for (;;)
        {
          if (folderIndex >= p->db.NumFolders)
            return SZ_ERROR_ARCHIVE;
          p->FolderToFile[folderIndex] = i;
          numSubStreams = 1;
          if (ssi.sdNumSubStreams.Data)
          {
            RINOK(SzReadNumber32(&ssi.sdNumSubStreams, &numSubStreams))
          }
          remSubStreams = numSubStreams;
          if (numSubStreams != 0)
            break;
          {
            const UInt64 folderUnpackSize = SzAr_GetFolderUnpackSize(&p->db, folderIndex);
            unpackPos += folderUnpackSize;
            if (unpackPos < folderUnpackSize)
              return SZ_ERROR_ARCHIVE;
          }
          folderIndex++;
        }
      }
      
      p->FileToFolder[i] = folderIndex;
      
      if (emptyStreams && SzBitArray_Check(emptyStreams, i))
        continue;
      
      if (--remSubStreams == 0)
      {
        const UInt64 folderUnpackSize = SzAr_GetFolderUnpackSize(&p->db, folderIndex);
        const UInt64 startFolderUnpackPos = p->UnpackPositions[p->FolderToFile[folderIndex]];
        if (folderUnpackSize < unpackPos - startFolderUnpackPos)
          return SZ_ERROR_ARCHIVE;
        unpackPos = startFolderUnpackPos + folderUnpackSize;
        if (unpackPos < folderUnpackSize)
          return SZ_ERROR_ARCHIVE;

        if (numSubStreams == 1 && SzBitWithVals_Check(&p->db.FolderCRCs, folderIndex))
        {
          p->CRCs.Vals[i] = p->db.FolderCRCs.Vals[folderIndex];
          crcMask |= mask;
        }
        folderIndex++;
      }
      else
      {
        UInt64 v;
        RINOK(ReadNumber(&ssi.sdSizes, &v))
        unpackPos += v;
        if (unpackPos < v)
          return SZ_ERROR_ARCHIVE;
      }
      if ((crcMask & mask) == 0 && digestsVals)
      {
        if (!digestsDefs || SzBitArray_Check(digestsDefs, digestIndex))
        {
          p->CRCs.Vals[i] = GetUi32(digestsVals);
          digestsVals += 4;
          crcMask |= mask;
        }
        digestIndex++;
      }
    }

    if (mask != 0x80)
    {
      const UInt32 byteIndex = (i - 1) >> 3;
      p->IsDirs[byteIndex] = isDirMask;
      p->CRCs.Defs[byteIndex] = crcMask;
    }
    
    p->UnpackPositions[i] = unpackPos;

    if (remSubStreams != 0)
      return SZ_ERROR_ARCHIVE;

    for (;;)
    {
      p->FolderToFile[folderIndex] = i;
      if (folderIndex >= p->db.NumFolders)
        break;
      if (!ssi.sdNumSubStreams.Data)
        return SZ_ERROR_ARCHIVE;
      RINOK(SzReadNumber32(&ssi.sdNumSubStreams, &numSubStreams))
      if (numSubStreams != 0)
        return SZ_ERROR_ARCHIVE;
      /*
      {
        UInt64 folderUnpackSize = SzAr_GetFolderUnpackSize(&p->db, folderIndex);
        unpackPos += folderUnpackSize;
        if (unpackPos < folderUnpackSize)
          return SZ_ERROR_ARCHIVE;
      }
      */
      folderIndex++;
    }

    if (ssi.sdNumSubStreams.Data && ssi.sdNumSubStreams.Size != 0)
      return SZ_ERROR_ARCHIVE;
  }
}
  return SZ_OK;
}


static SRes SzReadHeader(
    CSzArEx *p,
    CSzData *sd,
    ILookInStreamPtr inStream,
    ISzAllocPtr allocMain,
    ISzAllocPtr allocTemp)
{
  UInt32 i;
  SRes res;
  CBuf tempBufs[NUM_ADDITIONAL_STREAMS_MAX];

  for (i = 0; i < NUM_ADDITIONAL_STREAMS_MAX; i++)
    Buf_Init(tempBufs + i);
  
  res = SzReadHeader2(p, sd, inStream, tempBufs, allocMain, allocTemp);
  
  for (i = 0; i < NUM_ADDITIONAL_STREAMS_MAX; i++)
    Buf_Free(tempBufs + i, allocTemp);

  if (res == SZ_OK && sd->Size != 0)
    return SZ_ERROR_ARCHIVE;
  return res;
}

static SRes SzArEx_Open2(
    CSzArEx *p,
    ILookInStreamPtr inStream,
    ISzAllocPtr allocMain,
    ISzAllocPtr allocTemp)
{
  Byte header[k7zStartHeaderSize];
  Int64 startPosAfterHeader;
  UInt64 nextHeaderOffset, nextHeaderSize;
  size_t nextHeaderSizeT;
  UInt32 nextHeaderCRC;
  CBuf buf;
  SRes res;

  RINOK(LookInStream_Read2(inStream, header, k7zStartHeaderSize, SZ_ERROR_NO_ARCHIVE))
  startPosAfterHeader = 0; // k7zStartHeaderSize
  RINOK(ILookInStream_Seek(inStream, &startPosAfterHeader, SZ_SEEK_CUR))
  p->startPosAfterHeader = (UInt64)startPosAfterHeader;
  {
    unsigned i;
    for (i = 0; i < k7zSignatureSize; i++)
      if (header[i] != k7zSignature[i])
        return SZ_ERROR_NO_ARCHIVE;
    if (header[6] != 0) // k7zMajorVersion
      return SZ_ERROR_UNSUPPORTED;
  }
  if (CrcCalc(header + 12, 20) != GetUi32(header + 8))
    return SZ_ERROR_CRC;

  nextHeaderOffset = GetUi64(header + 12);
  nextHeaderSize = GetUi64(header + 20);
  nextHeaderCRC = GetUi32(header + 28);

  p->db.RangeLimit = nextHeaderOffset;
  if (nextHeaderOffset >= (UInt64)1 << 62 ||
      nextHeaderSize >= (UInt64)1 << 48)
    return SZ_ERROR_NO_ARCHIVE;
  nextHeaderSizeT = (size_t)nextHeaderSize;
  if (nextHeaderSizeT != nextHeaderSize)
    return SZ_ERROR_MEM;
  if (nextHeaderSizeT == 0)
  {
    if (nextHeaderOffset != 0 || nextHeaderCRC != 0)
      return SZ_ERROR_NO_ARCHIVE;
    return SZ_OK;
  }
  {
    Int64 pos = 0;
    RINOK(ILookInStream_Seek(inStream, &pos, SZ_SEEK_END))
    if ((UInt64)(pos - startPosAfterHeader) < nextHeaderOffset + nextHeaderSize)
      return SZ_ERROR_INPUT_EOF;
  }
  RINOK(LookInStream_SeekTo(inStream, (UInt64)startPosAfterHeader + nextHeaderOffset))

  if (!Buf_Create(&buf, nextHeaderSizeT, allocTemp))
    return SZ_ERROR_MEM;

  res = LookInStream_Read(inStream, buf.data, nextHeaderSizeT);
  
  if (res == SZ_OK)
  {
    res = SZ_ERROR_ARCHIVE;
    if (CrcCalc(buf.data, nextHeaderSizeT) == nextHeaderCRC)
    {
      CSzData sd;
      UInt64 type;
      sd.Data = buf.data;
      sd.Size = buf.size;
      
      res = ReadID(&sd, &type);
      
      if (res == SZ_OK && type == k7zIdEncodedHeader)
      {
        CSzAr tempAr;
        CBuf tempBuf;
        Buf_Init(&tempBuf);
        
        SzAr_Init(&tempAr);
        tempAr.RangeLimit = p->db.RangeLimit;

        res = SzReadAndDecodePackedStreams(inStream, &sd, &tempBuf, 1, p->startPosAfterHeader, &tempAr, allocTemp);
        SzAr_Free(&tempAr, allocTemp);
       
        if (res != SZ_OK)
        {
          Buf_Free(&tempBuf, allocTemp);
        }
        else
        {
          Buf_Free(&buf, allocTemp);
          buf.data = tempBuf.data;
          buf.size = tempBuf.size;
          sd.Data = buf.data;
          sd.Size = buf.size;
          res = ReadID(&sd, &type);
        }
      }
  
      if (res == SZ_OK)
      {
        if (type == k7zIdHeader)
        {
          /*
          CSzData sd2;
          unsigned ttt;
          for (ttt = 0; ttt < 40000; ttt++)
          {
            SzArEx_Free(p, allocMain);
            sd2 = sd;
            res = SzReadHeader(p, &sd2, inStream, allocMain, allocTemp);
            if (res != SZ_OK)
              break;
          }
          */
          res = SzReadHeader(p, &sd, inStream, allocMain, allocTemp);
        }
        else
          res = SZ_ERROR_UNSUPPORTED;
      }
    }
  }
 
  Buf_Free(&buf, allocTemp);
  return res;
}


SRes SzArEx_Open(CSzArEx *p, ILookInStreamPtr inStream,
    ISzAllocPtr allocMain, ISzAllocPtr allocTemp)
{
  const SRes res = SzArEx_Open2(p, inStream, allocMain, allocTemp);
  if (res != SZ_OK)
    SzArEx_Free(p, allocMain);
  return res;
}


SRes SzArEx_Extract(
    const CSzArEx *p,
    ILookInStreamPtr inStream,
    UInt32 fileIndex,
    UInt32 *blockIndex,
    Byte **tempBuf,
    size_t *outBufferSize,
    size_t *offset,
    size_t *outSizeProcessed,
    ISzAllocPtr allocMain,
    ISzAllocPtr allocTemp)
{
  const UInt32 folderIndex = p->FileToFolder[fileIndex];
  SRes res = SZ_OK;
  
  *offset = 0;
  *outSizeProcessed = 0;
  
  if (folderIndex == (UInt32)-1)
  {
    ISzAlloc_Free(allocMain, *tempBuf);
    *blockIndex = folderIndex;
    *tempBuf = NULL;
    *outBufferSize = 0;
    return SZ_OK;
  }

  if (*tempBuf == NULL || *blockIndex != folderIndex)
  {
    const UInt64 unpackSizeSpec = SzAr_GetFolderUnpackSize(&p->db, folderIndex);
    /*
    UInt64 unpackSizeSpec =
        p->UnpackPositions[p->FolderToFile[(size_t)folderIndex + 1]] -
        p->UnpackPositions[p->FolderToFile[folderIndex]];
    */
    const size_t unpackSize = (size_t)unpackSizeSpec;

    if (unpackSize != unpackSizeSpec)
      return SZ_ERROR_MEM;
    *blockIndex = folderIndex;
    ISzAlloc_Free(allocMain, *tempBuf);
    *tempBuf = NULL;
    
    if (res == SZ_OK)
    {
      *outBufferSize = unpackSize;
      if (unpackSize != 0)
      {
        *tempBuf = (Byte *)ISzAlloc_Alloc(allocMain, unpackSize);
        if (*tempBuf == NULL)
          res = SZ_ERROR_MEM;
      }
  
      if (res == SZ_OK)
      {
        res = SzAr_DecodeFolder(&p->db, folderIndex,
            inStream, p->dataPos, *tempBuf, unpackSize, allocTemp);
      }
    }
  }

  if (res == SZ_OK)
  {
    const UInt64 unpackPos = p->UnpackPositions[fileIndex];
    *offset = (size_t)(unpackPos - p->UnpackPositions[p->FolderToFile[folderIndex]]);
    *outSizeProcessed = (size_t)(p->UnpackPositions[(size_t)fileIndex + 1] - unpackPos);
    if (*offset + *outSizeProcessed > *outBufferSize)
      return SZ_ERROR_FAIL;
    if (SzBitWithVals_Check(&p->CRCs, fileIndex))
      if (CrcCalc(*tempBuf + *offset, *outSizeProcessed) != p->CRCs.Vals[fileIndex])
        res = SZ_ERROR_CRC;
  }

  return res;
}


size_t SzArEx_GetFileNameUtf16(const CSzArEx *p, size_t fileIndex, UInt16 *dest)
{
  const size_t * const offsets = p->FileNameOffsets;
  if (!offsets)
  {
    if (dest)
      *dest = 0;
    return 1;
  }
  {
    const size_t offs = offsets[fileIndex];
    const size_t len = offsets[fileIndex + 1] - offs;
    if (dest)
    {
      size_t i;
      const Byte *src = p->FileNames + offs * 2;
      for (i = 0; i < len; i++)
        dest[i] = GetUi16a(src + i * 2);
    }
    return len;
  }
}

/* ==== 7zDec.c ==== */
/* 7zDec.c -- Decoding from 7z folder
: Igor Pavlov : Public domain */

#include "Precomp.h"

#include <string.h>

/* #define Z7_PPMD_SUPPORT */

#include "7z.h"
#include "7zCrc.h"

#include "Bcj2.h"
#include "Bra.h"
#include "CpuArch.h"
#include "Delta.h"
#include "LzmaDec.h"
#include "Lzma2Dec.h"
#ifdef Z7_PPMD_SUPPORT
#include "Ppmd7.h"
#endif

#define k_Copy 0
#ifndef Z7_NO_METHOD_LZMA2
#define k_LZMA2 0x21
#endif
#define k_LZMA  0x30101
#define k_BCJ2  0x303011B

#if !defined(Z7_NO_METHODS_FILTERS)
#define Z7_USE_BRANCH_FILTER
#endif

#if !defined(Z7_NO_METHODS_FILTERS) || \
     defined(Z7_USE_NATIVE_BRANCH_FILTER) && defined(MY_CPU_ARM64)
#define Z7_USE_FILTER_ARM64
#ifndef Z7_USE_BRANCH_FILTER
#define Z7_USE_BRANCH_FILTER
#endif
#define k_ARM64 0xa
#endif

#if !defined(Z7_NO_METHODS_FILTERS) || \
     defined(Z7_USE_NATIVE_BRANCH_FILTER) && defined(MY_CPU_ARMT)
#define Z7_USE_FILTER_ARMT
#ifndef Z7_USE_BRANCH_FILTER
#define Z7_USE_BRANCH_FILTER
#endif
#define k_ARMT  0x3030701
#endif

#ifndef Z7_NO_METHODS_FILTERS
#define k_Delta 3
#define k_RISCV 0xb
#define k_BCJ   0x3030103
#define k_PPC   0x3030205
#define k_IA64  0x3030401
#define k_ARM   0x3030501
#define k_SPARC 0x3030805
#endif

#ifdef Z7_PPMD_SUPPORT

#define k_PPMD 0x30401

typedef struct
{
  IByteIn vt;
  const Byte *cur;
  const Byte *end;
  const Byte *begin;
  UInt64 processed;
  BoolInt extra;
  SRes res;
  ILookInStreamPtr inStream;
} CByteInToLook;

static Byte ReadByte(IByteInPtr pp)
{
  Z7_CONTAINER_FROM_VTBL_TO_DECL_VAR_pp_vt_p(CByteInToLook)
  if (p->cur != p->end)
    return *p->cur++;
  if (p->res == SZ_OK)
  {
    size_t size = (size_t)(p->cur - p->begin);
    p->processed += size;
    p->res = ILookInStream_Skip(p->inStream, size);
    size = (1 << 25);
    p->res = ILookInStream_Look(p->inStream, (const void **)&p->begin, &size);
    p->cur = p->begin;
    p->end = p->begin + size;
    if (size != 0)
      return *p->cur++;
  }
  p->extra = True;
  return 0;
}

static SRes SzDecodePpmd(const Byte *props, unsigned propsSize, UInt64 inSize, ILookInStreamPtr inStream,
    Byte *outBuffer, SizeT outSize, ISzAllocPtr allocMain)
{
  CPpmd7 *ppmd;
  SRes res;
  unsigned order;
  UInt32 memSize;

  if (propsSize != 5)
    return SZ_ERROR_UNSUPPORTED;
  order = props[0];
  memSize = GetUi32(props + 1);
  if (order < PPMD7_MIN_ORDER ||
      order > PPMD7_MAX_ORDER ||
      memSize < PPMD7_MIN_MEM_SIZE ||
      memSize > PPMD7_MAX_MEM_SIZE)
    return SZ_ERROR_UNSUPPORTED;
  if ((ppmd = (CPpmd7 *)ISzAlloc_Alloc(allocMain, sizeof(CPpmd7))) == NULL)
    return SZ_ERROR_MEM;
  Ppmd7_Construct(ppmd);
  res = SZ_ERROR_MEM;
  if (Ppmd7_Alloc(ppmd, memSize, allocMain))
  {
    CByteInToLook s;
    s.vt.Read = ReadByte;
    s.inStream = inStream;
    s.begin = s.end = s.cur = NULL;
    s.extra = False;
    s.res = SZ_OK;
    s.processed = 0;

    Ppmd7_Init(ppmd, order);
    ppmd->rc.dec.Stream = &s.vt;
    res = SZ_ERROR_DATA;
    if (Ppmd7z_RangeDec_Init(&ppmd->rc.dec) && !s.extra)
    {
      Byte *buf = outBuffer;
      const Byte *lim = buf + outSize;
      for (; buf != lim; buf++)
      {
        int sym = Ppmd7z_DecodeSymbol(ppmd);
        if (s.extra || sym < 0)
          break;
        *buf = (Byte)sym;
      }
      if (buf == lim)
        if (Ppmd7z_RangeDec_IsFinishedOK(&ppmd->rc.dec)
            // || (Ppmd7z_DecodeSymbol(&ppmd) == PPMD7_SYM_END && Ppmd7z_RangeDec_IsFinishedOK(&ppmd.rc.dec))
            )
          res = SZ_OK;
    }
    if (s.extra)
      res = (s.res != SZ_OK ? s.res : SZ_ERROR_DATA);
    else if (s.processed + (size_t)(s.cur - s.begin) != inSize)
      res = SZ_ERROR_DATA;
    Ppmd7_Free(ppmd, allocMain);
  }
  ISzAlloc_Free(allocMain, ppmd);
  return res;
}

#endif


static SRes SzDecodeLzma(const Byte *props, unsigned propsSize, UInt64 inSize, ILookInStreamPtr inStream,
    Byte *outBuffer, SizeT outSize, ISzAllocPtr allocMain)
{
  CLzmaDec state;
  SRes res = SZ_OK;

  LzmaDec_CONSTRUCT(&state)
  RINOK(LzmaDec_AllocateProbs(&state, props, propsSize, allocMain))
  state.dic = outBuffer;
  state.dicBufSize = outSize;
  LzmaDec_Init(&state);

  for (;;)
  {
    const void *inBuf = NULL;
    size_t lookahead = (1 << 18);
    if (lookahead > inSize)
      lookahead = (size_t)inSize;
    res = ILookInStream_Look(inStream, &inBuf, &lookahead);
    if (res != SZ_OK)
      break;

    {
      SizeT inProcessed = (SizeT)lookahead, dicPos = state.dicPos;
      ELzmaStatus status;
      res = LzmaDec_DecodeToDic(&state, outSize, (const Byte *)inBuf, &inProcessed, LZMA_FINISH_END, &status);
      lookahead -= inProcessed;
      inSize -= inProcessed;
      if (res != SZ_OK)
        break;

      if (status == LZMA_STATUS_FINISHED_WITH_MARK)
      {
        if (outSize != state.dicPos || inSize != 0)
          res = SZ_ERROR_DATA;
        break;
      }

      if (outSize == state.dicPos && inSize == 0 && status == LZMA_STATUS_MAYBE_FINISHED_WITHOUT_MARK)
        break;

      if (inProcessed == 0 && dicPos == state.dicPos)
      {
        res = SZ_ERROR_DATA;
        break;
      }

      res = ILookInStream_Skip(inStream, inProcessed);
      if (res != SZ_OK)
        break;
    }
  }

  LzmaDec_FreeProbs(&state, allocMain);
  return res;
}


#ifndef Z7_NO_METHOD_LZMA2

static SRes SzDecodeLzma2(const Byte *props, unsigned propsSize, UInt64 inSize, ILookInStreamPtr inStream,
    Byte *outBuffer, SizeT outSize, ISzAllocPtr allocMain)
{
  CLzma2Dec state;
  SRes res = SZ_OK;

  Lzma2Dec_CONSTRUCT(&state)
  if (propsSize != 1)
    return SZ_ERROR_DATA;
  RINOK(Lzma2Dec_AllocateProbs(&state, props[0], allocMain))
  state.decoder.dic = outBuffer;
  state.decoder.dicBufSize = outSize;
  Lzma2Dec_Init(&state);

  for (;;)
  {
    const void *inBuf = NULL;
    size_t lookahead = (1 << 18);
    if (lookahead > inSize)
      lookahead = (size_t)inSize;
    res = ILookInStream_Look(inStream, &inBuf, &lookahead);
    if (res != SZ_OK)
      break;

    {
      SizeT inProcessed = (SizeT)lookahead, dicPos = state.decoder.dicPos;
      ELzmaStatus status;
      res = Lzma2Dec_DecodeToDic(&state, outSize, (const Byte *)inBuf, &inProcessed, LZMA_FINISH_END, &status);
      lookahead -= inProcessed;
      inSize -= inProcessed;
      if (res != SZ_OK)
        break;

      if (status == LZMA_STATUS_FINISHED_WITH_MARK)
      {
        if (outSize != state.decoder.dicPos || inSize != 0)
          res = SZ_ERROR_DATA;
        break;
      }

      if (inProcessed == 0 && dicPos == state.decoder.dicPos)
      {
        res = SZ_ERROR_DATA;
        break;
      }

      res = ILookInStream_Skip(inStream, inProcessed);
      if (res != SZ_OK)
        break;
    }
  }

  Lzma2Dec_FreeProbs(&state, allocMain);
  return res;
}

#endif


static SRes SzDecodeCopy(UInt64 inSize, ILookInStreamPtr inStream, Byte *outBuffer)
{
  while (inSize > 0)
  {
    const void *inBuf;
    size_t curSize = (1 << 18);
    if (curSize > inSize)
      curSize = (size_t)inSize;
    RINOK(ILookInStream_Look(inStream, &inBuf, &curSize))
    if (curSize == 0)
      return SZ_ERROR_INPUT_EOF;
    memcpy(outBuffer, inBuf, curSize);
    outBuffer += curSize;
    inSize -= curSize;
    RINOK(ILookInStream_Skip(inStream, curSize))
  }
  return SZ_OK;
}

static BoolInt IS_MAIN_METHOD(UInt32 m)
{
  switch (m)
  {
    case k_Copy:
    case k_LZMA:
  #ifndef Z7_NO_METHOD_LZMA2
    case k_LZMA2:
  #endif
  #ifdef Z7_PPMD_SUPPORT
    case k_PPMD:
  #endif
      return True;
    default:
      return False;
  }
}

static BoolInt IS_SUPPORTED_CODER(const CSzCoderInfo *c)
{
  return
      c->NumStreams == 1
      /* && c->MethodID <= (UInt32)0xFFFFFFFF */
      && IS_MAIN_METHOD((UInt32)c->MethodID);
}

#define IS_BCJ2(c) ((c)->MethodID == k_BCJ2 && (c)->NumStreams == 4)

static SRes CheckSupportedFolder(const CSzFolder *f)
{
  if (f->NumCoders < 1 || f->NumCoders > 4)
    return SZ_ERROR_UNSUPPORTED;
  if (!IS_SUPPORTED_CODER(&f->Coders[0]))
    return SZ_ERROR_UNSUPPORTED;
  if (f->NumCoders == 1)
  {
    if (f->NumPackStreams != 1 || f->PackStreams[0] != 0 || f->NumBonds != 0)
      return SZ_ERROR_UNSUPPORTED;
    return SZ_OK;
  }
  
  
  #if defined(Z7_USE_BRANCH_FILTER)

  if (f->NumCoders == 2)
  {
    const CSzCoderInfo *c = &f->Coders[1];
    if (
        /* c->MethodID > (UInt32)0xFFFFFFFF || */
        c->NumStreams != 1
        || f->NumPackStreams != 1
        || f->PackStreams[0] != 0
        || f->NumBonds != 1
        || f->Bonds[0].InIndex != 1
        || f->Bonds[0].OutIndex != 0)
      return SZ_ERROR_UNSUPPORTED;
    switch ((UInt32)c->MethodID)
    {
    #if !defined(Z7_NO_METHODS_FILTERS)
      case k_Delta:
      case k_BCJ:
      case k_PPC:
      case k_IA64:
      case k_SPARC:
      case k_ARM:
      case k_RISCV:
    #endif
    #ifdef Z7_USE_FILTER_ARM64
      case k_ARM64:
    #endif
    #ifdef Z7_USE_FILTER_ARMT
      case k_ARMT:
    #endif
        break;
      default:
        return SZ_ERROR_UNSUPPORTED;
    }
    return SZ_OK;
  }

  #endif

  
  if (f->NumCoders == 4)
  {
    if (!IS_SUPPORTED_CODER(&f->Coders[1])
        || !IS_SUPPORTED_CODER(&f->Coders[2])
        || !IS_BCJ2(&f->Coders[3]))
      return SZ_ERROR_UNSUPPORTED;
    if (f->NumPackStreams != 4
        || f->PackStreams[0] != 2
        || f->PackStreams[1] != 6
        || f->PackStreams[2] != 1
        || f->PackStreams[3] != 0
        || f->NumBonds != 3
        || f->Bonds[0].InIndex != 5 || f->Bonds[0].OutIndex != 0
        || f->Bonds[1].InIndex != 4 || f->Bonds[1].OutIndex != 1
        || f->Bonds[2].InIndex != 3 || f->Bonds[2].OutIndex != 2)
      return SZ_ERROR_UNSUPPORTED;
    return SZ_OK;
  }
  
  return SZ_ERROR_UNSUPPORTED;
}






static SRes SzFolder_Decode2(const CSzFolder *folder,
    const Byte *propsData,
    const UInt64 *unpackSizes,
    const UInt64 *packPositions,
    ILookInStreamPtr inStream, UInt64 startPos,
    Byte *outBuffer, SizeT outSize, ISzAllocPtr allocMain,
    Byte *tempBuf[])
{
  UInt32 ci;
  SizeT tempSizes[3] = { 0, 0, 0};
  SizeT tempSize3 = 0;
  Byte *tempBuf3 = 0;

  RINOK(CheckSupportedFolder(folder))

  for (ci = 0; ci < folder->NumCoders; ci++)
  {
    const CSzCoderInfo *coder = &folder->Coders[ci];

    if (IS_MAIN_METHOD((UInt32)coder->MethodID))
    {
      UInt32 si = 0;
      UInt64 offset;
      UInt64 inSize;
      Byte *outBufCur = outBuffer;
      SizeT outSizeCur = outSize;
      if (folder->NumCoders == 4)
      {
        const UInt32 indices[] = { 3, 2, 0 };
        const UInt64 unpackSize = unpackSizes[ci];
        si = indices[ci];
        if (ci < 2)
        {
          Byte *temp;
          outSizeCur = (SizeT)unpackSize;
          if (outSizeCur != unpackSize)
            return SZ_ERROR_MEM;
          temp = (Byte *)ISzAlloc_Alloc(allocMain, outSizeCur);
          if (!temp && outSizeCur != 0)
            return SZ_ERROR_MEM;
          outBufCur = tempBuf[1 - ci] = temp;
          tempSizes[1 - ci] = outSizeCur;
        }
        else if (ci == 2)
        {
          if (unpackSize > outSize) /* check it */
            return SZ_ERROR_PARAM;
          tempBuf3 = outBufCur = outBuffer + (outSize - (size_t)unpackSize);
          tempSize3 = outSizeCur = (SizeT)unpackSize;
        }
        else
          return SZ_ERROR_UNSUPPORTED;
      }
      offset = packPositions[si];
      inSize = packPositions[(size_t)si + 1] - offset;
      RINOK(LookInStream_SeekTo(inStream, startPos + offset))

      if (coder->MethodID == k_Copy)
      {
        if (inSize != outSizeCur) /* check it */
          return SZ_ERROR_DATA;
        RINOK(SzDecodeCopy(inSize, inStream, outBufCur))
      }
      else if (coder->MethodID == k_LZMA)
      {
        RINOK(SzDecodeLzma(propsData + coder->PropsOffset, coder->PropsSize, inSize, inStream, outBufCur, outSizeCur, allocMain))
      }
    #ifndef Z7_NO_METHOD_LZMA2
      else if (coder->MethodID == k_LZMA2)
      {
        RINOK(SzDecodeLzma2(propsData + coder->PropsOffset, coder->PropsSize, inSize, inStream, outBufCur, outSizeCur, allocMain))
      }
    #endif
    #ifdef Z7_PPMD_SUPPORT
      else if (coder->MethodID == k_PPMD)
      {
        RINOK(SzDecodePpmd(propsData + coder->PropsOffset, coder->PropsSize, inSize, inStream, outBufCur, outSizeCur, allocMain))
      }
    #endif
      else
        return SZ_ERROR_UNSUPPORTED;
    }
    else if (coder->MethodID == k_BCJ2)
    {
      const UInt64 offset = packPositions[1];
      const UInt64 s3Size = packPositions[2] - offset;
      
      if (ci != 3)
        return SZ_ERROR_UNSUPPORTED;
      
      tempSizes[2] = (SizeT)s3Size;
      if (tempSizes[2] != s3Size)
        return SZ_ERROR_MEM;
      tempBuf[2] = (Byte *)ISzAlloc_Alloc(allocMain, tempSizes[2]);
      if (!tempBuf[2] && tempSizes[2] != 0)
        return SZ_ERROR_MEM;
      
      RINOK(LookInStream_SeekTo(inStream, startPos + offset))
      RINOK(SzDecodeCopy(s3Size, inStream, tempBuf[2]))

      if ((tempSizes[0] & 3) != 0 ||
          (tempSizes[1] & 3) != 0 ||
          tempSize3 + tempSizes[0] + tempSizes[1] != outSize)
        return SZ_ERROR_DATA;

      {
        CBcj2Dec p;
        
        p.bufs[0] = tempBuf3;   p.lims[0] = tempBuf3 + tempSize3;
        p.bufs[1] = tempBuf[0]; p.lims[1] = tempBuf[0] + tempSizes[0];
        p.bufs[2] = tempBuf[1]; p.lims[2] = tempBuf[1] + tempSizes[1];
        p.bufs[3] = tempBuf[2]; p.lims[3] = tempBuf[2] + tempSizes[2];
        
        p.dest = outBuffer;
        p.destLim = outBuffer + outSize;
        
        Bcj2Dec_Init(&p);
        RINOK(Bcj2Dec_Decode(&p))

        {
          unsigned i;
          for (i = 0; i < 4; i++)
            if (p.bufs[i] != p.lims[i])
              return SZ_ERROR_DATA;
          if (p.dest != p.destLim || !Bcj2Dec_IsMaybeFinished(&p))
            return SZ_ERROR_DATA;
        }
      }
    }
#if defined(Z7_USE_BRANCH_FILTER)
    else if (ci == 1)
    {
#if !defined(Z7_NO_METHODS_FILTERS)
      if (coder->MethodID == k_Delta)
      {
        if (coder->PropsSize != 1)
          return SZ_ERROR_UNSUPPORTED;
        {
          Byte state[DELTA_STATE_SIZE];
          Delta_Init(state);
          Delta_Decode(state, (unsigned)(propsData[coder->PropsOffset]) + 1, outBuffer, outSize);
        }
        continue;
      }
#endif
     
#ifdef Z7_USE_FILTER_ARM64
      if (coder->MethodID == k_ARM64)
      {
        UInt32 pc = 0;
        if (coder->PropsSize == 4)
        {
          pc = GetUi32(propsData + coder->PropsOffset);
          if (pc & 3)
            return SZ_ERROR_UNSUPPORTED;
        }
        else if (coder->PropsSize != 0)
          return SZ_ERROR_UNSUPPORTED;
        z7_BranchConv_ARM64_Dec(outBuffer, outSize, pc);
        continue;
      }
#endif

#if !defined(Z7_NO_METHODS_FILTERS)
      if (coder->MethodID == k_RISCV)
      {
        UInt32 pc = 0;
        if (coder->PropsSize == 4)
        {
          pc = GetUi32(propsData + coder->PropsOffset);
          if (pc & 1)
            return SZ_ERROR_UNSUPPORTED;
        }
        else if (coder->PropsSize != 0)
          return SZ_ERROR_UNSUPPORTED;
        z7_BranchConv_RISCV_Dec(outBuffer, outSize, pc);
        continue;
      }
#endif
      
#if !defined(Z7_NO_METHODS_FILTERS) || defined(Z7_USE_FILTER_ARMT)
      {
        if (coder->PropsSize != 0)
          return SZ_ERROR_UNSUPPORTED;
       #define CASE_BRA_CONV(isa) case k_ ## isa: Z7_BRANCH_CONV_DEC(isa)(outBuffer, outSize, 0); break; // pc = 0;
        switch (coder->MethodID)
        {
         #if !defined(Z7_NO_METHODS_FILTERS)
          case k_BCJ:
          {
            UInt32 state = Z7_BRANCH_CONV_ST_X86_STATE_INIT_VAL;
            z7_BranchConvSt_X86_Dec(outBuffer, outSize, 0, &state); // pc = 0
            break;
          }
          case k_PPC: Z7_BRANCH_CONV_DEC_2(BranchConv_PPC)(outBuffer, outSize, 0); break; // pc = 0;
          // CASE_BRA_CONV(PPC)
          CASE_BRA_CONV(IA64)
          CASE_BRA_CONV(SPARC)
          CASE_BRA_CONV(ARM)
         #endif
         #if !defined(Z7_NO_METHODS_FILTERS) || defined(Z7_USE_FILTER_ARMT)
          CASE_BRA_CONV(ARMT)
         #endif
          default:
            return SZ_ERROR_UNSUPPORTED;
        }
        continue;
      }
#endif
    } // (c == 1)
#endif // Z7_USE_BRANCH_FILTER
    else
      return SZ_ERROR_UNSUPPORTED;
  }

  return SZ_OK;
}


SRes SzAr_DecodeFolder(const CSzAr *p, UInt32 folderIndex,
    ILookInStreamPtr inStream, UInt64 startPos,
    Byte *outBuffer, size_t outSize,
    ISzAllocPtr allocMain)
{
  SRes res;
  CSzFolder folder;
  CSzData sd;
  
  const Byte *data = p->CodersData + p->FoCodersOffsets[folderIndex];
  sd.Data = data;
  sd.Size = p->FoCodersOffsets[(size_t)folderIndex + 1] - p->FoCodersOffsets[folderIndex];
  
  res = SzGetNextFolderItem(&folder, &sd);
  
  if (res != SZ_OK)
    return res;

  if (sd.Size != 0
      || folder.UnpackStream != p->FoToMainUnpackSizeIndex[folderIndex]
      || outSize != SzAr_GetFolderUnpackSize(p, folderIndex))
    return SZ_ERROR_FAIL;
  {
    unsigned i;
    Byte *tempBuf[3] = { 0, 0, 0};

    res = SzFolder_Decode2(&folder, data,
        &p->CoderUnpackSizes[p->FoToCoderUnpackSizes[folderIndex]],
        p->PackPositions + p->FoStartPackStreamIndex[folderIndex],
        inStream, startPos,
        outBuffer, (SizeT)outSize, allocMain, tempBuf);
    
    for (i = 0; i < 3; i++)
      ISzAlloc_Free(allocMain, tempBuf[i]);

    if (res == SZ_OK)
      if (SzBitWithVals_Check(&p->FolderCRCs, folderIndex))
        if (CrcCalc(outBuffer, outSize) != p->FolderCRCs.Vals[folderIndex])
          res = SZ_ERROR_CRC;

    return res;
  }
}

/* ===== Hyper Neiva 7z extraction wrapper (7z SDK, public domain) ===== */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct
{
  ILookInStream vt;
  const Byte *data;
  size_t size;
  size_t pos;
} HN7zStream;

static SRes HN7z_Look(ILookInStreamPtr pp, const void **buf, size_t *size)
{
  HN7zStream *p = (HN7zStream *)pp;
  size_t rem = (p->pos < p->size) ? (p->size - p->pos) : 0;
  if (*size > rem) *size = rem;
  *buf = p->data + p->pos;
  return SZ_OK;
}

static SRes HN7z_Skip(ILookInStreamPtr pp, size_t offset)
{
  HN7zStream *p = (HN7zStream *)pp;
  if (offset > p->size - p->pos) { p->pos = p->size; return SZ_ERROR_INPUT_EOF; }
  p->pos += offset;
  return SZ_OK;
}

static SRes HN7z_Read(ILookInStreamPtr pp, void *buf, size_t *size)
{
  HN7zStream *p = (HN7zStream *)pp;
  size_t rem = p->size - p->pos;
  size_t n = *size;
  if (n > rem) n = rem;
  if (n) memcpy(buf, p->data + p->pos, n);
  p->pos += n;
  *size = n;
  return SZ_OK;
}

static SRes HN7z_Seek(ILookInStreamPtr pp, Int64 *pos, ESzSeek origin)
{
  HN7zStream *p = (HN7zStream *)pp;
  Int64 base;
  Int64 np;
  switch (origin)
  {
    case SZ_SEEK_SET: base = 0; break;
    case SZ_SEEK_CUR: base = (Int64)p->pos; break;
    case SZ_SEEK_END: base = (Int64)p->size; break;
    default: return SZ_ERROR_PARAM;
  }
  np = base + *pos;
  if (np < 0) return SZ_ERROR_PARAM;
  if ((UInt64)np > p->size) { p->pos = p->size; return SZ_ERROR_INPUT_EOF; }
  p->pos = (size_t)np;
  *pos = np;
  return SZ_OK;
}

static void HN7z_StreamInit(HN7zStream *p, const Byte *data, size_t size)
{
  memset(p, 0, sizeof(*p));
  p->vt.Look = HN7z_Look;
  p->vt.Skip = HN7z_Skip;
  p->vt.Read = HN7z_Read;
  p->vt.Seek = HN7z_Seek;
  p->data = data;
  p->size = size;
  p->pos = 0;
}

static ISzAlloc hn7z_alloc = { SzAlloc, SzFree };

typedef struct
{
  Byte *fileData;
  size_t fileSize;
  HN7zStream stream;
  CSzArEx db;
  int opened;
  int lastErr;
  UInt32 blockIndex;
  Byte *outBuffer;
  size_t outBufferSize;
  size_t curOffset;
  size_t curSize;
} HN7z;

static int hn7z_utf16_to_utf8(const UInt16 *src, size_t len, char *dst, int dstSize)
{
  int o = 0;
  size_t i;
  if (!dst || dstSize <= 0) return -1;
  for (i = 0; i < len; i++)
  {
    UInt32 cp;
    UInt16 c = src[i];
    if (c == 0) break;
    if (c >= 0xD800 && c <= 0xDBFF && (i + 1) < len && src[i + 1] >= 0xDC00 && src[i + 1] <= 0xDFFF)
    {
      cp = 0x10000 + (((UInt32)(c - 0xD800)) << 10) + (UInt32)(src[i + 1] - 0xDC00);
      i++;
    }
    else if (c >= 0xD800 && c <= 0xDFFF) cp = 0xFFFD;
    else cp = c;

    if (cp < 0x80)
    {
      if (o + 1 >= dstSize) return -1;
      dst[o++] = (char)cp;
    }
    else if (cp < 0x800)
    {
      if (o + 2 >= dstSize) return -1;
      dst[o++] = (char)(0xC0 | (cp >> 6));
      dst[o++] = (char)(0x80 | (cp & 0x3F));
    }
    else if (cp < 0x10000)
    {
      if (o + 3 >= dstSize) return -1;
      dst[o++] = (char)(0xE0 | (cp >> 12));
      dst[o++] = (char)(0x80 | ((cp >> 6) & 0x3F));
      dst[o++] = (char)(0x80 | (cp & 0x3F));
    }
    else
    {
      if (o + 4 >= dstSize) return -1;
      dst[o++] = (char)(0xF0 | (cp >> 18));
      dst[o++] = (char)(0x80 | ((cp >> 12) & 0x3F));
      dst[o++] = (char)(0x80 | ((cp >> 6) & 0x3F));
      dst[o++] = (char)(0x80 | (cp & 0x3F));
    }
  }
  dst[o] = 0;
  return o;
}

int hn7z_open(const char *path, void **out)
{
  FILE *f;
  long sz;
  unsigned char *data;
  HN7z *h;
  SRes res;

  if (out) *out = 0;
  if (!path) return -1000;
  f = fopen(path, "rb");
  if (!f) return -1001;
  if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return -1002; }
  sz = ftell(f);
  if (sz <= 0) { fclose(f); return -1003; }
  if (fseek(f, 0, SEEK_SET) != 0) { fclose(f); return -1002; }
  data = (unsigned char *)malloc((size_t)sz);
  if (!data) { fclose(f); return -1004; }
  if (fread(data, 1, (size_t)sz, f) != (size_t)sz) { fclose(f); free(data); return -1005; }
  fclose(f);

  h = (HN7z *)calloc(1, sizeof(HN7z));
  if (!h) { free(data); return -1006; }
  h->fileData = data;
  h->fileSize = (size_t)sz;
  HN7z_StreamInit(&h->stream, data, (size_t)sz);
  SzArEx_Init(&h->db);
  res = SzArEx_Open(&h->db, &h->stream.vt, &hn7z_alloc, &hn7z_alloc);
  if (res != SZ_OK)
  {
    SzArEx_Free(&h->db, &hn7z_alloc);
    free(data);
    free(h);
    return (int)res;
  }
  h->opened = 1;
  if (out) *out = h;
  return 0;
}

int hn7z_num(void *hh)
{
  HN7z *h = (HN7z *)hh;
  if (!h || !h->opened) return 0;
  return (int)h->db.NumFiles;
}

int hn7z_info(void *hh, int i, char *name, int nameSize, int *isDir, unsigned long long *size)
{
  HN7z *h = (HN7z *)hh;
  size_t len;
  UInt16 *tmp;
  int r;

  if (!h || !h->opened) return -1;
  if (i < 0 || (UInt32)i >= h->db.NumFiles) return -1;
  if (isDir) *isDir = SzArEx_IsDir(&h->db, (UInt32)i) ? 1 : 0;
  if (size) *size = (unsigned long long)SzArEx_GetFileSize(&h->db, (UInt32)i);
  if (name && nameSize > 0) name[0] = 0;

  len = SzArEx_GetFileNameUtf16(&h->db, (size_t)i, 0);
  if (len == 0) return 0;
  tmp = (UInt16 *)malloc((len + 1) * sizeof(UInt16));
  if (!tmp) return -2;
  SzArEx_GetFileNameUtf16(&h->db, (size_t)i, tmp);
  tmp[len] = 0;
  r = hn7z_utf16_to_utf8(tmp, len, name, nameSize);
  free(tmp);
  return (r < 0) ? -3 : 0;
}

const unsigned char *hn7z_extract(void *hh, int i, unsigned long long *outSize)
{
  HN7z *h = (HN7z *)hh;
  SRes res;

  if (outSize) *outSize = 0;
  if (!h || !h->opened) return 0;
  if (i < 0 || (UInt32)i >= h->db.NumFiles) return 0;
  if (SzArEx_IsDir(&h->db, (UInt32)i)) return 0;
  if (SzArEx_GetFileSize(&h->db, (UInt32)i) == 0)
  {
    if (outSize) *outSize = 0;
    return (const unsigned char *)"";
  }

  res = SzArEx_Extract(&h->db, &h->stream.vt, (UInt32)i,
      &h->blockIndex, &h->outBuffer, &h->outBufferSize,
      &h->curOffset, &h->curSize, &hn7z_alloc, &hn7z_alloc);
  if (res != SZ_OK)
  {
    h->lastErr = (int)res;
    return 0;
  }
  if (h->curOffset > h->outBufferSize || h->curSize > h->outBufferSize - h->curOffset) return 0;
  if (outSize) *outSize = (unsigned long long)h->curSize;
  return h->outBuffer + h->curOffset;
}

int hn7z_last_error(void *hh)
{
  HN7z *h = (HN7z *)hh;
  return h ? h->lastErr : 0;
}

void hn7z_close(void *hh)
{
  HN7z *h = (HN7z *)hh;
  if (!h) return;
  if (h->opened) SzArEx_Free(&h->db, &hn7z_alloc);
  if (h->outBuffer) free(h->outBuffer);
  free(h->fileData);
  free(h);
}

#endif /* HN7Z_IMPL_H */

