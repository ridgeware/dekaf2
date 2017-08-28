//=============================================================================
//
// DEKAF(tm): Lighter, Faster, Smarter(tm)
//
// Copyright (c) 2000-2017, Ridgeware, Inc.
//
// +-------------------------------------------------------------------------+
// | /\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\|
// |/+---------------------------------------------------------------------+/|
// |/|                                                                     |/|
// |\|  ** THIS NOTICE MUST NOT BE REMOVED FROM THE SOURCE CODE MODULE **  |\|
// |/|                                                                     |/|
// |\|   OPEN SOURCE LICENSE                                               |\|
// |/|                                                                     |/|
// |\|   Permission is hereby granted, free of charge, to any person       |\|
// |/|   obtaining a copy of this software and associated                  |/|
// |\|   documentation files (the "Software"), to deal in the              |\|
// |/|   Software without restriction, including without limitation        |/|
// |\|   the rights to use, copy, modify, merge, publish,                  |\|
// |/|   distribute, sublicense, and/or sell copies of the Software,       |/|
// |\|   and to permit persons to whom the Software is furnished to        |\|
// |/|   do so, subject to the following conditions:                       |/|
// |\|                                                                     |\|
// |/|   The above copyright notice and this permission notice shall       |/|
// |\|   be included in all copies or substantial portions of the          |\|
// |/|   Software.                                                         |/|
// |\|                                                                     |\|
// |/|   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY         |/|
// |\|   KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE        |\|
// |/|   WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR           |/|
// |\|   PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS        |\|
// |/|   OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR          |/|
// |\|   OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR        |\|
// |/|   OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE         |/|
// |\|   SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.            |\|
// |/|                                                                     |/|
// |/+---------------------------------------------------------------------+/|
// |\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/ |
// +-------------------------------------------------------------------------+
//
//=============================================================================
//     URL="https://jlettvin@github.com:8080/experiment/UTF8?page=home#title";
//
// Special rules:
//  We assume that all characters used for delimiting URL parts
//  are NOT URL encoded and can be lexed without decoding.
//=============================================================================

#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <vector>

#include "kstring.h"
#include "kstringutils.h"

#include <fmt/format.h>

#include "kurl.h"

using fmt::format;
using std::vector;
using std::to_string;

