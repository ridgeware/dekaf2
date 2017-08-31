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

	bool bError{false};

	clear ();

	m_iEndOffset = iOffset;  // Stored for use by next ctor (if any).

	size_t iFound = svSource.find (':', iOffset);
	//## how can iFound be < iOffset? don't you mean iFound == iOffset ?
	if ((iFound == KStringView::npos) || (iFound <= iOffset))
	{
		bError = true;
	}
	else
	{
		m_sProto.assign (svSource.data () + iOffset, iFound - iOffset);

		// Pathological case if ":" is encoded as "%3A"
		//## how can this help here? iFound IS the ':', and it is not part of the assignment to m_sProto
		kUrlDecode (m_sProto);

		if (m_sProto == "file")
		{
			//## what's wrong with file?
			bError = true;
		}
		else if (m_sProto == "mailto")
		{
			m_iEndOffset = iFound + 1;
			m_bMailto = true;
			m_eProto = MAILTO;
		}
		//## how are you protected against string underflows here?
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
			bError = true;
			m_sProto.clear ();
		}
	}
	return !bError;
}



//||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||
// User
//||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||

//..............................................................................
bool User::parse (const KStringView& svSource, size_t iOffset)
//..............................................................................
{
	bool bError{false}; // Never set true for User //## then why carrying that variable around. Delete it, it distracts.
	clear ();

	m_iEndOffset = iOffset;  // Stored for use by next ctor (if any).

	size_t iFound  = svSource.find_first_of ("@/?#", iOffset);
	//## please rewrite without the use of the variable bEnded. It only complicates readability:
	//## if (iFound != KStringView::npos)
	//## {
	//##	..
	//## }
	//## else
	//## {
	//##	m_iEndOffset = ??
	//## }
	//## return true;
	bool   bEnded  = (iFound != KStringView::npos);
	bool   bAtSign = (bEnded && svSource[iFound] == '@');
	iFound = bEnded ? iFound : svSource.size();
	if (bAtSign)
	{
		size_t iColon = svSource.find (':', iOffset);
		if (iColon != KStringView::npos && iColon < iFound)
		{
			m_sUser.assign (svSource.data () + iOffset, iColon - iOffset);
			m_sPass.assign (svSource.data () + iColon + 1, iFound - iColon - 1);
		}
		else
		{
			m_sUser.assign (svSource.data () + iOffset, iFound - iOffset);
		}

		m_iEndOffset = iFound + 1;

		kUrlDecode (m_sUser);
		kUrlDecode (m_sPass);

	}
	return !bError;
}


//||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||
// Domain
//||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||

//..............................................................................
bool Domain::parseHostName (const KStringView& svSource, size_t iOffset)
//..............................................................................
{
	static const KString sDotCo (".co.");

	bool bError{false};
	m_iEndOffset = iOffset;  // Stored for use by next ctor (if any).

	size_t iInitial = iOffset + ((svSource[iOffset] == '/') ? 1 : 0);
	size_t iSize = svSource.size ();
	size_t iFound = svSource.find_first_of (":/?#", iInitial);

	iFound = (iFound == KStringView::npos) ? iSize : iFound;
	m_iEndOffset = iFound;
	m_sHostName.assign (svSource.data () + iOffset, iFound - iOffset);

	// Pathological case if ":" is encoded as "%3A"
	//## but how would that help? the %3A would not be in m_sHostName?
	//## And you do not search for : anymore in m_sHostName?
	kUrlDecode (m_sHostName);

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
	//## use .rfind() if you only search one element
	size_t i1Back = m_sHostName.find_last_of ('.');
	if (i1Back == KString::npos)
	{
		//## actually I do not think that this is an error..
		//## In private networks it is perfectly normal
		//## to just use a name without domain part.
		bError = true;
	}
	else
	{
		//## use .rfind() if you only search one element
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
				//## use .rfind() if you only search one element
				size_t i3Back = m_sHostName.find_last_of ('.', i2Back - 1);
				i3Back = (i3Back == KString::npos) ? iOffset : i3Back;
				m_sBaseName.assign (
				            //## your python script does no use spaces for the indentation here.
				            //## Our coding standard does. Just highlight the block and press CTRL-I
						m_sHostName.data () + i3Back + 1,
						i2Back - i3Back);
			}
			else
			{
				m_sBaseName.assign (
						m_sHostName.data () + i2Back + 1,
						i1Back - i2Back);
			}
		}
		else if (iOffset < m_sHostName.size () && iOffset < i1Back)
		{
			m_sBaseName.assign (
					m_sHostName.data () + iOffset,
					i1Back - iOffset + 1);
			m_sBaseName.size ();
		}
		else if (i1Back)
		{
			m_sBaseName.assign (m_sHostName.data (), i1Back);
		}
		else
		{
			// Uncertain about this.  Hostname without a '.'?
			//## as stated above, that's perfectly fine. But you
			//## won't arrive here as you err out earlier.
			m_sBaseName.assign (m_sHostName.data ());
		}
	}
	return !bError;
}

//..............................................................................
bool Domain::parse (const KStringView& svSource, size_t iOffset)
//..............................................................................
{
	bool bError{false};
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
			iNext = (iNext == KStringView::npos) ? svSource.size () : iNext;
			// Get port as string
			KString sPortName;
			sPortName.assign (svSource.data () + iColon, iNext - iColon);
			kUrlDecode (sPortName);

			const char* sPort = sPortName.c_str ();
			// Parse port as number
			m_iPortNum = static_cast<uint16_t> (kToUInt (sPort));
			m_iEndOffset = iNext;
		}
	}

	return !bError;
}



//||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||
// Path
//||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||

