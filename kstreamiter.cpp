#include "kstreamiter.h"

namespace dekaf2
{

//-----------------------------------------------------------------------------
KStreamIter::KStreamIter ()
//-----------------------------------------------------------------------------
    : m_pSelf   (this)
    , m_sLine   (NULL)
    , m_iLen    (0)
    , m_pFILE   (nullptr)
    , m_iLine   (1)
    , m_sData   ("")
    , m_bEmpty  (true)
{
}

//-----------------------------------------------------------------------------
KStreamIter::KStreamIter (FILE* pFile)
//-----------------------------------------------------------------------------
    : m_pSelf   (this)
    , m_sLine   (NULL)
    , m_iLen    (0)
    , m_pFILE   (pFile)
    , m_iLine   (1)
    , m_sData   ("")
    , m_bEmpty  (true)
{
	static KStreamIter sentinel = KStreamIter();

	debug ("ctor", "enter");
	m_pSentinel = &sentinel;

	if (!m_pFILE)
	{
		debug ("ctor", "no FILE");
		m_pSelf = m_pSentinel;
	}
	debug ("ctor", "leave");
}

//-----------------------------------------------------------------------------
KStreamIter::~KStreamIter()
//-----------------------------------------------------------------------------
{
	if (m_pFILE)
	{
		//std::fclose (m_pFILE);
	}
//	if (m_sLine)
//	{
//		std::free (m_sLine);
//	}
}

//-----------------------------------------------------------------------------
bool KStreamIter::operator!=(const KStreamIter& rhs) const
//-----------------------------------------------------------------------------
{
	return (this->m_pSelf != &rhs);
}

//-----------------------------------------------------------------------------
bool KStreamIter::operator==(const KStreamIter& rhs) const
//-----------------------------------------------------------------------------
{
	return (this->m_pSelf == &rhs);
}

//-----------------------------------------------------------------------------
KStreamIter& KStreamIter::operator++(int iIgnore)
//-----------------------------------------------------------------------------
{
	debug("++", "enter");
	if (m_pSelf == m_pSentinel)  // EOF or no file was opened
	{
		debug("++", "1");
		return *m_pSentinel;
	}
	else if (feof (m_pFILE))     // Convert to sentinel at EOF
	{
		debug("++", "2");
		done();
	}
	else if (m_bEmpty)           // request line to ignore
	{
		debug("++", "3");
		**this;
		m_bEmpty = true;
	}
	else                         // dismiss current buffer
	{
		debug("++", "4");
		m_bEmpty = true;
	}
	debug("++", "leave");
	return *m_pSelf;             // either this or &sentinel
}

//-----------------------------------------------------------------------------
std::string KStreamIter::operator*()
//-----------------------------------------------------------------------------
{
	debug("*", "enter");
	if (m_pSelf == m_pSentinel)  // No data returned after end()
	{
		debug("*", "no data");
	}
	else                         // Not sentinel yet
	{
		debug("*", "data");
		if (!m_pFILE)            // convert to sentinel if bad FILE
		{
			debug("*", "file");
			done();
		}
		else if (feof (m_pFILE)) // convert to sentinel at EOF
		{
			debug("*", "EOF");
			done();
		}
		else if (m_bEmpty)       // Line not yet read by operator*
		{
			debug("*", "empty");

			debug("*", "getline");

			size_t ret = getline(&m_ssLine, &m_iLen, m_pFILE);
			//bool bRet = getline(m_sLine, m_iLen);
			bool bRet = getNext();

			//if (ret == -1)
			if (!bRet)
			{
				debug("*", "-1");
				done();
			}
			else
			{
				debug("*", "data");
				m_sData = m_sLine;
				m_iLine++;
				m_bEmpty = false;
			}
		}
		else                     // Data persists if no ++;
		{
			debug("*", "ready");
		}
	}
	debug("*", "leave");
	return m_sData;
}

//-----------------------------------------------------------------------------
bool KStreamIter::initialize(FILE* pipe)
//-----------------------------------------------------------------------------
{
	m_pFILE = pipe;
	static KStreamIter sentinel = KStreamIter();

	m_pSentinel = &sentinel;

	if (!m_pFILE)
	{
		m_pSelf = m_pSentinel;
	}
	return (m_pFILE != nullptr);
}

////-----------------------------------------------------------------------------
//bool KStreamIter::getline (KString& sTheLine, size_t iMaxLen, bool bTextOnly)//{ return true; }
////-----------------------------------------------------------------------------
//{
//	return true;
//}

//-----------------------------------------------------------------------------
KStreamIter& KStreamIter::begin()
//-----------------------------------------------------------------------------
{
	debug("begin");
	clear();
	return *m_pSelf;
}

//-----------------------------------------------------------------------------
KStreamIter& KStreamIter::end()
//-----------------------------------------------------------------------------
{
	debug("end");
	return *m_pSentinel;
}

//-----------------------------------------------------------------------------
void KStreamIter::clear()
//-----------------------------------------------------------------------------
{
	debug("clear");
	m_pSelf  = m_pFILE ? this : m_pSentinel;
	m_iLine  = 1L;
	m_sData  = "";
	m_bEmpty = true;
}

//-----------------------------------------------------------------------------
void KStreamIter::done()
//-----------------------------------------------------------------------------
{
	debug("done");
	//std::cout << "done" << std::endl;
	m_pSelf = m_pSentinel;
	m_sData = "";
}

//-----------------------------------------------------------------------------
void KStreamIter::debug(const char* sTag, const char* sMsg)
//-----------------------------------------------------------------------------
{
	//std::cout << sTag << "\t" << sMsg << std::endl;
}

//-----------------------------------------------------------------------------
void asFILE(KStreamIter& instance)
//-----------------------------------------------------------------------------
{
	instance.begin();
}

//-----------------------------------------------------------------------------
void asIter(KStreamIter& instance)
//-----------------------------------------------------------------------------
{
	std::cout << "asIter enter" << std::endl;
	KStreamIter local = instance.begin();
	std::cout << "asIter begin" << std::endl;
	while (local != local.end())
	{
		std::cout << "asIter loop" << std::endl;
		std::cout << *local << std::endl;
		local++;
	}
	std::cout << "asIter leave" << std::endl;
}

} // END NAMESPACE DEKAF2
