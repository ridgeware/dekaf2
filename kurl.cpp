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
*  clear();                            // Restore to original empty state
*  KString();                          // same as serialize
*/

#include "kstringview.h"
#include "kstring.h"
#include "kstringutils.h"
#include "kurl.h"


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
	clear ();

	if (!svSource.empty ())
	{
		size_t iFound = svSource.find_first_of (':');

		if (iFound != KStringView::npos)
		{
			KStringView svProto = svSource.substr (0, iFound);

			// we do accept schemata with only one slash, as that is
			// a common typo (but we do correct them when serializing)
			if (svSource.size () >= iFound + 1
			    && svSource[iFound + 1] == '/')
			{
				svSource.remove_prefix (iFound + 2);

				if (!svSource.empty () && svSource.front () == '/')
				{
					svSource.remove_prefix (1);
				}

				kUrlDecode (svProto, m_sProto);

				m_eProto = UNKNOWN;
				// we do not want to recognize MAILTO in this branch, as it
				// has the wrong separator. But if we find it we store it as
				// unknown and then reproduce the same on serialization.
				for (uint16_t iProto = MAILTO + 1; iProto < UNKNOWN; ++iProto)
				{
					if (m_sCanonical[iProto] == m_sProto)
					{
						m_eProto = static_cast<eProto>(iProto);
						break;
					}
				}
			}
			else if (svProto == "mailto")
			{
				m_eProto = MAILTO;
				svSource.remove_prefix (iFound + 1);
			}
		}
	}

	return svSource;
}

//-----------------------------------------------------------------------------
/// @brief class Protocol in group KURL.  Generate Protocol portion of URL.
/// Class Protocol parses and maintains "scheme" portion of w3 URL.
bool Protocol::Serialize (KString& sTarget) const
//-----------------------------------------------------------------------------
{
	// m_eProto is UNKNOWN for protocols like "opaquelocktoken://"
	switch (m_eProto)
	{
		case UNDEFINED:
			return false;

		case UNKNOWN:
			kUrlEncode(m_sProto, sTarget);
			sTarget += "://";
			break;

		case MAILTO:
			sTarget += m_sCanonical[m_eProto];
			sTarget += ':';
			break;

		default:
			sTarget += m_sCanonical[m_eProto];
			sTarget += "://";
			break;
	}

	return true;
}

//-----------------------------------------------------------------------------
/// @brief class Protocol in group KURL.  Restores instance to empty state
void Protocol::clear()
//-----------------------------------------------------------------------------
{
	m_sProto.clear ();
	m_eProto = UNDEFINED;
}

// watch out: in Serialize(), we assume that the schemata
// do not need URL encoding! Therefore, when you add one
// that does, please use the URL encoded form here, too.
const KString Protocol::m_sCanonical [UNKNOWN+1]
{
	"",       // Empty placeholder for UNDEFINED, parse has not been run yet.
	"mailto",
	"http",
	"https",
	"file",
	"ftp",
	""        // Empty placeholder for UNKNOWN, use m_sProto.
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
	clear ();

	if (!svSource.empty ())
	{
		size_t iFound = svSource.find ('@');
		if (iFound != KStringView::npos)
		{
			KStringView svUser;
			KStringView svPass;

			size_t iColon = svSource.find (':');
			if (iColon < iFound)
			{
				svUser = svSource.substr (0, iColon);
				svPass = svSource.substr (iColon + 1, iFound - iColon - 1);
			}
			else
			{
				svUser = svSource.substr (0, iFound);
			}

			kUrlDecode (svUser, m_sUser);
			kUrlDecode (svPass, m_sPass);

			svSource.remove_prefix (iFound + 1);
		}
	}

	return svSource;
}

//-----------------------------------------------------------------------------
/// @brief class User in group KURL.  Generate User portion of URL.
bool User::Serialize (KString& sTarget) const
//-----------------------------------------------------------------------------
{
	// TODO Should username/password be url encoded?
	if (!m_sUser.empty ())
	{
		// TODO These exclusions are speculative.  Should they change?
		// RFC3986 rules apply to exclusions.
		// RFC3986 2.2. reserved    = gen(":/?#[]@") + sub("!$&'()*+,;=")
		// RFC3986 2.3. unreserved  = ALPHA + DIGIT + "-._~"
		// RFC3986 7.5. password field is deprecated

		KStringView svExclude{"-._~@/"};  // RFC3986 2.3 supplemented with "@/"
		kUrlEncode (m_sUser, sTarget, svExclude);

		if (!m_sPass.empty ())
		{
			sTarget += ':';
			// TODO These exclusions are speculative.  Should they change?
			// Rules for encoding passwords are not explicit.
			// Human-centric passwords are likely to include difficult chars.
			// Password field is deprecated as insecure.  This is legacy support.
			// utests all pass, but password field is not stress-tested.
			kUrlEncode (m_sPass, sTarget, svExclude);
		}

		sTarget += '@';
	}

	return true;
}

