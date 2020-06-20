// 定
///////////////////////////////////////////////////////////////////////////////
//
//	sa: http://www.ibm.com/developerworks/jp/linux/library/l-port64/
//	sa: http://gcc.gnu.org/onlinedocs/cpp/Common-Predefined-Macros.html
//	sa: http://lists.alioth.debian.org/pipermail/glibc-bsd-devel/2006-January/000965.html
//
//	check ur specification.
// [user@host ~]$ gcc -dM -E - < /dev/null
//
// see also
//	/usr/include/asm-i386/types.h
//	/usr/include/asm-x86_64/types.h
//	/usr/include/linux/types.h
//	/usr/include/bits/types.h
//	/usr/include/sys/types.h
//	/usr/include/asm-generic/
//	MSVC/stdint.h
//
//
///////////////////////////////////////////////////////////////////////////////


#pragma once


#if defined(__ILP32__)				// PTR 4	tgt .i386
typedef signed char			S8;		// 1
typedef unsigned char		U8;		// 1
typedef signed short		S16;	// 2
typedef unsigned short		U16;	// 2
typedef signed int			S32;	// 4
typedef unsigned int		U32;	// 4
#elif defined(__LP64__)				// PTR 8	tgt .x86_64, int = 4, I32LP64
typedef signed char			S8;		// 1
typedef unsigned char		U8;		// 1
typedef signed short		S16;	// 2
typedef unsigned short		U16;	// 2
typedef signed int			S32;	// 4
typedef unsigned int		U32;	// 4
#elif defined(__LLP64__)			// PTR 8, int = 4, IL32P64
typedef signed char			S8;		// 1
typedef unsigned char		U8;		// 1
typedef signed short		S16;	// 2
typedef unsigned short		U16;	// 2
typedef signed int			S32;	// 4
typedef unsigned int		U32;	// 4
#elif defined(__ILP64__)			// PTR 8, !!!! int = 8 !!!
typedef signed char			S8;		// 1
typedef unsigned char		U8;		// 1
typedef signed short		S16;	// 2
typedef unsigned short		U16;	// 2
typedef signed int			S32;	// 8
typedef unsigned int		U32;	// 8
#else								// PTR 4	same tgt .i386
//---#include <c++/4.9/bits/random.h>
typedef signed char			S8;		// 1
typedef unsigned char		U8;		// 1
typedef signed short		S16;	// 2
typedef unsigned short		U16;	// 2
typedef signed int			S32;	// 4
typedef unsigned int		U32;	// 4
#endif//~(__ILP32__, __LLP64__, __LP64__, __ILP64__, other system)


//!!! U64 already defined in net-snmp-devel !!!
#if defined(_MSC_VER)
	typedef __int64				S64;
	typedef unsigned __int64	U64;
#else//~defined(_MSC_VER)
#	if !defined(u_int_64_t)
#		include <sys/types.h>
#	endif//~!defined(u_int_64_t)
		typedef int64_t		S64;		// 8
#	if !defined(U64)
		typedef u_int64_t	U64;		// 8
#	endif//~!defined(U64)
		typedef u_int64_t	size64_t;	// 8
#endif//~defined(_MSC_VER)


#if defined(__linux__) && !defined(__i386__) && !defined(__ARM_32BIT_STATE)
	typedef				__int128	S128;
	typedef	unsigned	__int128	U128;
#else // defined(__linux__) && !defined(__i386__) && !defined(__ARM_32BIT_STATE)
typedef union u128_t {
	U8	u8[16];
	U16	u16[8];
	U32	u32[4];
	U64	u64[2];
} U128, OCTWORD;
#endif//~defined(__linux__) && !defined(__i386__) &&  !defined(__ARM_32BIT_STATE)


union u256_t {
	U8	u8[32];
	U16	u16[16];
	U32	u32[8];
	U64	u64[4];
} typedef U256;

union u512_t {
	U8	u8[64];
	U16	u16[32];
	U32	u32[16];
	U64	u64[8];
} typedef U512;

union u1024_t {
	U8	u8[128];
	U16	u16[64];
	U32	u32[32];
	U64	u64[16];
} typedef U1024, U1K;

union u2048_t {
	U8	u8[256];
	U16	u16[128];
	U32	u32[64];
	U64	u64[32];
} typedef U2048, U2K;




