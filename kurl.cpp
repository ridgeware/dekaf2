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

#include "kstringview.h"
#include "kstring.h"
#include "kstringutils.h"
#include "kurl.h"
#include "kstack.h"


namespace dekaf2 {

//-------------------------------------------------------------------------
KString kGetBaseDomain (KStringView sHostName)
//-------------------------------------------------------------------------
{
	KString sBaseName;

	if (!sHostName.empty())
	{
		// sBaseName   is special-cased
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

		auto iDotEnd = sHostName.rfind ('.');
		if (iDotEnd == 0 || iDotEnd == KStringView::npos)
		{
			// Ignore simple non-dot hostname (localhost).
		}
		else
		{
			// When there is at least 1 dot, look for domain name features

			auto iDotStart = sHostName.rfind ('.', iDotEnd - 1);

			if (iDotStart != KStringView::npos)
			{
				// When there are at least 2 dots, look for ".co.".

				KStringView svCheckForDotCo (sHostName);
				svCheckForDotCo.remove_prefix (iDotStart);

				if (svCheckForDotCo.StartsWith (".co."))
				{
					iDotEnd = iDotStart;
					iDotStart = sHostName.rfind ('.', iDotStart - 1);
				}
			}

			if (iDotStart == KStringView::npos)
			{
				iDotStart = 0;
			}
			else
			{
				++iDotStart;
			}

			sBaseName = kToUpper(KStringView(sHostName.data() + iDotStart, iDotEnd - iDotStart));
		}
	}

	return sBaseName;
}


namespace url {

namespace detail {

#ifndef __clang__
template class URIComponent<URLEncodedString, URIPart::User,     '\0', false, true,  true >;
template class URIComponent<URLEncodedString, URIPart::Password, '\0', false, true,  true >;
template class URIComponent<URLEncodedString, URIPart::Domain,   '\0', false, false, true >;
template class URIComponent<URLEncodedString, URIPart::Port,     ':',  true,  false, true >;
template class URIComponent<URLEncodedString, URIPart::Path,     '/',  false, false, true >;
template class URIComponent<URLEncodedQuery,  URIPart::Query,    '?',  true,  false, false>;
template class URIComponent<URLEncodedString, URIPart::Fragment, '#',  true,  false, true >;
#endif

}

// watch out: in Parse() and Serialize(), we assume that the schemata
// do not need URL encoding! Therefore, when you add one that does,
// please use the URL encoded form here, too.

struct Protocols
{
	const uint16_t    iPort;
	const KStringView sName;
	const KStringView sProtoPrefix;
};

constexpr Protocols s_Canonical [KProtocol::UNKNOWN+1] =
{
	{    0, ""       , ""          }, // Empty placeholder for UNDEFINED, parse has not been run yet.
	{   25, "mailto" , "mailto:"   }, // mailto stays first, auto second!
	{   80, "auto"   , "//"        }, // auto falls back to HTTP..
	{   80, "http"   , "http://"   },
	{  443, "https"  , "https://"  },
	{    0, "file"   , "file://"   },
	{   21, "ftp"    , "ftp://"    },
	{ 9418, "git"    , "git://"    },
	{    0, "svn"    , "svn://"    },
	{  531, "irc"    , "irc://"    },
	{  119, "news"   , "news://"   },
	{  119, "nntp"   , "nntp://"   },
	{   23, "telnet" , "telnet://" },
	{   70, "gopher" , "gopher://" }, // pure nostalgia
	{    0, "unix"   , "unix://"   }, // this is for unix socket files ("unix:///this/is/my/socket")
	{   25, "smtp"   , "smtp://"   },
	{  587, "smtps"  , "smtps://"  },
	{    0, ""       , ""          }  // Empty placeholder for UNKNOWN, use m_sProto.
};

//-----------------------------------------------------------------------------
void KProtocol::SetProto(KStringView svProto)
//-----------------------------------------------------------------------------
{
	m_eProto = UNKNOWN;
	// we do not want to recognize MAILTO in this branch, as it
	// has the wrong separator. But if we find it we store it as
	// unknown and then reproduce the same on serialization.
	for (uint16_t iProto = MAILTO + 2; iProto < UNKNOWN; ++iProto)
	{
		if (s_Canonical[iProto].sName == svProto)
		{
			m_eProto = static_cast<eProto>(iProto);
			break;
		}
	}

	if (m_eProto == UNKNOWN)
	{
		// only store the protocol scheme if it is not one
		// of the canonical
		if (!svProto.empty())
		{
			m_sProto = svProto;
			m_sProto += "://";
		}
	}

} // SetProto

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
KStringView KProtocol::Parse (KStringView svSource, bool bAcceptWithoutColon)
//-----------------------------------------------------------------------------
{
	clear ();

	if (!svSource.empty ())
	{
		// we search for the colon, but also for all other chars
		// that would indicate that we are not in a protocol part of the URL
		size_t iFound = svSource.find_first_of (": \t@/;?=#");

		if (iFound != KStringView::npos)
		{
			// check if the found char is the colon
			if (svSource[iFound] == ':')
			{
				KStringView svProto = svSource.substr (0, iFound);
				// we do accept schemata with only one slash, as that is
				// a common typo (but we do correct them when serializing)
				if (svSource.size () > iFound + 1
					&& svSource[iFound + 1] == '/')
				{
					svSource.remove_prefix (iFound + 2);

					if (!svSource.empty () && svSource.front () == '/')
					{
						svSource.remove_prefix (1);
					}

					SetProto(svProto);
				}
				else if (svProto == "mailto")
				{
					m_eProto = MAILTO;
					svSource.remove_prefix (iFound + 1);
				}
			}
			else if (iFound == 0 && svSource[0] == '/')
			{
				if (svSource.size() > 1 && svSource[1] == '/')
				{
					m_eProto = AUTO;
					svSource.remove_prefix(2);
				}
			}
		}
		else if (bAcceptWithoutColon)
		{
			SetProto(svSource);
			svSource.clear();
		}
	}

	return svSource;
}

//-----------------------------------------------------------------------------
/// @brief Generate KProtocol/Scheme portion of URL.
// Class KProtocol parses and maintains "scheme" portion of w3c URL.
KStringView KProtocol::Serialize () const
//-----------------------------------------------------------------------------
{
	KStringView sProto;

	// m_eProto is UNKNOWN for protocols like "opaquelocktoken://"
	switch (m_eProto)
	{
		case UNDEFINED:
			// The serialization is correctly empty when no value was parsed.
			break;

		case UNKNOWN:
			sProto = m_sProto;
			break;

		default:
			sProto = s_Canonical[m_eProto].sProtoPrefix;
			break;
	}

	return sProto;
}

//-------------------------------------------------------------------------
uint16_t KProtocol::DefaultPort() const
//-------------------------------------------------------------------------
{
	uint16_t iPort = s_Canonical[m_eProto].iPort;

	if (!iPort)
	{
		kWarning("no default port - return 0");
	}

	return iPort;
}

//-----------------------------------------------------------------------------
/// Restores instance to empty state
void KProtocol::clear()
//-----------------------------------------------------------------------------
{
	m_sProto.clear ();
	m_eProto = UNDEFINED;
}


} // end of namespace url


//-------------------------------------------------------------------------
KURI::KURI(KURL url)
//-------------------------------------------------------------------------
	: Path(std::move(url.Path))
	, Query(std::move(url.Query))
	, Fragment(std::move(url.Fragment))
{
}

//-------------------------------------------------------------------------
KStringView KURI::Parse(KStringView svSource)
//-------------------------------------------------------------------------
{
	KURL url(svSource);
	*this = std::move(url);
	return svSource;
}

//-------------------------------------------------------------------------
void KURI::clear()
//-------------------------------------------------------------------------
{
	Path.clear();
	Query.clear();
	Fragment.clear();
}

//-------------------------------------------------------------------------
bool KURI::Serialize(KString& sTarget) const
//-------------------------------------------------------------------------
{
	Query.WantStartSeparator();
	Fragment.WantStartSeparator();
	return Path.Serialize          (sTarget)
	        && Query.Serialize     (sTarget)
	        && Fragment.Serialize  (sTarget);
}

//-------------------------------------------------------------------------
bool KURI::Serialize(KOutStream& sTarget) const
//-------------------------------------------------------------------------
{
	Query.WantStartSeparator();
	Fragment.WantStartSeparator();
	return Path.Serialize          (sTarget)
	        && Query.Serialize     (sTarget)
	        && Fragment.Serialize  (sTarget);
}

//-------------------------------------------------------------------------
KStringView KURL::Parse(KStringView svSource)
//-------------------------------------------------------------------------
{
	clear ();

	svSource = Protocol.Parse  (svSource); // mandatory, but we do not enforce
	svSource = User.Parse      (svSource);
	svSource = Password.Parse  (svSource);
	svSource = Domain.Parse    (svSource); // mandatory for non-files, but we do not enforce
	svSource = Port.Parse      (svSource);
	svSource = Path.Parse      (svSource, true);
	svSource = Query.Parse     (svSource, true);
	svSource = Fragment.Parse  (svSource, true);

	return svSource;
}

//-------------------------------------------------------------------------
void KURL::clear()
//-------------------------------------------------------------------------
{
	Protocol.clear();
	User.clear();
	Password.clear();
	Domain.clear();
	Port.clear();
	Path.clear();
	Query.clear();
	Fragment.clear();
}

//-------------------------------------------------------------------------
bool KURL::Serialize(KString& sTarget) const
//-------------------------------------------------------------------------
{
	Port.WantStartSeparator();
	Query.WantStartSeparator();
	Fragment.WantStartSeparator();
	return Protocol.Serialize      (sTarget)
	        && User.Serialize      (sTarget)
	        && Password.Serialize  (sTarget)
	        && Domain.Serialize    (sTarget)
	        && Port.Serialize      (sTarget)
	        && Path.Serialize      (sTarget)
	        && Query.Serialize     (sTarget)
	        && Fragment.Serialize  (sTarget);
}

//-------------------------------------------------------------------------
bool KURL::Serialize(KOutStream& sTarget) const
//-------------------------------------------------------------------------
{
	Port.WantStartSeparator();
	Query.WantStartSeparator();
	Fragment.WantStartSeparator();
	return Protocol.Serialize      (sTarget)
	        && User.Serialize      (sTarget)
	        && Password.Serialize  (sTarget)
	        && Domain.Serialize    (sTarget)
	        && Port.Serialize      (sTarget)
	        && Path.Serialize      (sTarget)
	        && Query.Serialize     (sTarget)
	        && Fragment.Serialize  (sTarget);
}

//-------------------------------------------------------------------------
bool KURL::GetURI(KString& sTarget) const
//-------------------------------------------------------------------------
{
	Port.WantStartSeparator();
	Query.WantStartSeparator();
	Fragment.WantStartSeparator();
	return Path.Serialize      (sTarget)
	    && Query.Serialize     (sTarget)
	    && Fragment.Serialize  (sTarget);
}

//-------------------------------------------------------------------------
KString KURL::IsolatePath (KStringView sURL)
//-------------------------------------------------------------------------
{
	if (sURL.empty()) {
		return ("");
	}

	KStack <KString> Parts;
	kSplit (Parts, sURL, "/", /*sTrim*/"");

	KString  sPath;
	uint32_t iStartWith = (Parts[0].Contains(":")) ? 3: 0;

	for (auto ii=iStartWith; ii < Parts.size(); ++ii)
	{
		sPath += kFormat ("/{}", Parts[ii]);
	}

	return sPath;

} // IsolatePath

//-------------------------------------------------------------------------
bool operator==(const KURL& left, const KURL& right)
//-------------------------------------------------------------------------
{
	return left.Protocol     == right.Protocol
	        && left.User     == right.User
	        && left.Password == right.Password
	        && left.Domain   == right.Domain
	        && left.Port     == right.Port
	        && left.Path     == right.Path
	        && left.Query    == right.Query
	        && left.Fragment == right.Fragment;
}

//-------------------------------------------------------------------------
KTCPEndPoint::KTCPEndPoint(const KURL& URL)
//-------------------------------------------------------------------------
    : Domain(URL.Domain)
    , Port(URL.Port)
{
	if (Port.empty())
	{
		Port = KString::to_string(URL.Protocol.DefaultPort());
	}
}

//-------------------------------------------------------------------------
void KTCPEndPoint::clear()
//-------------------------------------------------------------------------
{
	Domain.clear();
	Port.clear();
}

//-------------------------------------------------------------------------
bool KTCPEndPoint::Serialize(KString& sTarget) const
//-------------------------------------------------------------------------
{
	Port.WantStartSeparator();
	return Domain.Serialize    (sTarget)
		&& Port.Serialize      (sTarget);
}

//-------------------------------------------------------------------------
bool KTCPEndPoint::Serialize(KOutStream& sTarget) const
//-------------------------------------------------------------------------
{
	Port.WantStartSeparator();
	return Domain.Serialize    (sTarget)
		&& Port.Serialize      (sTarget);
}
//-------------------------------------------------------------------------
bool operator==(const KTCPEndPoint& left, const KTCPEndPoint& right)
//-------------------------------------------------------------------------
{
	return left.Domain == right.Domain
	        && left.Port == right.Port;
}

} // namespace dekaf2
