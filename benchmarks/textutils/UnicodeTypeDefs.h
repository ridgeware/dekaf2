#ifndef __UNICODEDEFS_H_
#define __UNICODEDEFS_H_

/**
 * \var UChar
 * Define UChar to be UCHAR_TYPE, if that is #defined (for example, to char16_t),
 * or wchar_t if that is 16 bits wide; always assumed to be unsigned.
 * If neither is available, then define UChar to be uint16_t.
 *
 * This makes the definition of UChar platform-dependent
 * but allows direct string type compatibility with platforms with
 * 16-bit wchar_t types.
 *
 * @stable ICU 4.4
 */

#if defined(__MINGW32__) || defined(__CYGWIN__) || ((defined(_AIX) || defined(__TOS_AIX__)) && !defined(__64BIT__)) || (defined(__TOS_MVS__) && !defined(_LP64)) || ((defined(__OS400__) || defined(__TOS_OS400__)) && !defined(__UTF32__)) || defined(WIN32) || defined(_WIN32) || defined(WIN64) || defined(_WIN64)
	typedef wchar_t UnicodeChar; // platform-dependent, size must be 2 bytes
	#define __UC(x) L ## x
#else
#if (_MSC_VER >= 1800) || (__cplusplus >= 201103L) // C++ 11
    typedef char16_t UnicodeChar;
	#define __UC(x) u ## x
#elif defined(__CHAR16_TYPE__)
	typedef __CHAR16_TYPE__ UnicodeChar;
#else
#   include <stdint.h>
    typedef uint16_t UnicodeChar;
#endif
#endif

#ifdef __UC
#define _UC(x) __UC(x)
#endif

/**
 * Define UnicodeChar32 as a type for single Unicode code points.
 * UnicodeChar32 is a signed 32-bit integer
 */
typedef signed int UnicodeChar32;

#endif //__UNICODEDEFS_H_
