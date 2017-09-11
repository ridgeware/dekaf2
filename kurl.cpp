/*
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
*/

/** @brief KURL class Architecture
*=============================================================================
* https://en.wikipedia.org/wiki/URL
* All URL parsing is related to RFC3986 version 3.1 for this regex:
*      scheme:[//[user[:password]@]host[:port]][/path][?query][#fragment]
*=============================================================================
* Example minimal abstract URL:
*       a://b:c@d.e:f/g/h?i=j#k
*        ^^^ ^ ^   ^ ^   ^ ^ ^
* Characters identified by a ^ above are expected to NOT be URL encoded.
*=============================================================================
* Relationship between RFC3986 and class structure:
*  a://b:c@d.e:f/g/h?i=j#k
*  ----------------------------------------------------------------------------
*  RFC3986            |class   |identifies             |parse/serialize
*  ----------------------------------------------------------------------------
*  scheme             |Protocol|a                      |a://
*  user:password      |User    |b.c                    |b.c@
*  host:port          |Domain  |d.e:f                  |d.e:f
*  path?query#fragment|URI     |/g/h, i=j, k           |/g/h?i=j#k
*  all the above      |URL     |a://b:c@d.e:f/g/h?i=j#k|a://b:c@d.e:f/g/h?i=j#k
*  ----------------------------------------------------------------------------
*=============================================================================
* Class fine structure:
*  Protocol stores enum for known schemes and KString for unknown schemes.
*  User     stores KStrings for user and password.
*  Domain   stores host and port as KStrings, and DOMAIN as uppercase KString.
*  URI      is multiple-inherited from classes Path, Query, and Fragment.
*  URL      is multiple-inherited from classes Protocol, User, Domain, and URI.
*=============================================================================
* Hidden classes/fields:
*  Path     stores the like-named RFC field.
*  Query    stores the like-named RFC field.
*  Fragment stores the like-named RFC field.
*  Port     part of Domain is not a class but stores the like-named RFC field.
*=============================================================================
* Use of KStringView for parsing decreases string copying.
* Parse methods shall return false on error, true on success.
* The main pattern of use during parsing is as follows.
* Start with a KStringView svURL: "a://b:c@d.e:f/g/h?i=j#k".
* Within KURL::URL constructor:
*  KStringView svA = Protocol::parse (svURL);  // svA is "b:c@d.e:f/g/h?i=j#k"
*  KStringView svB = User::parse (svA);        // svB is "d.e:f/g/h?i=j#k"
*  KStringView svC = Domain::parse (svB);      // svC is "/g/h?i=j#k"
*  KStringView svD = URI::parse (svC);         // svD is ""
* So that:
*  KString sTarget;
*  Protocol::serialize (sTarget);      // sTarget is "a://"
*  User::serialize (sTarget);          // sTarget is "a://b:c@"
*  Domain::serialize (sTarget);        // sTarget is "a://b:c@d.e:f"
*  URI::serialize (sTarget);           // sTarget is "a://b:c@d.e:f/g/h?i=j#k"
*-----------------------------------------------------------------------------
* Other methods:
*  T ();                               // default ctor
*  T (KStringView);                    // normal ctor
*  T (const T&);                       // copy ctor
*  operator=(const T& rhs);            // copy
*  operator=(T&& rhs);                 // move
*  operator>>(KString& sTarget);       // same as serialize
*  operator<<(KString& sSource);       // same as parse (but returns instance)
*  operator==(const T& rhs);           // compare lhs with rhs
*  operator!=(const T& rhs);           // compare lhs with rhs
*  Clear();                            // Restore to original empty state
*  KString();                          // same as serialize
*/

#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <vector>

#include "kstring.h"
#include "kstringutils.h"
#include "kurl.h"

#include <fmt/format.h>

using fmt::format;
using std::vector;
using std::to_string;