//-------------------------------------------------------------------------
KStringView Domain::getBaseDomain () const
//-------------------------------------------------------------------------
{
	// lazy evaluation of base domain - it is a rarely used function,
	// but has significant costs
	if (m_sBaseName.empty() && !m_sHostName.empty())
	{
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
		auto iDotEnd = m_sHostName.rfind ('.');
		if (iDotEnd == 0 || iDotEnd == KString::npos)
		{
			// Ignore simple non-dot hostname (localhost).
		}
		else
		{
			// When there is at least 1 dot, look for domain name features

			auto iDotStart = m_sHostName.rfind ('.', iDotEnd - 1);

			if (iDotStart != KString::npos)
			{
				// When there are at least 2 dots, look for ".co.".

				KStringView svCheckForDotCo (m_sHostName);
				svCheckForDotCo.remove_prefix (iDotStart);

				if (svCheckForDotCo.StartsWith (".co."))
				{
					iDotEnd = iDotStart;
					iDotStart = m_sHostName.rfind ('.', iDotStart - 1);
				}
			}

			if (iDotStart == KString::npos)
			{
				iDotStart = 0;
			}
			else
			{
				++iDotStart;
			}

			m_sBaseName = kToUpper(KStringView(m_sHostName.data() + iDotStart, iDotEnd - iDotStart));
		}
	}

	return m_sBaseName;
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
KStringView Domain::ParseHostName (KStringView svSource, bool bDecode)
//-----------------------------------------------------------------------------
{
	if (!svSource.empty())
	{
		size_t iFound;

		if (svSource.front() == '[')
		{
			// an IPv6 address
			iFound = svSource.find(']');
			if (iFound == KStringView::npos)
			{
				// unterminated IPv6 address
				return svSource;
			}

			// we want to include the closing ] into the hostname string
			++iFound;
		}
		else
		{
			// anything else than an IPv6 address
			iFound = svSource.find_first_of (":/?#");
			if (iFound == KStringView::npos)
			{
				iFound = svSource.size();
			}
		}

		KStringView svHostName = svSource.substr(0, iFound);

		if (bDecode)
		{
			m_sHostName.clear ();
			// decode while copying
			kUrlDecode (svHostName, m_sHostName);
		}
		else
		{
			m_sHostName = svHostName;
		}

		svSource.remove_prefix(iFound);
	}

	return svSource;
}

//-----------------------------------------------------------------------------
/// @brief class Domain in group KURL.  parses "host:port" portion of w3 URL.
KStringView Domain::Parse (KStringView svSource)
//-----------------------------------------------------------------------------
{
	clear ();

	svSource = ParseHostName (svSource);

	if (!svSource.empty() && !m_sHostName.empty())
	{
		// we have extracted a host name

		if (svSource.front() == ':')
		{
			auto iNext = svSource.find_first_of ("/?#", 1);
			if (iNext == KStringView::npos)
			{
				iNext = svSource.size ();
			}

			KStringView svPortName{svSource.data () + 1, iNext - 1};

			// clear() has set m_iPortNum to 0
			for (auto ch : svPortName)
			{
				if (!std::isdigit(ch))
				{
					if (ch == '%')
					{
						// url decode port
						KString sPortName;
						kUrlDecode (svPortName, sPortName);
						m_iPortNum = static_cast<uint16_t> (kToUInt (sPortName));
					}
					// else leave so far decoded port number, it has garbage appended
					break;
				}
				m_iPortNum *= 10;
				m_iPortNum += ch - '0';
			}

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
	if (!m_sHostName.empty ())
	{
		// TODO These exclusions are speculative.  Should they change?
		KStringView svExclude{"-._~@/"};  // RFC3986 2.3 supplemented with "@/"
		kUrlEncode (m_sHostName, sTarget, svExclude);
		if (m_iPortNum)
		{
			sTarget += ':';
			sTarget += std::to_string (m_iPortNum);
		}
	}

	return true;
}



//-----------------------------------------------------------------------------
/// @brief class Path in group KURL.  Parses "path" portion of w3 URL.
/// RFC3986 3.3: (See RFC)
/// Implementation: All characters after domain from '/' to 1st of "?#\0".
/// scheme:[//[user[:password]@]host[:port]][/path][?query][#fragment]
///                                          -----   -----   --------
/// Path extracts and stores a KStringView of URL "path"
KStringView Path::Parse (KStringView svSource, bool bRequiresPrefix)
//-----------------------------------------------------------------------------
{
	clear ();

	if (!svSource.empty ())
	{
		if (!bRequiresPrefix || svSource.front () == '/')
		{
			// Remainder after path may be query or fragment.
			auto iFound = svSource.find_first_of ("?#", 1);
			if (iFound == KStringView::npos)
			{
				iFound = svSource.size ();
			}

			// m_sPath is cleared by clear() above
			kUrlDecode(svSource.substr(0, iFound), m_sPath);

			svSource.remove_prefix (iFound);
		}
	}

	return svSource;
}

//-----------------------------------------------------------------------------
/// @brief class Path in group KURL.  Generate Path portion of URL.
bool Path::Serialize (KString& sTarget) const
//-----------------------------------------------------------------------------
{
	if (!m_sPath.empty ())
	{
		KStringView svExclude{"-._~@/"};  // RFC3986 2.3 supplemented with "@/"
		kUrlEncode (m_sPath, sTarget, svExclude);
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
KStringView Query::Parse (KStringView svSource, bool bRequiresPrefix)
//-----------------------------------------------------------------------------
{
	clear ();

	if (!svSource.empty ())
	{
		if (svSource.front () == '?')
		{
			svSource.remove_prefix (1);
		}
		else if (bRequiresPrefix)
		{
			return svSource;
		}

		if (!svSource.empty())
		{
			auto iFound = svSource.find ('#');
			if (iFound == KStringView::npos)
			{
				iFound = svSource.size();
			}

			KStringView svQuery{svSource.substr (0, iFound)};

			svQuery = decode (svQuery);  // KurlDecode must be done on key=val separately.

			svSource.remove_prefix (iFound - svQuery.size());
		}
	}

	return svSource;
}

//-----------------------------------------------------------------------------
/// @brief class Query in group KURL.  Split/decode.
/// Split query string into key:value pairs, then decode keys and values.
KStringView Query::decode (KStringView svQuery)
//-----------------------------------------------------------------------------
{
	while (!svQuery.empty())
	{
		// Get bounds of query pair
		auto iEnd = svQuery.find ('&'); // Find separator
		if (iEnd == KString::npos)
		{
			iEnd = svQuery.size();
		}

		KStringView svEncoded{svQuery.substr (0, iEnd)};

		auto iEquals = svEncoded.find ('=');
		if (iEquals > iEnd)
		{
			iEquals = iEnd;
		}

		KStringView svKeyEncoded (svEncoded.substr (0          , iEquals));
		KStringView svValEncoded (svEncoded.substr (iEquals + 1         ));

		// we can have empty values
		if (svKeyEncoded.size () /* && svValEncoded.size () */ )
		{
			// decoding may only happen AFTER '=' '&' detections
			KString sKey, sVal;
			kUrlDecode (svKeyEncoded, sKey);
			kUrlDecode (svValEncoded, sVal);
			m_kpQuery.Add (std::move (sKey), std::move (sVal));
		}

		svQuery.remove_prefix(iEnd + 1);
	}

	return svQuery;
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
KStringView Fragment::Parse (KStringView svSource, bool bRequiresPrefix)
//-----------------------------------------------------------------------------
{
	clear ();

	if (!svSource.empty ())
	{
		if (svSource.front() == '#')
		{
			svSource.remove_prefix (1);
			m_bHash = true;
		}
		else if (bRequiresPrefix)
		{
			return svSource;
		}

		kUrlDecode (svSource, m_sFragment);

		svSource.clear();
	}

	return svSource;
}

//-----------------------------------------------------------------------------
/// @brief class Fragment in group KURL.  Generate fragment portion of URL.
bool Fragment::Serialize (KString& sTarget) const
//-----------------------------------------------------------------------------
{
	if (m_bHash || !m_sFragment.empty())
	{
		sTarget += '#';
	}

	if (!m_sFragment.empty ())
	{
		kUrlEncode (m_sFragment, sTarget);
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
	clear ();

	svSource = Path    ::Parse (svSource, true);
	svSource = Query   ::Parse (svSource, true); // optional
	svSource = Fragment::Parse (svSource, true); // optional

	return svSource;
}

//-----------------------------------------------------------------------------
/// @brief class URI in group KURL.  Generate URI from members.
bool URI::Serialize (KString& sTarget) const
//-----------------------------------------------------------------------------
{
	return Path    ::Serialize (sTarget)
	    && Query   ::Serialize (sTarget)
	    && Fragment::Serialize (sTarget);
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
	clear ();

	svSource = Protocol::Parse (svSource); // mandatory, but we do not enforce
	svSource = User    ::Parse (svSource); // optional
	svSource = Domain  ::Parse (svSource); // mandatory for non-files, but we do not enforce
	svSource = URI     ::Parse (svSource); // optional

	return svSource;
}

//-----------------------------------------------------------------------------
/// @brief class URL in group KURL.  Generate URL from members.
bool URL::Serialize (KString& sTarget) const
//-----------------------------------------------------------------------------
{
	return Protocol::Serialize (sTarget)
	    && User    ::Serialize (sTarget)
	    && Domain  ::Serialize (sTarget)
	    && URI     ::Serialize (sTarget);
}

} // namespace KURL

/** @} */ // End of group KURL

} // namespace dekaf2
