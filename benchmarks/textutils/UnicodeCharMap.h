#ifndef __UNICODECHARMAP_H_
#define __UNICODECHARMAP_H_

#include "UnicodeTypeDefs.h"
#include "UnicodeStringTypeDefs.h"
#include "TextHandlingUtilsDefs.h"

namespace unicodecharmap
{
	TEXTHANDLINGUTILS_EXPORT bool IsLower(UnicodeChar32 ch);
	TEXTHANDLINGUTILS_EXPORT bool IsLower(UnicodeChar ch);
	TEXTHANDLINGUTILS_EXPORT bool IsUpper(UnicodeChar32 ch);
	TEXTHANDLINGUTILS_EXPORT bool IsUpper(UnicodeChar ch);
	TEXTHANDLINGUTILS_EXPORT UnicodeChar32 ToLower(UnicodeChar32 ch);
	TEXTHANDLINGUTILS_EXPORT UnicodeChar ToLower(UnicodeChar ch);
	TEXTHANDLINGUTILS_EXPORT UnicodeChar32 ToUpper(UnicodeChar32 ch);
	TEXTHANDLINGUTILS_EXPORT UnicodeChar ToUpper(UnicodeChar ch);
	TEXTHANDLINGUTILS_EXPORT void tolower(std::string& str); // lowercase a UTF-8 string
	TEXTHANDLINGUTILS_EXPORT void tolower(UString& ustr); // lowercase a UTF-16 string
	TEXTHANDLINGUTILS_EXPORT void tolower(UString32& ustr32); // lowercase a UTF-32 string
	TEXTHANDLINGUTILS_EXPORT void toupper(std::string& str); // uppercase a UTF-8 string
	TEXTHANDLINGUTILS_EXPORT void toupper(UString& ustr); // uppercase a UTF-16 string
	TEXTHANDLINGUTILS_EXPORT void toupper(UString32& ustr32); // uppercase a UTF-32 string
};

#endif //__UNICODECHARMAP_H_
