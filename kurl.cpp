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

//## format this appropriately so it will be detailed descriptions in doxygen

// https://en.wikipedia.org/wiki/URL
// scheme:[//[user[:password]@]host[:port]][/path][?query][#fragment]

// Suppose you want to parse fields from:
//     URL="https://jlettvin@github.com:8080/experiment/UTF8?page=home#title";
//     Protocol::Protocol kproto1(URL, 0);
// The 0 is the offset at which to start parsing.
// All these classes store the initial offset internally.
// On successful parse, the offset of the next character past what was parsed
// is stored instead.  This offset is to be retrieved from the class
// in order to provide the next class constructor/parse with a valid offset.
// This design allows independent parsing for an autonomous field or
// sequenced dependent parsing for multi-field strings.
// To see an example, review the parse method for KURL (last in file kurl.cpp).

// For Protocol:
// When Protocol is done with a successful parse, it stores a new offset
// internally at m_iEndOffset (8 for "https://" starting from iOffset==0).
// This is the offset immediately following the protocol or scheme.
// We can now parse UserInfo starting at the stored offset.
// Using the offset from UserInfo, we can now parse the domain.
//     Domain::Domain kdomain1(URL, offset);
// Other ctors and parse functions work similarly.
// Suppose we have a scrap of URL from which we wish to parse a domain.
//     scrap="jlettvin@github.com"; // strlen(scrap) == 19
//     here = 0;
//     Domain::Domain kdomain2(scrap, here);
// here will have the value 19 after this.
// A full URL is divided and parsed by running a sequence:
//     offset = 0;
//     URL="https://jlettvin@github.com:8080/experiment/UTF8?page=home#title";
//     Protocol::Protocol(URL,off);  // update offset to  8 using getEndOffset()
//     Domain::Domain(URL, off);     // update offset to 32 using getEndOffset()
//     URI::URI(URL, offset);        // update offset to 64 using getEndOffset()
// URL[offset] == '\0';  // End of string

// I think I am turning Hungarian.
// Members m_aHungarian, Statics s_bHungarian, all else is unprefixed Hungarian.

// TODO there is much common code between classes.  virtual might work well.
// Specifically, getEndOffset is always the same.
// All classes have members m_iOffset
// All classes have a parse, serialize, and clear.
// Only these last methods require specialization.
//-----------------------------------------------------------------------------
// iOffset is the starting offset for parsing.
// Class::parse stores iOffset and updates it on successful parse.
// This parse length is stored in m_iEndOffset which value is
// returned by the method size_t iOffset = getEndOffset ();
// If successful bError is set false and iOffset will have a larger value.
// Otherwise bError is set true and this value is zero.
// For typical calls, scheme is at the beginning, so iOffset should begin 0.
// parse methods shall return false on error, true on success.

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
KStringView Protocol::parse (KStringView svSource)
// Protocol::parse identifies and stores accessors to protocol elements.
//..............................................................................
{
	clear ();
	if (nullptr == svSource)
	{
		return svSource;
	}

	size_t iFound = svSource.find_first_of (':');

	bool bError{iFound == KStringView::npos};

	if (!bError)
	{
		size_t iSlash = 0;
		m_sProto.assign (svSource.data (), iFound);
		svSource.remove_prefix (iFound);
		iFound = 0;
		size_t iSize  = svSource.size();
		while (iFound < iSize && svSource[++iFound] == '/')
		{
			++iSlash;
		}

		// Pathological case if ":" is encoded as "%3A"
		//## how can this help here? iFound IS the ':', and it is not part of the assignment to m_sProto
		//?? I expect ':' to be explicit.  The remainder may be encoded.
		kUrlDecode (m_sProto);

		if (m_sProto == "file" && iSlash == 3)
		{
			m_eProto = eFILE;
			iFound--;           // Leave trailing slash of file:///
			m_sPost.assign (svSource.data (), iFound);
		}
		else if (m_sProto == "mailto" && iSlash == 0)
		{
			m_bMailto = true;
			m_eProto = eMAILTO;
			m_sPost = ":";
		}
		else if (iSlash == 2)
		//## how are you protected against string underflows here?
		{
			if      (m_sProto == "ftp"  ) m_eProto = eFTP;
			else if (m_sProto == "http" ) m_eProto = eHTTP;
			else if (m_sProto == "https") m_eProto = eHTTPS;
			else                          m_eProto = eUNDEFINED;
			m_sPost.assign (svSource.data (), iFound);
		}
		else
		{
			bError = true;
		}
		if (bError)
		{
			clear();
		}
		else
		{
			svSource.remove_prefix (iFound);
		}
	}
	return svSource;
}



