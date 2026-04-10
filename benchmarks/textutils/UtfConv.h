#ifndef __UTFCONV_H_
#define __UTFCONV_H_

#include <string>
#include <vector>
#include "UnicodeTypeDefs.h"
#include "TextHandlingUtilsDefs.h"

namespace utf_conv
{
	int TEXTHANDLINGUTILS_EXPORT UTF32ToUTF8(UnicodeChar32 c, char* buffer, int buff_size);
	void TEXTHANDLINGUTILS_EXPORT UTF32ToUTF8(UnicodeChar32 c, std::string& strRet); // deprecated
	std::string TEXTHANDLINGUTILS_EXPORT UTF32ToUTF8(UnicodeChar32 c);
	void TEXTHANDLINGUTILS_EXPORT UTF32ToUTF8(const std::basic_string<UnicodeChar32>& ustrSrc, std::string& strRet, std::vector<int>* pvcOffsets = NULL); // deprecated
	std::string TEXTHANDLINGUTILS_EXPORT UTF32ToUTF8(const std::basic_string<UnicodeChar32>& ustrSrc, std::vector<int>* pvcOffsets = NULL);

	std::basic_string<UnicodeChar> TEXTHANDLINGUTILS_EXPORT UTF32ToUTF16(UnicodeChar32 c);
	void TEXTHANDLINGUTILS_EXPORT UTF32ToUTF16(const std::basic_string<UnicodeChar32>& ustrSrc, std::basic_string<UnicodeChar>& ustrRet, std::vector<int>* pvcOffsets = NULL); // deprecated
	std::basic_string<UnicodeChar> TEXTHANDLINGUTILS_EXPORT UTF32ToUTF16(const std::basic_string<UnicodeChar32>& ustrSrc, std::vector<int>* pvcOffsets = NULL);

	int TEXTHANDLINGUTILS_EXPORT UTF16ToUTF8(UnicodeChar c, char* buffer, int buff_size);
	void TEXTHANDLINGUTILS_EXPORT UTF16ToUTF8(UnicodeChar c, std::string& strRet); // deprecated
	std::string TEXTHANDLINGUTILS_EXPORT UTF16ToUTF8(UnicodeChar c);
	void TEXTHANDLINGUTILS_EXPORT UTF16ToUTF8(const std::basic_string<UnicodeChar>& ustrSrc, std::string& strRet); // deprecated
	std::string TEXTHANDLINGUTILS_EXPORT UTF16ToUTF8(const std::basic_string<UnicodeChar>& ustrSrc);

	void TEXTHANDLINGUTILS_EXPORT UTF8ToUTF16(const std::string& strSrc, std::basic_string<UnicodeChar>& ustrRet); // deprecated
	std::basic_string<UnicodeChar> TEXTHANDLINGUTILS_EXPORT UTF8ToUTF16(const std::string& strSrc);
	void TEXTHANDLINGUTILS_EXPORT UTF8ToUTF16(const char* pszSrc, std::basic_string<UnicodeChar>& ustrRet); // deprecated
	std::basic_string<UnicodeChar> TEXTHANDLINGUTILS_EXPORT UTF8ToUTF16(const char* pszSrc);

	void TEXTHANDLINGUTILS_EXPORT UTF8ToUTF32(const std::string& strSrc, std::basic_string<UnicodeChar32>& ustrRet); // deprecated
	std::basic_string<UnicodeChar32> TEXTHANDLINGUTILS_EXPORT UTF8ToUTF32(const std::string& strSrc);
	void TEXTHANDLINGUTILS_EXPORT UTF8ToUTF32(const char* pszSrc, std::basic_string<UnicodeChar32>& ustrRet); // deprecated
	std::basic_string<UnicodeChar32> TEXTHANDLINGUTILS_EXPORT UTF8ToUTF32(const char* pszSrc);

	int TEXTHANDLINGUTILS_EXPORT UTF16ToUTF32(const std::basic_string<UnicodeChar>& ustrSrc, UnicodeChar32* pszRet, int buff_size);
	void TEXTHANDLINGUTILS_EXPORT UTF16ToUTF32(const std::basic_string<UnicodeChar>& ustrSrc, std::basic_string<UnicodeChar32>& ustrRet); // deprecated
	std::basic_string<UnicodeChar32> TEXTHANDLINGUTILS_EXPORT UTF16ToUTF32(const std::basic_string<UnicodeChar>& ustrSrc);

	int TEXTHANDLINGUTILS_EXPORT UTF32ToUTF8(const UnicodeChar32* const srcData
									,  const unsigned int       srcCount
									, unsigned int* const       charsEaten = NULL
									,         char* const       toFill = NULL
									,  const unsigned int       maxBytes = 0
									,          int* const       offsetArray = NULL
									, const unsigned int        maxOffsets = 0);

	int TEXTHANDLINGUTILS_EXPORT UTF16ToUTF8(const UnicodeChar* const srcData
									,  const unsigned int       srcCount
									, unsigned int* const       charsEaten = NULL
									,         char* const       toFill = NULL
									,  const unsigned int       maxBytes = 0
									,          int* const       offsetArray = NULL
									,  const unsigned int       maxOffsets = 0
									,          const bool       bDataPending = false);

	int TEXTHANDLINGUTILS_EXPORT UTF8ToUTF16(const  char* const srcData
									,  const unsigned int       srcCount
									, unsigned int* const       bytesEaten = NULL
									,  UnicodeChar* const       toFill = NULL
									,  const unsigned int       maxChars = 0
									,          int* const       offsetArray = NULL
									,  const unsigned int       maxOffsets = 0);

	int TEXTHANDLINGUTILS_EXPORT UTF8ToUTF32(const  char* const srcData
									,   const unsigned int      srcCount
									,  unsigned int* const      bytesEaten = NULL
									, UnicodeChar32* const      toFill = NULL
									,   const unsigned int      maxChars = 0
									,           int* const      offsetArray = NULL
									,   const unsigned int      maxOffsets = 0);
}

#endif //__UTFCONV_H_