namespace dekaf2
{

/// @defgroup KURL
/// KURL implements all classes for parsing, maintaining, and serializing URLs.
/// Example URL:
///     "https://jlettvin@github.com:8080/experiment/UTF8?page=home#title";
///           ^^^        ^          ^    ^               ^         ^
/// Characters identified by ^ above are expected to NOT be URL encoded.
/// @{
namespace KURL
{

//-----------------------------------------------------------------------------
/// @brief class Protocol in group KURL.  Parse into members.
/// Protocol parses and maintains "scheme" portion of w3 URL.
/// RFC3986 3.1: scheme = ALPHA *( ALPHA / DIGIT / "+" / "-" / "." )
/// scheme:[//[user[:password]@]host[:port]][/path][?query][#fragment]
/// ------
/// Protocol extracts, stores, and reproduces URL "scheme".
/// Implementation: identify all characters until ':'.
/// For [file, ftp, http, https, mailto] store only the eProto.
/// For others, store the characters.
/// getters/setters and reserialization are available.
KStringView Protocol::Parse (KStringView svSource)
//-----------------------------------------------------------------------------
{
	Clear ();
	if (nullptr == svSource)
	{
		return svSource;
	}

	size_t iFound = svSource.find_first_of (':');

	if (iFound != KStringView::npos)
	{
		size_t iSlash = 0;
		m_sProto.assign (svSource.data (), iFound);
		svSource.remove_prefix (iFound);
		iFound = 0;
		size_t iSize = svSource.size();
		while (iFound < iSize && svSource[++iFound] == '/')
		{
			++iSlash;
		}

		kUrlDecode (m_sProto);

		// TODO should we be generous, parsing past wrong count slashes?
		// We always serialize it back to correct form.
		// Being strict violates our "accept everything we can" rule.
		// Example strict alternatives are shown below in comments.

		if (m_sProto == "file" && iSlash > 0)
		{
			m_eProto = FILE;
			iFound--;           // Leave trailing slash of file:/// for Path
		}
		else if (m_sProto == "mailto")
		{
			m_eProto = MAILTO;
		}
		else if (iSlash > 0)
		{
			if      (m_sProto == "ftp"  ) m_eProto = FTP;
			else if (m_sProto == "http" ) m_eProto = HTTP;
			else if (m_sProto == "https") m_eProto = HTTPS;
			else                          m_eProto = UNKNOWN;
		}
		else
		{
			Clear ();
			return svSource;
		}
		svSource.remove_prefix (iFound);
	}
	return svSource;
}

//-----------------------------------------------------------------------------
/// @brief class Protocol in group KURL.  Generate Protocol portion of URL.
/// Class Protocol parses and maintains "scheme" portion of w3 URL.
bool Protocol::Serialize (KString& sTarget) const
//-----------------------------------------------------------------------------
{
	// m_eProto is NEVER UNDEFINED except on clear or default ctor.
	// m_eProto is UNKNOWN for protocols like "opaquelocktoken://"
	// TODO should anything else happen for UNDEFINED or UNKNOWN?
	bool bProduce = (m_eProto != UNDEFINED);;
	if (bProduce)
	{
		sTarget += (m_eProto < UNKNOWN)
			? m_sKnown[m_eProto]
			: m_sProto + "://";
	}
	return bProduce;
}

//-----------------------------------------------------------------------------
/// @brief class Protocol in group KURL.  Restores instance to empty state
void Protocol::Clear()
//-----------------------------------------------------------------------------
{
	m_sProto.clear ();
	m_eProto = UNDEFINED;
}

KString Protocol::m_sKnown [UNKNOWN+1]
{
	"UNDEFINED",    // Parse has not been run yet.
	"file://",
	"ftp://",
	"http://",
	"https://",
	"mailto:",
	"UNKNOWN"       // Use m_sProto.
};

//-----------------------------------------------------------------------------
/// @brief class User in group KURL.  Parse into members.
/// Class User parses and maintains "user" and "password" portion of w3 URL.
/// RFC3986 3.2: authority   = [ userinfo "@" ] host [ ":" port ]
/// Implementation: We take all characters until '@'.
/// scheme:[//[user[:password]@]host[:port]][/path][?query][#fragment]
///            ----  --------
/// User extracts and stores a KStringView of URL "user" and "password".
//
// TODO Parse cannot do both partial parsing of URL and
// complete parsing on user[:password] without a trailing '@'.
// Chose to require '@'.
KStringView User::Parse (KStringView svSource)
//-----------------------------------------------------------------------------
{
	Clear ();
	if (nullptr != svSource)
	{
		size_t iFound = svSource.find ("@"); //## make this a '@' (believe me, it is a huge difference)
		if (iFound == KStringView::npos)
		{
			return svSource;
		}

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

		kUrlDecode (m_sUser);
		kUrlDecode (m_sPass);
		svSource.remove_prefix (iFound + 1);
	}
	return svSource;
}

//-----------------------------------------------------------------------------
/// @brief class User in group KURL.  Generate User portion of URL.
bool User::Serialize (KString& sTarget) const
//-----------------------------------------------------------------------------
{
	// TODO Should username/password be url encoded?
	if (m_sUser.size ())
	{
		// TODO These exclusions are speculative.  Should they change?
		//## aren't there fixed rules for what to encode and what not in the
		//## W3C standards?
		KString sExclude{".-+_"};
		KString sTemp;
		kUrlEncode (m_sUser, sTemp, sExclude);
		sTarget += sTemp;

		if (m_sPass.size ())
		{
			sTemp.clear ();
			sTarget += ':';
			// TODO These exclusions are speculative.  Should they change?
			//## aren't there fixed rules for what to encode and what not in the
			//## W3C standards?
			KString sExclude{".-+_!@#$%^&*()={}[]|;:<>,/?"};
			kUrlEncode (m_sPass, sTemp, sExclude);
			sTarget += sTemp;
		}
		sTarget += '@';
	}
	return true;
}


//-----------------------------------------------------------------------------
/// @brief class Domain in group KURL.  Special detailed parse of hostname.
/// Class Domain parses and maintains "host" and "port" portion of w3 URL.
/// RFC3986 3.2: authority = [ userinfo "@" ] host [ ":" port ]
/// RFC3986 3.2.2: host = IP-literal / IPv4address / reg-name
/// RFC3986 3.2.3: port = *DIGIT
/// Implementation: We take domain from '@'+1 to first of "/:?#\0".
/// Implementation: We take port from ':'+1 to first of "/?#\0".
/// scheme:[//[user[:password]@]host[:port]][/path][?query][#fragment]
///                             ----  ----
/// Domain extracts and stores a KStringView of URL "host" and "port"
KStringView Domain::ParseHostName (KStringView svSource)
//-----------------------------------------------------------------------------
{
	static const KString sDotCo (".co.");

	if (!svSource.size()) return svSource;
	bool bError{false};

	size_t iInitial = ((svSource[0] == '/') ? 1 : 0);
	size_t iSize = svSource.size ();
	size_t iFound = svSource.find_first_of (":/?#", iInitial);

	iFound = (iFound == KStringView::npos) ? iSize : iFound;
	m_sHostName.assign (svSource.data (), iFound);

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

			// This commented code fails, so it is implemented in a short loop.
			//size_t iCompare = static_cast<size_t> (
				//sDotCo.compare (0, sDotCo.size (), m_sHostName.data () + i2Back));
			size_t iCompare{0};
			for (size_t ii=0; ii < sDotCo.size (); ++ii)
			{
				if (sDotCo[ii] != m_sHostName[i2Back + ii])
				{
					break;
				}
				++iCompare;
			}

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

//-----------------------------------------------------------------------------
/// @brief class Domain in group KURL.  parses "host:port" portion of w3 URL.
KStringView Domain::Parse (KStringView svSource)
//-----------------------------------------------------------------------------
{
	Clear ();
	if (nullptr == svSource)
	{
		return nullptr;
	}

	size_t iBefore = svSource.size ();

	svSource = ParseHostName (svSource);
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
			//## why don't you use the KStringView -> KString version of kUrlDecode here?
			//## It would save one copy
			kUrlDecode (sPortName);

			const char* sPort = sPortName.c_str ();
			// Parse port as number
			m_iPortNum = static_cast<uint16_t> (kToUInt (sPort));
			svSource.remove_prefix (iNext);
		}
	}

