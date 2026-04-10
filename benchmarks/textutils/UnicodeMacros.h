#ifndef __UNICODEMACROS_H_
#define __UNICODEMACROS_H_

//useful macros

/**
* Is this code unit a lead surrogate (U+d800..U+dbff)?
* @param c 16-bit code unit
* @return TRUE or FALSE
*/
#define UD_IS_LEAD_SURROGATE(c) (((c)&0xfffffc00)==0xd800)

/**
* Is this code unit a trail surrogate (U+dc00..U+dfff)?
* @param c 16-bit code unit
* @return TRUE or FALSE
*/
#define UD_IS_TRAIL_SURROGATE(c) (((c)&0xfffffc00)==0xdc00)

/**
* Is this code point or unit a surrogate (U+d800..U+dfff)?
* @param c 32-bit code point or 16-bit code unit
* @return TRUE or FALSE
*/
#define UD_IS_SURROGATE(c) (((c)&0xfffff800)==0xd800)

/**
* Is this byte a trail byte in UTF-8 (80..bf)?
* @param c 8-bit
* @return TRUE or FALSE
*/
#define UD_IS_UTF8_TRAIL_BYTE(c) (((c)&0xc0)==0x80)

/**
* Is this byte a lead byte in UTF-8 (c0..bf)?
* @param c 8-bit
* @return TRUE or FALSE
*/
#define UD_IS_UTF8_LEAD_BYTE(c) (((c)&0xc0)==0xc0)

#endif //__UNICODEMACROS_H_
