#ifndef __UNICODECONTAINERS_H_
#define __UNICODECONTAINERS_H_

#include "UnicodeTypeDefs.h"
#include "TextHandlingUtilsDefs.h"
#include <string>
#include <vector>
#include <utility>

#define CREATE_HEADER_FILES 0

class TEXTHANDLINGUTILS_EXPORT CUnicodeString
{
public:
	// constructors/destructor
	// pszUtf8String must not be modified during the life of CUnicodeString object
	CUnicodeString(const char* pszUtf8String);
	// pszUtf16String must not be modified during the life of CUnicodeString object
	CUnicodeString(const UnicodeChar* pszUtf16String);
	~CUnicodeString();

	UnicodeChar32 char32At(int pos) const;
	int length() const { return m_iLength; }
	int moveIndex32(int index, int delta) const;
	const UnicodeChar* GetUtf16String() const;
	const char* GetUtf8String() const;
	int GetOffsetInString(int pos) const;
	int GetOffsetInUtf8String(int pos) const;

private:
	CUnicodeString(const CUnicodeString& other);
	CUnicodeString& operator=(const CUnicodeString& other);

	enum US_ORIGIN_ENC
	{
		USOE_UTF8,
		USOE_UTF16,
	};

	US_ORIGIN_ENC m_OriginEnc;
	UnicodeChar* m_pszUtf16String;
	char* m_pszUtf8String;
	int* m_pOffsetArray;
	int m_iLength;
};

class TEXTHANDLINGUTILS_EXPORT CUtfString
{
public:
	// default constructor and destructor
	CUtfString(size_t szLength)
		: m_szLength(szLength) {}
	CUtfString()
		: m_szLength(0) {}
	virtual ~CUtfString() {}

	virtual UnicodeChar32 char32At(size_t pos) const = 0;
	virtual size_t moveIndex32Fwd(size_t index, size_t delta) const = 0;
	virtual size_t moveIndex32Back(size_t index, size_t delta) const = 0;

	size_t length() const { return m_szLength; }

protected:
	size_t m_szLength;
};

class TEXTHANDLINGUTILS_EXPORT CUtf8String : public CUtfString
{
public:
	CUtf8String(const char* pszUtf8String, size_t szLength)
		: CUtfString(szLength), m_pszUtf8String(pszUtf8String) {}
	CUtf8String(const char* pszUtf8String);
	CUtf8String(const std::string& strUtf8String);

	CUtf8String& assign(const char* pszUtf8String, size_t szLength);
	CUtf8String& assign(const char* pszUtf8String);
	CUtf8String& assign(const std::string& strUtf8String);

	// overrides
	UnicodeChar32 char32At(size_t pos) const;
	size_t moveIndex32Fwd(size_t index, size_t delta) const;
	size_t moveIndex32Back(size_t index, size_t delta) const;

private:
	const char* m_pszUtf8String;
};

class TEXTHANDLINGUTILS_EXPORT CUtf16String : public CUtfString
{
public:
	CUtf16String(const UnicodeChar* pszUtf16String, size_t szLength)
		: CUtfString(szLength), m_pszUtf16String(pszUtf16String) {}
	CUtf16String(const UnicodeChar* pszUtf16String);
	CUtf16String(const std::basic_string<UnicodeChar>& strUtf16String);

	CUtf16String& assign(const UnicodeChar* pszUtf16String, size_t szLength);
	CUtf16String& assign(const UnicodeChar* pszUtf16String);
	CUtf16String& assign(const std::basic_string<UnicodeChar>& strUtf16String);

	// overrides
	UnicodeChar32 char32At(size_t pos) const;
	size_t moveIndex32Fwd(size_t index, size_t delta) const;
	size_t moveIndex32Back(size_t index, size_t delta) const;

private:
	static size_t getstringlength(const UnicodeChar* pszUtf16String);

	const UnicodeChar* m_pszUtf16String;
};

class TEXTHANDLINGUTILS_EXPORT CUnicodeChar32Set
{
public:
	CUnicodeChar32Set(const UnicodeChar32* pList, int iLenList)
		: m_pList(pList), m_iLenList(iLenList) {}
	CUnicodeChar32Set()
		: m_pList(NULL), m_iLenList(0) {}

	void load(const UnicodeChar32* pList, int iLenList);
	bool contains(UnicodeChar32 c) const;
	void GetList(std::vector<UnicodeChar32>& vcElements) const;
	int size() const { return m_iLenList; }

#if CREATE_HEADER_FILES
	static bool ReadPropertyFile(const char* pszFileName, std::vector<std::string>& vecLinesOfFile);
	static bool CreateCodeForListDef(const std::vector<std::string>& vecPropFileLines, const char* pszTypeName, std::string& strDef, bool bTypePrecedesPoundSign = true);
#endif

private:
	int findCodePoint(UnicodeChar32 c) const;

	const UnicodeChar32* m_pList;
	int m_iLenList;
};

class TEXTHANDLINGUTILS_EXPORT CUnicodeChar32Map
{
public:
	CUnicodeChar32Map(const UnicodeChar32* pKeyList, const UnicodeChar32* pValueList, int iLenLists)
		: m_pKeyList(pKeyList), m_pValueList(pValueList), m_iLenLists(iLenLists) {}
	CUnicodeChar32Map()
		: m_pKeyList(NULL), m_pValueList(NULL), m_iLenLists(0) {}

	void load(const UnicodeChar32* pKeyList, const UnicodeChar32* pValueList, int iLenLists);
	UnicodeChar32 mapping(UnicodeChar32 c) const;
	void GetList(std::vector<std::pair<UnicodeChar32, UnicodeChar32> >& vcElements) const;

private:
	int findCodePoint(UnicodeChar32 c) const;

	const UnicodeChar32* m_pKeyList;
	const UnicodeChar32* m_pValueList;
	int m_iLenLists;
};

class TEXTHANDLINGUTILS_EXPORT CUnicodeChar32CatMap
{
public:
	CUnicodeChar32CatMap(const UnicodeChar32* pList, int iLenList, const int* pCat, int iLenCat)
		: m_pList(pList), m_pCat(pCat), m_iLenList(iLenList), m_iLenCat(iLenCat) {}
	CUnicodeChar32CatMap()
		: m_pList(NULL), m_pCat(NULL), m_iLenList(0), m_iLenCat(0) {}

	void load(const UnicodeChar32* pList, int iLenList, const int* pCat, int iLenCat);
	int category(UnicodeChar32 c) const;
	void GetList(std::vector<UnicodeChar32>& vcElements, int cat) const;

private:
	int findCodePoint(UnicodeChar32 c) const;

	const UnicodeChar32* m_pList;
	const int* m_pCat;
	int m_iLenList;
	int m_iLenCat;
};

class TEXTHANDLINGUTILS_EXPORT CUtf8StringSet
{
public:
	CUtf8StringSet(const char** pList, int iLen)
		: m_pList(pList), m_iLenList(iLen) {}
	CUtf8StringSet()
		: m_pList(NULL), m_iLenList(0) {}

	void load(const char** pList, int iLen);
	bool contains(const std::string& str) const;
	void GetList(std::vector<std::string>& vcElements) const;
	int size() const { return m_iLenList; }

private:
	int GetListInd(const char* psz) const;

	const char** m_pList;
	int m_iLenList;
};

#endif //__UNICODESTRING_H_