namespace dekaf2
{
namespace KURL
{

//||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||
// Protocol
//||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||

//..............................................................................
bool Protocol::parse (const KStringView& svSource, size_t iOffset)
// Protocol::parse identifies and stores accessors to protocol elements.
//..............................................................................
{
	// TODO handle "file:///{path}" ?

	clear ();

	m_iEndOffset = iOffset;  // Stored for use by next ctor (if any).

	size_t iFound = svSource.find (':', iOffset);
	if ((iFound == KString::npos) || (iFound <= (iOffset+1)))
	{
		m_bError = true;
	}
	else
	{
		m_sProto = KString(svSource).substr (iOffset, iFound - iOffset);

		if (m_bDecode)
		{
			// Pathological case if ":" is encoded as "%3A"
			kUrlDecode (m_sProto);
			m_bDecode = false;
		}

		if (m_sProto == "file")
		{
			m_bError = true;
		}
		else if (m_sProto == "mailto")
		{
			m_iEndOffset = iFound + 1;
			m_bMailto = true;
			m_eProto = MAILTO;
		}
		else if (svSource[iFound+1] == '/' && svSource[iFound+2] == '/')
		{
			m_iEndOffset = iFound + 3;
			m_bMailto = false;

			if      (m_sProto == "ftp"  ) m_eProto = FTP;
			else if (m_sProto == "http" ) m_eProto = HTTP;
			else if (m_sProto == "https") m_eProto = HTTPS;
			else                          m_eProto = UNDEFINED;
		}
		else
		{
			m_bError = true;
		}
	}
	return !m_bError;
}

/* TODO Do we need these friend operators?
//..............................................................................
inline bool operator== (const Protocol& left, const Protocol& right)
//..............................................................................
{
	return left.m_sProto == right.m_sProto;
}

//..............................................................................
inline bool operator!= (const Protocol& left, const Protocol& right)
//..............................................................................
{
	return left.m_sProto != right.m_sProto;
}
*/



//||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||
// User
//||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||

//..............................................................................
bool User::parse (const KStringView& svSource, size_t iOffset)
//..............................................................................
{
	clear ();

	m_iEndOffset = iOffset;  // Stored for use by next ctor (if any).

	size_t iFound = svSource.find_first_of ("@/?#", iOffset);
	//## it would be more correct to compare against KStringView::npos here and
	//## in the lines below
	if (iFound != KString::npos && svSource[iFound] == '@')
	{
		size_t iColon = svSource.find (':', iOffset);
		if (iColon != KString::npos && iColon < iFound)
		{
			//## did you consider for a second what you do here, performance wise?
			//## svSource may have 1 billion chars. You then construct a temporary
			//## KString, only to create a anoter one (with .substr()) that you
			//## then assign to a third string.
			//## Why not simply like this:
			//## m_sUser.assign(svSource.data() + iOffset, iColon - iOffset);
			m_sUser = KString(svSource).substr (iOffset, iColon - iOffset);
			//## same here
			m_sPass = KString(svSource).substr (iColon + 1, iFound - iColon - 1);
		}
		else
		{
			//## same here
			m_sUser = KString(svSource).substr (iOffset, iFound - iOffset);
		}

		m_bError = false;
		m_iEndOffset = iFound + 1;

		//## in what case should this string not be decoded? And why does
		//## m_bDecode need to be a member var and not a par to this function?
		if (m_bDecode)
		{
			kUrlDecode (m_sUser);
			kUrlDecode (m_sPass);
			m_bDecode = false;
		}

	}
	return !m_bError;
}


//||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||
// Domain
//||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||

//..............................................................................
bool Domain::parseHostName (const KStringView& svSource, size_t iOffset)
//..............................................................................
{
	static const KString sDotCo (".co.");

	m_iEndOffset = iOffset;  // Stored for use by next ctor (if any).

	size_t iInitial = iOffset + ((svSource[iOffset] == '/') ? 1 : 0);
	size_t iSize = svSource.size ();
	size_t iFound = svSource.find_first_of (":/?#", iInitial);

	iFound = (iFound == KString::npos) ? iSize : iFound;
	m_iEndOffset = iFound;
	//## see my comments above about performance issues here and how to fix it
	m_sHostName  = KString(svSource).substr (iOffset, iFound - iOffset);

	if (m_bDecode)
	{
		// Pathological case if ":" is encoded as "%3A"
		kUrlDecode (m_sHostName);
	}

	// m_sBaseName   is special-cased
	//
	//         google.com       1Back but no 2Back     GOOGLE
	//
	//        www.ibm.com       2Back but no ".co."    IBM
	//
	// foo.bar.baz.co.jp        3Back and is ".co."    BAZ
	//    ^   ^   ^  ^
	//    |   |   |  |
	//    4   3   2  1 Back
	// If ".co." between 2Back & 1Back : base domain is between 3Back & 2Back
	// If not, and 2Back exists: base domain is between 2Back & 1Back
	// If not even 2Back then base domain is between beginning and 1Back
	//
	// getBaseDomain () converts KStringView to KString then uses MakeUpper.
	size_t i1Back = m_sHostName.find_last_of ('.');
	if (i1Back == KString::npos)
	{
		m_bError = true;
	}
	else
	{
		size_t i2Back = m_sHostName.find_last_of ('.', i1Back - 1);
		if (i2Back != KString::npos)
		{
			bool bDotCo = true;
			for (size_t ii = 0; ii < 4 && bDotCo; ++ii)
			{
				bDotCo &= (m_sHostName[i2Back + ii] == sDotCo[ii]);
			}
			if (bDotCo)
			{
				size_t i3Back = m_sHostName.find_last_of ('.', i2Back - 1);
				i3Back = (i3Back == KString::npos) ? iOffset : i3Back;
				m_sBaseName   = m_sHostName.substr (i3Back + 1, i2Back - i3Back);
			}
			else
			{
				m_sBaseName   = m_sHostName.substr (i2Back + 1, i1Back - i2Back);
			}
		}
		else
		{
			m_sBaseName   = m_sHostName.substr (iOffset, i1Back - iOffset + 1);
		}
	}
	return !m_bError;
}

//..............................................................................
bool Domain::parse (const KStringView& svSource, size_t iOffset)
//..............................................................................
{
	clear ();

	if (!parseHostName (svSource, iOffset) || m_iEndOffset == svSource.size ())
	{
	}
	else
	{

		size_t iColon = m_iEndOffset;
		if (svSource[iColon] == ':')
		{
			++iColon;
			size_t iNext = svSource.find_first_of ("/?#", iColon);
			iNext = (iNext == KString::npos) ? svSource.size () : iNext;
			// Get port as string
			// Something is wrong with substr on KStringView.
			KString sPortName{KString(svSource).substr (iColon, iNext - iColon)};
			//KStringView svPortName = sPortName;
			//KString sPortName{svPortName};
			if (m_bDecode)
			{
				kUrlDecode (sPortName);
			}

			const char* sPort = sPortName.c_str ();
			// Parse port as number
			m_iPortNum = static_cast<uint16_t> (kToUInt (sPort));
			m_iEndOffset = iNext;
		}
	}

	m_bDecode = false;
	return !m_bError;
}



//||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||
// Path
//||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||

//..............................................................................
bool Path::parse (const KStringView& svSource, size_t iOffset)
//..............................................................................
{
	clear ();

	m_iEndOffset = iOffset;                     // Stored for use by next ctor

	size_t iIndex{iOffset};
	m_bError = false;

	size_t iStart = svSource.find_first_of ("/?#", iOffset);

	// Remaining string MUST begin with Path, Query, or Fragment prefix.
	m_bError = (svSource.size () && iStart == KString::npos);

	if (!m_bError && svSource[iStart] == '/')
	{
		++iOffset;

		size_t iSize = svSource.size ();
		size_t iFound = svSource.find_first_of ("?#", iOffset);

		iFound  = (iFound == KString::npos) ? iSize : iFound;
		m_sPath = KString(svSource).substr (iOffset-1, iFound - iOffset + 1);
		iIndex  = iOffset = m_iEndOffset = iFound;
		if (m_bDecode)
		{
			kUrlDecode (m_sPath);
			m_bDecode = false;
		}
	}

	if (!m_bError)
	{
		m_iEndOffset = iIndex;
	}
	else
	{
		clear ();
		m_bError = true;
	}
	return !m_bError;
}



//||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||
// Query
//||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||

//..............................................................................
bool Query::parse (const KStringView& svSource, size_t iOffset)
//..............................................................................
{
	clear ();

	m_iEndOffset = iOffset;  // Stored for use by next ctor (if any).

	if (iOffset < svSource.size () && svSource[iOffset] == '?')
	{
		// Enable use of either '?' prefix or not.
		++iOffset;
	}
	size_t iSize = svSource.size ();
	size_t iFound = svSource.find ('#', iOffset);
	iFound = (iFound == KString::npos) ? iSize : iFound;
	m_iEndOffset = iFound;
	//## why is there a need to keep a copy of the verbatim query string
	//## in a member variable - can't it at any time be reconstructed
	//## from the KProps struct?
	m_sQuery = KString(svSource).substr (iOffset, iFound - iOffset);

	if (m_bDecode)
	{
		kUrlDecode (m_sQuery);
		m_bDecode = false;
	}

	decode ();

	return !m_bError;
}

//..............................................................................
bool Query::decode ()
//..............................................................................
{
	size_t iAnchor = 0, iEnd, iEquals, iTerminal = m_sQuery.size ();

	if (iTerminal)
	{
		do
		{
			// Get bounds of query pair
			iEnd = m_sQuery.find ('&', iAnchor); // Find division

			iEnd = (iEnd == KString::npos) ? iTerminal : iEnd;
			KStringView svEncoded(m_sQuery.substr (iAnchor, iEnd - iAnchor));

			iEquals = svEncoded.find ('=');
			if (iEquals == KString::npos)
			{
				m_bError = true;
				return !m_bError;
			}
			KString svKey (KString(svEncoded).substr (0          , iEquals));
			KString svVal (KString(svEncoded).substr (iEquals + 1         ));

			if (svKey.size () && svVal.size ())
			{
				//## key and val are already decoded, look at parse()...
				KString sKey, sVal;
				kUrlDecode (svKey, sKey);
				kUrlDecode (svVal, sVal);
				//## and not using a std::move() here is not good.
				m_kpQuery.Add (sKey, sVal);
			}

			iAnchor = iEnd + 1;  // Move anchor forward

		} while (iEnd < iTerminal);
	}
	return true;
}


//..............................................................................
bool Query::serialize (KString& sTarget) const
//..............................................................................
{
	if (m_kpQuery.size () != 0)
	{
		typedef KProps<KString, KString, true, false> KProp_t;

		sTarget += '?';

		bool bAmpersand = false;
		//## change the below loop to
		//## for (const auto& it : m_kpQuery)
		//## {
		//## 	if (bAmpersand)
		//## 	{
		//## 		sTarget += '&';
		//## 	}
		//## 	else
		//## 	{
		//## 		bAmpersand = true;
		//## 	}
		//## 	kUrlEncode(it.first, sTarget);
		//## 	sTarget += '=';
		//## 	kUrlEncode(it.second, sTarget);
		//## }
		KProp_t::const_iterator cmIter;
		for (cmIter = m_kpQuery.begin (); cmIter != m_kpQuery.end (); ++cmIter)
		{
			if (bAmpersand)
			{
				sTarget += '&';
			}
			bAmpersand = true;

			kUrlEncode (cmIter->first , sTarget);
			sTarget += '=';
			kUrlEncode (cmIter->second, sTarget);
		}
	}
	return true;
}



//||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||
// Fragment
//||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||


//..............................................................................
bool Fragment::parse (const KStringView& svSource, size_t iOffset)
//..............................................................................
{
	clear ();

	m_iEndOffset = iOffset;  // Stored for use by next ctor (if any).

	//## see above..
	m_sFragment  = KString(svSource).substr (iOffset, svSource.size () - iOffset);
	if (m_bDecode)
	{
		kUrlDecode (m_sFragment);
		m_bDecode = false;
	}


	m_iEndOffset = svSource.size ();
	return !m_bError;
}



//||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||
// KURI
//||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||

//..............................................................................
bool URI::parse (const KStringView& svSource, size_t iOffset)
//..............................................................................
{
	clear ();

	m_iEndOffset = iOffset;                     // Stored for use by next ctor

	size_t iIndex{iOffset};
	size_t iSize{svSource.size ()};

	m_bError = !Path::parse (svSource, iIndex);
	iIndex   = Path::getEndOffset ();

	if (!m_bError)
	{
		if (iIndex < iSize && svSource[iIndex] == '?')
		{
			m_bError = !Query::parse (svSource, iIndex);  // optional
			iIndex   = Query::getEndOffset ();
		}
	}

	if (!m_bError)
	{
		if (iIndex < iSize && svSource[iIndex] == '#')
		{
			m_bError = !Fragment::parse (svSource, iIndex);
			iIndex   = Fragment::getEndOffset ();
		}
	}

	m_bError = (iIndex != svSource.size ());

	if (!m_bError)
	{
		m_iEndOffset = iIndex;
	}
	else
	{
		clear ();
		m_bError = true;
	}
	return !m_bError;
}



//||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||
// URL
//||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||

//..............................................................................
bool URL::parse (const KStringView& svSource, size_t iOffset)
//..............................................................................
{
	bool bResult = true;

	m_iEndOffset = iOffset;  // Stored for use by next ctor (if any).

	bResult &= Protocol::parse (svSource, m_iEndOffset);  // mandatory
	iOffset  = Protocol::getEndOffset ();
	if (bResult)
	{
		User::parse (svSource, iOffset);                  // optional
		iOffset  = User::getEndOffset ();

		bResult &= Domain::parse (svSource, iOffset);     // mandatory
		iOffset  = Domain::getEndOffset ();
	}
	if (bResult)
	{
		bResult &= URI::parse (svSource, iOffset);        // mandatory
		iOffset  = URI::getEndOffset ();
	}

	m_bError = !bResult;

	if (m_bError)
	{
		clear ();
		m_bError = true;
	}
	if (!m_bError)
	{
		m_iEndOffset = iOffset;
	}

	return !m_bError;
}

} // namespace KURL

} // namespace dekaf2
