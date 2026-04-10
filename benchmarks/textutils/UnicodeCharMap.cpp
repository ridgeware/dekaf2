#include "UnicodeCharMap.h"
#include "UnicodeCharMappings.h"
#include "UnicodeContainers.h"
#include "UtfConv.h"
#include <algorithm>

//static const CUnicodeChar32Map g_ToLowerMap(listToLowerKeys, listToLowerVals, lenListToLowerKeys);
//static const CUnicodeChar32Map g_ToUpperMap(listToUpperKeys, listToUpperVals, lenListToUpperKeys);

bool unicodecharmap::IsLower(UnicodeChar32 ch)
{
	return ((ch >= 0 && ch < lenArrToUpper) ? arrToUpper[ch] : false);
}

bool unicodecharmap::IsLower(UnicodeChar ch)
{
	UnicodeChar32 conv = arrToUpper[ch];
	return ((conv & 0xFFFF0000) == 0 && static_cast<UnicodeChar>(conv) != ch);
}

bool unicodecharmap::IsUpper(UnicodeChar32 ch)
{
	return ((ch >= 0 && ch < lenArrToLower) ? arrToLower[ch] : false);
}

bool unicodecharmap::IsUpper(UnicodeChar ch)
{
	UnicodeChar32 conv = arrToLower[ch];
	return ((conv & 0xFFFF0000) == 0 && static_cast<UnicodeChar>(conv) != ch);
}

UnicodeChar32 unicodecharmap::ToLower(UnicodeChar32 ch)
{
	return (ch >= 0 && ch < lenArrToLower) ? arrToLower[ch] : ch;
}

UnicodeChar unicodecharmap::ToLower(UnicodeChar ch)
{
	UnicodeChar32 conv = arrToLower[ch];
	return ((conv & 0xFFFF0000) == 0) ? static_cast<UnicodeChar>(conv) : ch;
}

UnicodeChar32 unicodecharmap::ToUpper(UnicodeChar32 ch)
{
	return (ch >= 0 && ch < lenArrToUpper) ? arrToUpper[ch] : ch;
}

UnicodeChar unicodecharmap::ToUpper(UnicodeChar ch)
{
	UnicodeChar32 conv = arrToUpper[ch];
	return ((conv & 0xFFFF0000) == 0) ? static_cast<UnicodeChar>(conv) : ch;
}

void unicodecharmap::tolower(UString& ustr)
{
	UString32 ustr32 = utf_conv::UTF16ToUTF32(ustr);
	unicodecharmap::tolower(ustr32);
	ustr = utf_conv::UTF32ToUTF16(ustr32);
}

void unicodecharmap::tolower(std::string& str)
{
	UString32 ustr32 = utf_conv::UTF8ToUTF32(str);
	unicodecharmap::tolower(ustr32);
	str = utf_conv::UTF32ToUTF8(ustr32);
}

void unicodecharmap::tolower(UString32& ustr32)
{
	if (ustr32.empty())
		return;

	std::transform(ustr32.begin(), ustr32.end(), ustr32.begin(), (UnicodeChar32(*)(UnicodeChar32))unicodecharmap::ToLower);
}

void unicodecharmap::toupper(UString& ustr)
{
	UString32 ustr32 = utf_conv::UTF16ToUTF32(ustr);
	unicodecharmap::toupper(ustr32);
	ustr = utf_conv::UTF32ToUTF16(ustr32);
}

void unicodecharmap::toupper(std::string& str)
{
	UString32 ustr32 = utf_conv::UTF8ToUTF32(str);
	unicodecharmap::toupper(ustr32);
	str = utf_conv::UTF32ToUTF8(ustr32);
}

void unicodecharmap::toupper(UString32& ustr32)
{
	if (ustr32.empty())
		return;

	std::transform(ustr32.begin(), ustr32.end(), ustr32.begin(), (UnicodeChar32(*)(UnicodeChar32))unicodecharmap::ToUpper);
}
