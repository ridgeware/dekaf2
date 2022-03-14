/*
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

				if (svCheckForDotCo.starts_with (".co."))
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

//-----------------------------------------------------------------------------
bool kIsSubDomainOf(const url::KDomain& Domain, const url::KDomain& SubDomain)
//-----------------------------------------------------------------------------
{
	KString sDomain    = Domain.get().ToLowerASCII();
	KString sSubDomain = SubDomain.get().ToLowerASCII();

	if (sSubDomain.ends_with(sDomain))
	{
		if (sSubDomain.size() == sDomain.size())
		{
			return true;
		}
		// check for real subdomain:
		// Domain:        mydomain.com
		// SubDomain: www.mydomain.com
		else if (sSubDomain[sSubDomain.size() - sDomain.size() - 1] == '.')
		{
			return true;
		}
	}
	return false;

} // kIsSubDomainOf

namespace url {

namespace detail {

template class URIComponent<URLEncodedString, URIPart::User,     '\0', false, true,  true >;
template class URIComponent<URLEncodedString, URIPart::Password, '\0', false, true,  true >;
template class URIComponent<URLEncodedString, URIPart::Domain,   '\0', false, false, true >;
template class URIComponent<URLEncodedUInt,   URIPart::Port,     ':',  true,  false, false>;
template class URIComponent<URLEncodedString, URIPart::Path,     '/',  false, false, true >;
template class URIComponent<URLEncodedQuery,  URIPart::Query,    '?',  true,  false, false>;
template class URIComponent<URLEncodedString, URIPart::Fragment, '#',  true,  false, true >;

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
	{   80, "ws"     , "ws://"     },
	{  443, "wss"    , "wss://"    },
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

	if (! iPort)
	{
		if (m_eProto == UNKNOWN)
		{
			kDebug (1, "no default port for protocol {} ('{}') - returning 0", m_eProto, m_sProto);
		}
		else
		{
			kDebug (1, "no default port for protocol {} ('{}') - returning 0", m_eProto, s_Canonical[m_eProto].sName);
		}
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
KResource::KResource(const KURL& url)
//-------------------------------------------------------------------------
	: Path(url.Path)
	, Query(url.Query)
{
}

//-------------------------------------------------------------------------
KStringView KResource::Parse(KStringView svSource)
//-------------------------------------------------------------------------
{
	// identify a resource from a larger URL string
	auto pos = svSource.find_first_of("/?");

	if (pos != KStringView::npos
		&& pos > 0
		&& svSource[pos] == '/'
		&& svSource[pos-1] == ':')
	{
		// this is most likely the scheme separator ("://"), step past it
		pos = svSource.find_first_of("/?", pos + 2);
	}

	if (pos != KStringView::npos)
	{
		svSource.remove_prefix(pos);
		svSource = ParseResourcePart(svSource);
	}
	else
	{
		clear();
	}

	return svSource;
}

//-------------------------------------------------------------------------
KStringView KResource::ParseResourcePart(KStringView svSource)
//-------------------------------------------------------------------------
{
	svSource = Path.Parse      (svSource, true);
	svSource = Query.Parse     (svSource, true);

	return svSource;
}

//-------------------------------------------------------------------------
void KResource::clear()
//-------------------------------------------------------------------------
{
	Path.clear();
	Query.clear();
}

//-------------------------------------------------------------------------
bool KResource::Serialize(KStringRef& sTarget) const
//-------------------------------------------------------------------------
{
	Query.WantStartSeparator();
	return Path.Serialize          (sTarget)
	        && Query.Serialize     (sTarget);
}

//-------------------------------------------------------------------------
bool KResource::Serialize(KOutStream& sTarget) const
//-------------------------------------------------------------------------
{
	Query.WantStartSeparator();
	return Path.Serialize          (sTarget)
	        && Query.Serialize     (sTarget);
}

//-------------------------------------------------------------------------
bool operator==(const KResource& left, const KResource& right)
//-------------------------------------------------------------------------
{
	return left.Path     == right.Path
	    && left.Query    == right.Query;
}

//-------------------------------------------------------------------------
bool KResource::operator<(const KResource& other) const
//-------------------------------------------------------------------------
{
	if (Path  < other.Path ) return true;
	if (Path  > other.Path ) return false;
	if (Query < other.Query) return true;

	return false;
}

//-------------------------------------------------------------------------
KStringView KURL::Parse(KStringView svSource)
//-------------------------------------------------------------------------
{
	svSource = Protocol.Parse  (svSource); // mandatory, but we do not enforce
	svSource = User.Parse      (svSource);
	svSource = Password.Parse  (svSource);
	svSource = Domain.Parse    (svSource); // mandatory for non-files, but we do not enforce
	svSource = Port.Parse      (svSource);
	svSource = KResource::ParseResourcePart(svSource);
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
	KResource::clear();
	Fragment.clear();
}

//-------------------------------------------------------------------------
bool KURL::Serialize(KStringRef& sTarget) const
//-------------------------------------------------------------------------
{
	Port.WantStartSeparator();
	Fragment.WantStartSeparator();
	return Protocol.Serialize      (sTarget)
	        && User.Serialize      (sTarget, Password.empty() ? '@' : ':')
	        && Password.Serialize  (sTarget, '@')
	        && Domain.Serialize    (sTarget)
	        && Port.Serialize      (sTarget)
	        && KResource::Serialize(sTarget)
	        && Fragment.Serialize  (sTarget);
}

//-------------------------------------------------------------------------
bool KURL::Serialize(KOutStream& sTarget) const
//-------------------------------------------------------------------------
{
	Port.WantStartSeparator();
	Fragment.WantStartSeparator();
	return Protocol.Serialize      (sTarget)
	        && User.Serialize      (sTarget, Password.empty() ? '@' : ':')
	        && Password.Serialize  (sTarget, '@')
	        && Domain.Serialize    (sTarget)
	        && Port.Serialize      (sTarget)
	        && KResource::Serialize(sTarget)
	        && Fragment.Serialize  (sTarget);
}

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
bool KURL::operator<(const KURL& other) const
//-------------------------------------------------------------------------
{
	if (Protocol < other.Protocol) return true;
	if (Protocol > other.Protocol) return false;
	if (User     < other.User    ) return true;
	if (User     > other.User    ) return false;
	if (Password < other.Password) return true;
	if (Password > other.Password) return false;
	if (Domain   < other.Domain  ) return true;
	if (Domain   > other.Domain  ) return false;
	if (Port     < other.Port    ) return true;
	if (Port     > other.Port    ) return false;
	if (Path     < other.Path    ) return true;
	if (Path     > other.Path    ) return false;
	if (Query    < other.Query   ) return true;
	if (Query    > other.Query   ) return false;
	if (Fragment < other.Fragment) return true;
	return false;
}

//-------------------------------------------------------------------------
KTCPEndPoint::KTCPEndPoint(const KURL& URL)
//-------------------------------------------------------------------------
    : Domain(URL.Domain)
    , Port(URL.Port)
{
	if (Port.empty())
	{
		if (URL.Protocol.DefaultPort() > 0)
		{
			Port = KString::to_string(URL.Protocol.DefaultPort());
		}
	}
}

//-------------------------------------------------------------------------
KStringView KTCPEndPoint::Parse(KStringView svSource)
//-------------------------------------------------------------------------
{
	// identify a TCPEndPoint from a larger URL string

	// extract the protocol / scheme if existing, we use it later to set a port
	// if omitted
	url::KProtocol Protocol;
	svSource = Protocol.Parse  (svSource);

	auto pos = svSource.find_first_of("@/;?#");

	if (pos != KStringView::npos && svSource[pos] == '@')
	{
		// this is most likely the username separator, step past it
		svSource.remove_prefix(pos + 1);
	}

	svSource = Domain.Parse    (svSource);
	svSource = Port.Parse      (svSource);

	if (Port.empty() && !Protocol.empty())
	{
		Port = KString::to_string(Protocol.DefaultPort());
	}

	return svSource;
}

//-------------------------------------------------------------------------
void KTCPEndPoint::clear()
//-------------------------------------------------------------------------
{
	Domain.clear();
	Port.clear();
}

//-------------------------------------------------------------------------
bool KTCPEndPoint::Serialize(KStringRef& sTarget) const
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
	    && left.Port   == right.Port;
}

//-------------------------------------------------------------------------
bool KTCPEndPoint::operator<(const KTCPEndPoint& other) const
//-------------------------------------------------------------------------
{
	if (Domain < other.Domain) return true;
	if (Domain > other.Domain) return false;
	if (Port   < other.Port  ) return true;
	return false;
}

// old boost::multi_index versions are not noexcept move constructable, so we drop this
// test in case..
static_assert(!std::is_nothrow_move_constructible<URLEncodedQuery>::value ||
			  std::is_nothrow_move_constructible<KURL>::value,
			  "KURL is intended to be nothrow move constructible, but is not!");

static_assert(!std::is_nothrow_move_constructible<URLEncodedQuery>::value ||
			  std::is_nothrow_move_constructible<KResource>::value,
			  "KResource is intended to be nothrow move constructible, but is not!");

static_assert(std::is_nothrow_move_constructible<KTCPEndPoint>::value,
			  "KTCPEndPoint is intended to be nothrow move constructible, but is not!");

static_assert(std::is_nothrow_move_constructible<url::KUser>::value,
			  "url::KUser is intended to be nothrow move constructible, but is not!");

static_assert(std::is_nothrow_move_constructible<url::KPassword>::value,
			  "url::KPassword is intended to be nothrow move constructible, but is not!");

static_assert(std::is_nothrow_move_constructible<url::KDomain>::value,
			  "url::KDomain is intended to be nothrow move constructible, but is not!");

static_assert(std::is_nothrow_move_constructible<url::KPort>::value,
			  "url::KPort is intended to be nothrow move constructible, but is not!");

static_assert(std::is_nothrow_move_constructible<url::KPath>::value,
			  "url::KPath is intended to be nothrow move constructible, but is not!");

static_assert(!std::is_nothrow_move_constructible<URLEncodedQuery>::value ||
			  std::is_nothrow_move_constructible<url::KQuery>::value,
			  "url::KQuery is intended to be nothrow move constructible, but is not!");

static_assert(std::is_nothrow_move_constructible<url::KFragment>::value,
			  "url::KFragment is intended to be nothrow move constructible, but is not!");

static_assert(std::is_nothrow_move_constructible<url::KProtocol>::value,
			  "url::KProtocol is intended to be nothrow move constructible, but is not!");

} // namespace dekaf2
