#pragma once
#include <iostream>
#include <cstdio>
#include <string>
#include <cstdlib>
#include "kstring.h"

namespace dekaf2
{

#define btoa(b) ((b)?"true":"false")

//-----------------------------------------------------------------------------
class KStreamRW
//-----------------------------------------------------------------------------
{
//------
public:
//------
	KStreamRW(); // Needed for sentinel
	KStreamRW(FILE* pFile);
	virtual ~KStreamRW();
	bool initialize(FILE* pipe);

	// "Standard Interface
	bool readLine(KString& sOutputBuffer, size_t iMaxLen = 0, bool bTextOnly = false);
	bool writeLine(KString& sInputBuffer);
	virtual bool getNext() { return true; } // to be removed

	// Iterator Interface
	KStreamRW& operator++(int iIgnore);
	KString  operator*();
	KStreamRW& end();
	KStreamRW& begin();
	// TODO Iterator write method?
	void         operator=(const KString& rhs);

	bool         operator!=(const KStreamRW& rhs) const;
	bool         operator==(const KStreamRW& rhs) const;


//------
protected:
//------
	KString      m_sLine;
	size_t       m_iLen;

//------
private:
//------

	//char*      m_sLine;
	FILE*        m_pFILE; // TODO different kinds of file descriptors for file I/O?
	KStreamRW*   m_pSelf;        // becomes sentinel ptr when end is reached
	KStreamRW*   m_pSentinel;    // sentinel ptr

	size_t       m_iLine;
	std::string  m_sData;
	bool         m_bEmpty;

	void         clear();
	void         done();

	void         debug(const char* sTag, const char* sMsg = "");
}; // END KStreamRW

} // END namespace dekaf2
