#include <stddef.h>

#include <cassert>

#if defined(unix) || \
	defined(__unix) || \
	defined(__unix__) || \
	defined(__APPLE__) || \
	defined(__MACH__) || \
	defined(__linux__) || \
	defined(__ANDROID__) || \
	defined(__FreeBSD__)

	#define Sys_Unix_Like_ 1
#elif defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
	#define Sys_Win32_ 1
#endif

#if defined(__aarch64__) || \
	defined(__amd64__) || \
	defined (__amd64) || \
	defined (__x86_64__) || \
	defined (__x86_64)

	#define Sys_Arch_64_ 1
#else
	#define Sys_Arch_32_ 1// Huge assumption here
#endif

#ifdef Sys_Arch_32_
	#define Data_Model_ILP32_ 1
#endif

#ifdef Sys_Arch_64_
	#ifdef Sys_Unix_Like_
		#define Data_Model_LP64_ 1
	#endif

	#ifdef Sys_Win32_
		#define DataModel_LLP64_ 1
	#endif
#endif	

#define DEFAULT_ALIGNMENT sizeof(void*)

typedef unsigned char			u8;
typedef unsigned short int 		u16;
typedef unsigned int 			u32;
typedef unsigned long long int 	u64;
typedef size_t					usize;

typedef signed char 			i8;
typedef signed short int 		i16;
typedef signed int 				i32;
typedef signed long long int 	i64;
#ifdef Sys_Arch_64_
	typedef i64 isize;
#else
	typedef i32 isize;
#endif

typedef float 		f32;
typedef double 		f64;
typedef long double f128;

#define U32IndexNone_ ~0U

#define cstring char*
#define cstring_const char const*

#define NoOp_ ((void)0)
#ifdef Debug_
	#define Assert_(x) if (!(x)) __debugbreak(); assert((x))
#else
	#define Assert_(x) x
#endif

// This one's at the bottom just to avoid syntax highlighting weirdness
#define Recast_ reinterpret_cast