//||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||
// User
//||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||

//..............................................................................
KStringView User::parse (KStringView svSource)
//..............................................................................
{
	clear ();
	if (nullptr == svSource)
	{
		return nullptr;
	}

	bool bError{false}; // Never set true for User //## then why carrying that variable around. Delete it, it distracts.

	size_t iSize   = svSource.size ();
	size_t iFound  = svSource.find_first_of ("@/?#");
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

	if (bAtSign)
	{
		m_sPost = "@";
	}

	iFound = bEnded ? iFound : iSize;

	if (!bAtSign)
	{
		bError = true;
	}
	else
	{
		size_t iColon = svSource.find (':');
		if (iColon != KStringView::npos && iColon < iFound)
		{
			m_sUser.assign (svSource.data (), iColon);
			m_sPass.assign (svSource.data () + iColon + 1, iFound - iColon - 1);
		}
		else
		{
			m_sUser.assign (svSource.data (), iFound);
		}
		//iFound += (iFound == iSize);
		iFound += bAtSign;

		kUrlDecode (m_sUser);
		kUrlDecode (m_sPass);



	}
	if (!bError)
	{
		svSource.remove_prefix (iFound);
	}
	return svSource;
}


//||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||
// Domain
//||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||

//..............................................................................
KStringView Domain::parseHostName (KStringView svSource)
//..............................................................................
{
	static const KString sDotCo (".co.");

	if (!svSource.size()) return svSource;
	bool bError{false};

	size_t iInitial = ((svSource[0] == '/') ? 1 : 0);
	size_t iSize = svSource.size ();
	size_t iFound = svSource.find_first_of (":/?#", iInitial);

	iFound = (iFound == KStringView::npos) ? iSize : iFound;
	m_sHostName.assign (svSource.data (), iFound);

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
	size_t i1Back = m_sHostName.rfind ('.');
	if (i1Back == KString::npos)
	{
		// Ignore simple non-dot hostname (localhost).
	}
	else
	{
		// When there is at least 1 dot, look for domain name features
		size_t i2Back = m_sHostName.rfind ('.', i1Back - 1);
		if (i2Back != KString::npos)
		{
			// When there are at least 2 dots, look for ".co.".
			size_t iCompare = static_cast<size_t> (
				sDotCo.compare (0, sDotCo.size (), m_sHostName.data () + i2Back));
			bool bDotCo = (iCompare == sDotCo.size () );
			if (bDotCo)
			{
				size_t i3Back = m_sHostName.rfind ('.', i2Back - 1);
				i3Back = (i3Back == KString::npos) ? 0 : i3Back;
				m_sBaseName.assign (
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
		else
		{
			m_sBaseName.assign (m_sHostName.data (), i1Back);
		}
	}
	if (!bError)
	{
		svSource.remove_prefix(iFound);
	}
	return svSource;
}

//..............................................................................
KStringView Domain::parse (KStringView svSource)
//..............................................................................
{
	clear ();
	if (nullptr == svSource)
	{
		return nullptr;
	}

	size_t iBefore = svSource.size ();

	svSource = parseHostName (svSource);
	size_t iAfter = svSource.size ();

	if (iBefore && iAfter == iBefore)
	{
		// No host name
	}
	else
	{

		size_t iColon = 0;
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
			svSource.remove_prefix (iNext);
		}
	}

	return svSource;
}



//||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||
// Path
//||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||

//..............................................................................
KStringView Path::parse (KStringView svSource)
//..............................................................................
{
	clear ();
	if (nullptr == svSource)
	{
		//return nullptr;
		return svSource;
	}

	bool bError{false};

	size_t iStart = svSource.find_first_of ("/?#");

	// Remaining string MUST begin with Path, Query, or Fragment prefix.
	//## why do you only test here for svSource.size() ? Shouldn't that
	//## be the first thing you do?
	//## Also, please remove bError variable from the source, and use
	//## if/then/else and early returns. That makes the code much easier
	//## to read.

	if (iStart != KStringView::npos && svSource[iStart] == '/')
	{
		size_t iSize = svSource.size ();
		size_t iFound = svSource.find_first_of ("?#", 1);

		iFound  = (iFound == KStringView::npos) ? iSize : iFound;
		m_sPath.assign (svSource.data (), iFound);
		svSource.remove_prefix (iFound);
		kUrlDecode (m_sPath);
	}
	return svSource;
}



//||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||
// Query
//||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||

//..............................................................................
KStringView Query::parse (KStringView svSource)
//..............................................................................
{
	clear ();
	if (nullptr == svSource)
	{
		return svSource;
	}

	size_t iOffset{0};

	if (0 < svSource.size () && svSource[iOffset] == '?')
	{
		// Enable use of either '?' prefix or not.
		++iOffset;
	}
	size_t iSize = svSource.size ();
	size_t iFound = svSource.find ('#', iOffset);
	iFound = (iFound == KStringView::npos) ? iSize : iFound;
	KStringView svQuery;
	svQuery = svSource.substr (iOffset, iFound - iOffset);

	if (decode (svQuery))   // KurlDecode must be done on key=val separately.
	{
		svSource.remove_prefix (iFound);
	}
	else
	{
		clear ();
	}

	return svSource;
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
				if (sKeyEncoded.size () && !sKey.size ())
				{
					sKey = sKeyEncoded; // painful pass-thru of invalid key
				}
				if (sValEncoded.size () && !sVal.size ())
				{
					sVal = sValEncoded; // painful pass-thru of invalid value
				}
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
KStringView Fragment::parse (KStringView svSource)
//..............................................................................
{
	clear ();
	if (nullptr == svSource)
	{
		return svSource;
	}

	m_sFragment.assign (svSource.data (), svSource.size ());
	kUrlDecode (m_sFragment);
	svSource.remove_prefix (svSource.size ());

	return svSource;
}



//||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||
// KURI
//||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||

//..............................................................................
KStringView URI::parse (KStringView svSource)
//..............................................................................
{
	clear ();
	if (nullptr == svSource)
	{
		return svSource;    // Empty svSource compares to nullptr.  SURPRISE!
	}

	size_t iSize{svSource.size ()};

	bool bError{false};

	svSource = Path::parse (svSource);

	if (!bError)
	{
		if (iSize > 0 && svSource[0] == '?')
		{
			svSource = Query::parse (svSource);  // optional
			iSize = svSource.size ();
		}
	}

	if (!bError)
	{
		if (iSize > 0 && svSource[0] == '#')
		{
			svSource = Fragment::parse (svSource);
		}
	}

	bError = (0 != svSource.size ());

	if (!bError)
	{
		svSource.remove_prefix (svSource.size ());
	}
	else
	{
		clear ();
	}
	return svSource;
}



//||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||
// URL
//||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||

//..............................................................................
KStringView URL::parse (KStringView svSource)
//..............................................................................
{
	clear ();
	if (nullptr == svSource)
	{
		return svSource;
	}

	bool bResult{true};
	bool bError{false};
	size_t iBefore, iAfter;

	iBefore = svSource.size ();
	svSource = Protocol::parse (svSource);                  // mandatory
	iAfter  = svSource.size ();
	bResult = (iBefore != iAfter);
	if (bResult)
	{
		svSource = User::parse (svSource);                  // optional

		if (getProtocolEnum () != eFILE)
		{
			iBefore = svSource.size ();
			svSource = Domain::parse (svSource);            // mandatory
			iAfter  = svSource.size ();
			bResult = (iBefore != iAfter);
		}
	}
	if (bResult)
	{
		svSource = URI::parse (svSource);                   // mandatory
	}

	bError = !bResult;

	if (bError)
	{
		clear ();
		bError = true;
	}
	if (!bError)
	{
		svSource.remove_prefix (svSource.size ());
	}

	return svSource;
}

} // namespace KURL

} // namespace dekaf2