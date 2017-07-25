#pragma once
#include <iostream>
#include <cstdio>
#include <string>
#include <cstdlib>
#include "kstring.h"

namespace dekaf2
{

//-----------------------------------------------------------------------------
class KStreamIter
//-----------------------------------------------------------------------------
{
//------
public:
//------
	KStreamIter();    // Needed for sentinel
	KStreamIter(FILE* pFile);
	virtual ~KStreamIter();

	bool         operator!=(const KStreamIter& rhs) const;
	bool         operator==(const KStreamIter& rhs) const;

	KStreamIter& operator++(int iIgnore);
	std::string  operator*();

	bool         initialize(FILE* pipe);
	//virtual bool getline (KString& sTheLine, size_t iMaxLen=0, bool bTextOnly = false);//{ return true; }
	virtual bool getNext() { return true; }

	KStreamIter& end();
	KStreamIter& begin();

//------
protected:
//------
	KString      m_sLine;
	size_t       m_iLen;
//------
private:
//------

	KStreamIter* m_pSelf;        // becomes sentinel ptr at end
	char*        m_ssLine;
	//KString      m_sLine;
	//size_t       m_iLen;

	FILE*        m_pFILE;
	KStreamIter* m_pSentinel;    // sentinel ptr

	size_t       m_iLine;
	std::string  m_sData;
	bool         m_bEmpty;

	void         clear();
	void         done();

	void         debug(const char* sTag, const char* sMsg = "");
}; // END KStreamIter

} // END namespace dekaf2