//..............................................................................
bool Path::parse (const KStringView& svSource, size_t iOffset)
//..............................................................................
{
	clear ();

	bool bError{false};
	m_iEndOffset = iOffset;                     // Stored for use by next ctor

	size_t iIndex{iOffset};

	size_t iStart = svSource.find_first_of ("/?#", iOffset);

	// Remaining string MUST begin with Path, Query, or Fragment prefix.
	//## why do you only test here for svSource.size() ? Shouldn't that
	//## be the first thing you do?
	//## Also, please remove bError variable from the source, and use
	//## if/then/else and early returns. That makes the code much easier
	//## to read.
	bError = (svSource.size () > iOffset && iStart == KStringView::npos);

	if (!bError && svSource[iStart] == '/')
	{
		++iOffset;

		size_t iSize = svSource.size ();
		size_t iFound = svSource.find_first_of ("?#", iOffset);

		iFound  = (iFound == KStringView::npos) ? iSize : iFound;
		m_sPath.assign (svSource.data () + iOffset-1, iFound - iOffset + 1);
		iIndex  = iOffset = m_iEndOffset = iFound;
		kUrlDecode (m_sPath);
	}

	if (!bError)
	{
		m_iEndOffset = iIndex;
	}
	else
	{
		clear ();
		bError = true;
	}
	return !bError;
}



//||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||
// Query
//||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||

//..............................................................................
bool Query::parse (const KStringView& svSource, size_t iOffset)
//..............................................................................
{
	bool bError{false};
	clear ();

	m_iEndOffset = iOffset;  // Stored for use by next ctor (if any).

	if (iOffset < svSource.size () && svSource[iOffset] == '?')
	{
		// Enable use of either '?' prefix or not.
		++iOffset;
	}
	size_t iSize = svSource.size ();
	size_t iFound = svSource.find ('#', iOffset);
	iFound = (iFound == KStringView::npos) ? iSize : iFound;
	m_iEndOffset = iFound;
	KStringView svQuery;
	svQuery = svSource.substr (iOffset, iFound - iOffset);

	decode (svQuery);   // KurlDecode must be done on key=val separately.

	return !bError;
}

//..............................................................................
bool Query::decode (KStringView svQuery)
//..............................................................................
{
	//## remove bError
	bool bError{false};
	size_t iAnchor = 0, iEnd, iEquals, iTerminal = svQuery.size ();

	if (iTerminal)
	{
		do
		{
			// Get bounds of query pair
			iEnd = svQuery.find ('&', iAnchor); // Find division

			iEnd = (iEnd == KString::npos) ? iTerminal : iEnd;

			//## this should be a string view
			KString sEncoded;
			sEncoded.assign (svQuery.data () + iAnchor, iEnd - iAnchor);

			iEquals = sEncoded.find ('=');
			if (iEquals == KStringView::npos)
			{
				bError = (iTerminal != iEnd);
				return !bError;
			}
			//## these should be string views
			KString sKeyEncoded (sEncoded.substr (0          , iEquals));
			KString sValEncoded (sEncoded.substr (iEquals + 1         ));

			if (sKeyEncoded.size () && sValEncoded.size ())
			{
				// decoding may only happen AFTER '=' '&' detections
				KString sKey, sVal;
				//## kUrlDecode should probably be changed to take as first
				//## param a KStringView instead of a template String
				kUrlDecode (sKeyEncoded, sKey);
				kUrlDecode (sValEncoded, sVal);
				m_kpQuery.Add (std::move (sKey), std::move (sVal));
			}

			iAnchor = iEnd + 1;  // Move anchor forward

		} while (iEnd < iTerminal);
	}
	return !bError;
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
		for (const auto& it : m_kpQuery)
		{
			if (bAmpersand)
			{
				sTarget += '&';
			}
			else
			{
				bAmpersand = true;
			}
			kUrlEncode (it.first, sTarget);
			sTarget += '=';
			kUrlEncode (it.second, sTarget);
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
	//## remove bError
	bool bError{false};
	clear ();

	//## remove this assigment
	m_iEndOffset = iOffset;  // Stored for use by next ctor (if any).

	m_sFragment.assign (svSource.data () + iOffset, svSource.size () - iOffset);
	kUrlDecode (m_sFragment);

	m_iEndOffset = svSource.size ();
	return !bError;
}



//||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||
// KURI
//||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||

//..............................................................................
bool URI::parse (const KStringView& svSource, size_t iOffset)
//..............................................................................
{
	bool bError{false};
	clear ();

	m_iEndOffset = iOffset;                     // Stored for use by next ctor

	size_t iIndex{iOffset};
	size_t iSize{svSource.size ()};

	bError = !Path::parse (svSource, iIndex);
	iIndex   = Path::getEndOffset ();

	if (!bError)
	{
		if (iIndex < iSize && svSource[iIndex] == '?')
		{
			bError = !Query::parse (svSource, iIndex);  // optional
			iIndex   = Query::getEndOffset ();
		}
	}

	if (!bError)
	{
		if (iIndex < iSize && svSource[iIndex] == '#')
		{
			bError = !Fragment::parse (svSource, iIndex);
			iIndex   = Fragment::getEndOffset ();
		}
	}

	bError = (iIndex != svSource.size ());

	if (!bError)
	{
		m_iEndOffset = iIndex;
	}
	else
	{
		clear ();
		bError = true;
	}
	return !bError;
}



//||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||
// URL
//||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||

//..............................................................................
bool URL::parse (const KStringView& svSource, size_t iOffset)
//..............................................................................
{
	bool bResult{true};
	bool bError{false};

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

	bError = !bResult;

	if (bError)
	{
		clear ();
		bError = true;
	}
	if (!bError)
	{
		m_iEndOffset = iOffset;
	}

	return !bError;
}

} // namespace KURL

} // namespace dekaf2