	return svSource;
}

//-----------------------------------------------------------------------------
/// @brief class Domain in group KURL.  Generate Domain portion of URL.
bool Domain::Serialize (KString& sTarget) const
//-----------------------------------------------------------------------------
{
	bool bSome = true;
	if (m_sHostName.size ())
	{
		// TODO These exclusions are speculative.  Should they change?
		//## there must be standards. why does this have to be speculative?
		KString sExclude{"-_."};
		KString sTemp;
		kUrlEncode (m_sHostName, sTemp, sExclude);
		sTarget += sTemp;
		if (m_iPortNum)
		{
			sTarget += ':';
			sTarget += std::to_string (m_iPortNum);
		}
	}
	return bSome;
}



//-----------------------------------------------------------------------------
/// @brief class Path in group KURL.  Parses "path" portion of w3 URL.
/// RFC3986 3.3: (See RFC)
/// Implementation: All characters after domain from '/' to 1st of "?#\0".
/// scheme:[//[user[:password]@]host[:port]][/path][?query][#fragment]
///                                          -----   -----   --------
/// Path extracts and stores a KStringView of URL "path"
KStringView Path::Parse (KStringView svSource)
//-----------------------------------------------------------------------------
{
	Clear ();
	if (nullptr == svSource)
	{
		return svSource;
	}

	if (svSource[0] == '/')
	{
		// Remainder after path may be query or fragment.
		size_t iFound = svSource.find_first_of ("?#", 1);
		iFound = (iFound == KStringView::npos) ? svSource.size () : iFound;

		m_sPath.assign (svSource.data (), iFound);
		svSource.remove_prefix (iFound);
		kUrlDecode (m_sPath);
	}
	return svSource;
}

//-----------------------------------------------------------------------------
/// @brief class Path in group KURL.  Generate Path portion of URL.
bool Path::Serialize (KString& sTarget) const
//-----------------------------------------------------------------------------
{
	if (m_sPath.size ())
	{
		KString sPath;
		KString sExclude{"_-./"};
		kUrlEncode (m_sPath, sPath, sExclude);
		sTarget += sPath;
	}
	return true;
}



//-----------------------------------------------------------------------------
/// @brief class Query in group KURL.  Parses "query" portion of w3 URL.
/// It is also responsible for parsing the query into a private property map.
/// RFC3986 3.3: (See RFC)
/// Implementation: All characters after domain from '?' to "#" or EOL.
/// scheme:[//[user[:password]@]host[:port]][/path][?query][#fragment]
///                                                  -----
/// Query extracts and stores a KStringView of URL "query"
KStringView Query::Parse (KStringView svSource)
//-----------------------------------------------------------------------------
{
	Clear ();
	if (nullptr == svSource)
	{
		return svSource;
	}

	if (svSource[0] == '?')
	{
		svSource.remove_prefix (1);
	}

	size_t iSize = svSource.size ();
	size_t iFound = svSource.find ('#');
	iFound = (iFound == KStringView::npos) ? iSize : iFound;
	KStringView svQuery{svSource.substr (0, iFound)};

	if (decode (svQuery))   // KurlDecode must be done on key=val separately.
	{
		svSource.remove_prefix (iFound);
	}
	else
	{
		Clear ();
	}

	return svSource;
}

//-----------------------------------------------------------------------------
/// @brief class Query in group KURL.  Split/decode.
/// Split query string into key:value pairs, then decode keys and values.
bool Query::decode (KStringView svQuery)
//-----------------------------------------------------------------------------
{
	size_t iAnchor = 0, iEnd, iEquals, iTerminal = svQuery.size ();

	if (iTerminal)
	{
		do
		{
			// Get bounds of query pair
			iEnd = svQuery.find ('&', iAnchor); // Find separator
			iEnd = (iEnd == KString::npos) ? iTerminal : iEnd;  // handle none

			KStringView svEncoded{svQuery.substr (iAnchor, iEnd - iAnchor)};

			iEquals = svEncoded.find ('=');
			if (iEquals > iEnd)
			{
				return false;
			}
			KStringView svKeyEncoded (svEncoded.substr (0          , iEquals));
			KStringView svValEncoded (svEncoded.substr (iEquals + 1         ));

			if (svKeyEncoded.size () && svValEncoded.size ())
			{
				// decoding may only happen AFTER '=' '&' detections
				KString sKey, sVal;
				kUrlDecode (svKeyEncoded, sKey);
				kUrlDecode (svValEncoded, sVal);
				if (svKeyEncoded.size () && !sKey.size ())
				{
					sKey = svKeyEncoded; // painful pass-thru of invalid key
				}
				if (svValEncoded.size () && !sVal.size ())
				{
					sVal = svValEncoded; // painful pass-thru of invalid value
				}
				m_kpQuery.Add (std::move (sKey), std::move (sVal));
			}

			iAnchor = iEnd + 1;  // Move anchor forward

		} while (iEnd < iTerminal);
	}
	return true;
}


//-----------------------------------------------------------------------------
/// @brief class Query in group KURL.  Generate Query portion of URL.
bool Query::Serialize (KString& sTarget) const
//-----------------------------------------------------------------------------
{
	if (m_kpQuery.size () != 0)
	{
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



//-----------------------------------------------------------------------------
/// @brief class Fragment in group KURL.  Parses "fragment" portion of w3 URL.
/// RFC3986 3.3: (See RFC)
/// Implementation: All characters after domain from '/' to 1st of "?#\0".
/// scheme:[//[user[:password]@]host[:port]][/path][?query][#fragment]
///                                                          --------
/// Fragment extracts and stores a KStringView of URL "fragment"
KStringView Fragment::Parse (KStringView svSource)
//-----------------------------------------------------------------------------
{
	Clear ();
	//## please check for !empty(), not a nullptr
	if (nullptr == svSource)
	{
		return svSource;
	}

	if (svSource[0] == '#')
	{
		svSource.remove_prefix (1);
	}
	m_sFragment.assign (svSource.data (), svSource.size ());
	kUrlDecode (m_sFragment);
	svSource.remove_prefix (svSource.size ());

	return svSource;
}

//-----------------------------------------------------------------------------
/// @brief class Fragment in group KURL.  Generate fragment portion of URL.
bool Fragment::Serialize (KString& sTarget) const
//-----------------------------------------------------------------------------
{
	//## this actually does not reconstruct a simple "#" at the end of the URL
	if (m_sFragment.size ())
	{
		sTarget += "#" + m_sFragment;
	}
	return true;
}



//-----------------------------------------------------------------------------
/// @brief class URI in group KURL.  Parses TransPerfect URI portion of w3 URL.
/// It includes the "path", "query", and "fragment" portions.
/// The aggregation of /path?query#fragment without individual path
/// is a TransPerfect design decision; not arbitrary.
/// RFC3986 3.3: (See RFC)
/// Implementation: All characters after domain from '/' to 1st of "?#\0".
/// scheme:[//[user[:password]@]host[:port]][/path][?query][#fragment]
///                                          -----   -----   --------
/// URI extracts and stores a KStringView of URL "path"
/// URI also encapsulates Query and Fragment
KStringView URI::Parse (KStringView svSource)
//-----------------------------------------------------------------------------
{
	Clear ();
	if (nullptr == svSource)
	{
		return svSource;    // Empty svSource compares to nullptr.  SURPRISE!
		//## no, an empty svSource (when default constructed) is a nullptr.
		//## But I would really test for .empty() or .size(), as I doubt
		//## nullptr would compare equal to ""
	}

	size_t iSize{svSource.size ()};

	bool bError{false};

	svSource = Path::Parse (svSource);

	if (!bError)
	{
		if (iSize > 0 && svSource[0] == '?')
		{
			svSource = Query::Parse (svSource);  // optional
			iSize = svSource.size ();
		}
	}

	if (!bError)
	{
		if (iSize > 0 && svSource[0] == '#')
		{
			svSource = Fragment::Parse (svSource);
		}
	}

	bError = (0 != svSource.size ());

	if (!bError)
	{
		svSource.remove_prefix (svSource.size ());
	}
	else
	{
		Clear ();
	}
	return svSource;
}

//-----------------------------------------------------------------------------
/// @brief class URI in group KURL.  Generate URI from members.
bool URI::Serialize (KString& sTarget) const
//-----------------------------------------------------------------------------
{
	bool bResult = true;

	bResult = Path               ::Serialize (sTarget);
	bResult = bResult && Query   ::Serialize (sTarget);
	bResult = bResult && Fragment::Serialize (sTarget);
	return bResult;
}



//-----------------------------------------------------------------------------
/// @brief class URL in group KURL.  parses w3 URL into needed parts.
/// URL contains the "scheme", "user", "password", "host", "port" and URI.
/// This is a complete accounting for all fields of the w3 URL.
/// scheme:[//[user[:password]@]host[:port]][/path][?query][#fragment]
/// ------     ----  --------   ----  ----   -----   -----   --------
/// URL extracts and stores all elements of a URL.
KStringView URL::Parse (KStringView svSource)
//-----------------------------------------------------------------------------
{
	Clear ();
	if (nullptr == svSource)
	{
		return svSource;
	}

	bool bResult{true};
	bool bError{false};
	size_t iBefore, iAfter;

	iBefore  = svSource.size ();
	svSource = Protocol::Parse (svSource);                  // mandatory
	iAfter   = svSource.size ();
	bResult  = (iBefore != iAfter);
	if (bResult)
	{
		svSource = User::Parse (svSource);                  // optional

		if (getProtocolEnum () != FILE)
		{
			iBefore  = svSource.size ();
			svSource = Domain::Parse (svSource);            // mandatory
			iAfter   = svSource.size ();
			bResult  = (iBefore != iAfter);
		}
	}
	if (bResult)
	{
		svSource = URI::Parse (svSource);                   // mandatory
	}

	bError = !bResult;

	if (bError)
	{
		Clear ();
		bError = true;
	}
	if (!bError)
	{
		svSource.remove_prefix (svSource.size ());
	}

	return svSource;
}

//-----------------------------------------------------------------------------
/// @brief class URL in group KURL.  Generate URL from members.
bool URL::Serialize (KString& sTarget) const
//-----------------------------------------------------------------------------
{
	bool bResult = true;

	bResult &= Protocol::Serialize (sTarget);
	bResult &= User    ::Serialize (sTarget);
	bResult &= Domain  ::Serialize (sTarget);
	bResult &= URI     ::Serialize (sTarget);

	return bResult;
}

} // namespace KURL

/** @} */ // End of group KURL

} // namespace dekaf2
