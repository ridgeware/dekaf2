#include "kstreamrw.h"

#include "klog.h"

namespace dekaf2
{

//-----------------------------------------------------------------------------
KStreamRW::KStreamRW ()
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
KStreamRW::KStreamRW (FILE* pFile)
//-----------------------------------------------------------------------------
    : m_pSelf   (this)
    , m_sLine   (NULL)
    , m_iLen    (0)
    , m_pFILE   (pFile)
    , m_iLine   (1)
    , m_sData   ("")
    , m_bEmpty  (true)
{
	static KStreamRW sentinel = KStreamRW();

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
KStreamRW::~KStreamRW()
//-----------------------------------------------------------------------------
{
	if (m_pFILE)
	{
		//std::fclose (m_pFILE);
	}

}

//-----------------------------------------------------------------------------
bool KStreamRW::operator!=(const KStreamRW& rhs) const
//-----------------------------------------------------------------------------
{
	return (this->m_pSelf != &rhs);
}

//-----------------------------------------------------------------------------
bool KStreamRW::operator==(const KStreamRW& rhs) const
//-----------------------------------------------------------------------------
{
	return (this->m_pSelf == &rhs);
}

//-----------------------------------------------------------------------------
KStreamRW& KStreamRW::operator++(int iIgnore)
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
KString KStreamRW::operator*()
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

			//size_t ret = getline(&m_ssLine, &m_iLen, m_pFILE);
			bool bRet = readLine(m_sLine, m_iLen);
			//bool bRet = getNext();

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
bool KStreamRW::initialize(FILE* pipe)
//-----------------------------------------------------------------------------
{
	m_pFILE = pipe;
	static KStreamRW sentinel = KStreamRW();

	m_pSentinel = &sentinel;

	if (!m_pFILE)
	{
		m_pSelf = m_pSentinel;
	}
	return (m_pFILE != nullptr);
}

//-----------------------------------------------------------------------------
bool KStreamRW::readLine (KString& sOutputBuffer, size_t iMaxLen, bool bTextOnly)
//-----------------------------------------------------------------------------
{
	KLog().debug(3, "KStreamRW::readLine(KString & sOutputBuffer = %s, size_t iMaxLen = %i, bool bTextOnly = %s )", sOutputBuffer.c_str(), iMaxLen, btoa(bTextOnly));

	//sOutputBuffer.clear(); // If I don't clear, then this will also append, could be useful.
	if (!m_pFILE)
	{
		return false;
	}
	//std::cout << "iMaxLen: " << iMaxLen ;
	iMaxLen = (iMaxLen == 0) ? iMaxLen-1: iMaxLen; // size_t is unsigned, ergo 0-1=max_value
	//std::cout << "| iMaxLen: " << iMaxLen << std::endl;
	for (int i = 0; i < iMaxLen; i++)
	{
		int iCh = fgetc(m_pFILE);
		switch (iCh)
		{
			case EOF:
			{
				return !sOutputBuffer.empty();
			}
			case '\r':
				if (!bTextOnly) // don't want EOL chars in text only mode
				{
					sOutputBuffer += static_cast<KString::value_type>(iCh);
				}
				break;
			case '\n':
				if (!bTextOnly) // don't want EOL chars in text only mode
				{
					sOutputBuffer += static_cast<KString::value_type>(iCh);
				}
				return true;
			default:
				sOutputBuffer += static_cast<KString::value_type>(iCh);
				break;
		}
	}
	return (0 < sOutputBuffer.length()); // return true if chars were read
}

//-----------------------------------------------------------------------------
bool KStreamRW::writeLine (KString& sOutputBuffer)
//-----------------------------------------------------------------------------
{
	KLog().debug(3, "KStreamRW::writeLine(KString & sOutputBuffer = %s) start", sOutputBuffer.c_str());

	//sOutputBuffer.clear(); // If I don't clear, then this will also append, could be useful.
	if (!m_pFILE)
	{
		return false;
	}

	for (size_t i = 0; i < sOutputBuffer.size(); i++)
	{
		int iNewChar = static_cast<int>(sOutputBuffer[i]);
		int iRetChar = fputc(iNewChar, m_pFILE);
		if (iNewChar != iRetChar)
		{
			// EOF should have been returned to indicate error, otherwise char inserted is returned
			KLog().debug(1, "KStreamRW::writeLine(KString & sOutputBuffer = %s) FAILED to write %i char %s", sOutputBuffer.c_str(), i, iNewChar);
			return false;
		}

	}
	return true; // return true if chars were written as expected
}

//-----------------------------------------------------------------------------
KStreamRW& KStreamRW::begin()
//-----------------------------------------------------------------------------
{
	debug("begin");
	clear();
	return *m_pSelf;
}

//-----------------------------------------------------------------------------
KStreamRW& KStreamRW::end()
//-----------------------------------------------------------------------------
{
	debug("end");
	return *m_pSentinel;
}

//-----------------------------------------------------------------------------
void KStreamRW::clear()
//-----------------------------------------------------------------------------
{
	debug("clear");
	m_pSelf  = m_pFILE ? this : m_pSentinel;
	m_iLine  = 1L;
	m_sData  = "";
	m_bEmpty = true;
}

//-----------------------------------------------------------------------------
void KStreamRW::done()
//-----------------------------------------------------------------------------
{
	debug("done");
	m_pSelf = m_pSentinel;
	m_sData = "";
}

//-----------------------------------------------------------------------------
void KStreamRW::debug(const char* sTag, const char* sMsg)
//-----------------------------------------------------------------------------
{
	//std::cout << sTag << "\t" << sMsg << std::endl;
}

//-----------------------------------------------------------------------------
void asFILE(KStreamRW& instance)
//-----------------------------------------------------------------------------
{
	instance.begin();
}

//-----------------------------------------------------------------------------
void asIter(KStreamRW& instance)
//-----------------------------------------------------------------------------
{
	std::cout << "asIter enter" << std::endl;
	KStreamRW local = instance.begin();
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
