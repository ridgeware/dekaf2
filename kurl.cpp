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


namespace dekaf2
{

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

				m_eProto = UNKNOWN;
				// we do not want to recognize MAILTO in this branch, as it
				// has the wrong separator. But if we find it we store it as
				// unknown and then reproduce the same on serialization.
				for (uint16_t iProto = MAILTO + 1; iProto < UNKNOWN; ++iProto)
				{
					if (m_sCanonical[iProto] == svProto)
					{
						m_eProto = static_cast<eProto>(iProto);
						break;
					}
				}

				if (m_eProto == UNKNOWN)
				{
					// only store the protocol scheme if it is not one
					// of the canonical
					kUrlDecode (svProto, m_sProto);
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
/// @brief Generate Protocol/Scheme portion of URL.
// Class Protocol parses and maintains "scheme" portion of w3c URL.
bool Protocol::Serialize (KString& sTarget) const
//-----------------------------------------------------------------------------
{
	// m_eProto is UNKNOWN for protocols like "opaquelocktoken://"
	switch (m_eProto)
	{
		case UNDEFINED:
			return false;

		case UNKNOWN:
			kUrlEncode(m_sProto, sTarget, URIPart::Protocol);
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
/// Restores instance to empty state
void Protocol::clear()
//-----------------------------------------------------------------------------
{
	m_sProto.clear ();
	m_eProto = UNDEFINED;
}

// watch out: in Parse() and Serialize(), we assume that the schemata
// do not need URL encoding! Therefore, when you add one that does,
// please use the URL encoded form here, too.
const KStringView::value_type* Protocol::m_sCanonical [UNKNOWN+1] =
{
	"",       // Empty placeholder for UNDEFINED, parse has not been run yet.
	"mailto",
	"http",
	"https",
	"file",
	"ftp",
	"git",
	"svn",
	"irc",
	"news",
	"nntp",
	"telnet",
	"gopher", // pure nostalgia
	""        // Empty placeholder for UNKNOWN, use m_sProto.
};

//-------------------------------------------------------------------------
KStringView URI::Parse(KStringView svSource)
//-------------------------------------------------------------------------
{
	clear ();

	svSource = path.Parse      (svSource, true);
	svSource = pathparam.Parse (svSource, true);
	svSource = query.Parse     (svSource, true);
	svSource = fragment.Parse  (svSource, true);

	return svSource;
}

//-------------------------------------------------------------------------
void URI::clear()
//-------------------------------------------------------------------------
{
	path.clear();
	pathparam.clear();
	query.clear();
	fragment.clear();
}

//-------------------------------------------------------------------------
bool URI::Serialize(KString& sTarget) const
//-------------------------------------------------------------------------
{
	return path.Serialize          (sTarget)
	        && pathparam.Serialize (sTarget)
	        && query.Serialize     (sTarget)
	        && fragment.Serialize  (sTarget);
}

//-------------------------------------------------------------------------
URI& URI::operator=(const URL& url)
//-------------------------------------------------------------------------
{
	path      = url.path;
	pathparam = url.pathparam;
	query     = url.query;
	fragment  = url.fragment;
	return *this;
}

//-------------------------------------------------------------------------
KStringView URL::Parse(KStringView svSource)
//-------------------------------------------------------------------------
{
	clear ();

	svSource = protocol.Parse  (svSource); // mandatory, but we do not enforce
	svSource = user.Parse      (svSource); // optional
	svSource = password.Parse  (svSource); // optional
	svSource = domain.Parse    (svSource); // mandatory for non-files, but we do not enforce
	svSource = port.Parse      (svSource);
	svSource = path.Parse      (svSource, true);
	svSource = pathparam.Parse (svSource, true);
	svSource = query.Parse     (svSource, true);
	svSource = fragment.Parse  (svSource, true);

	return svSource;
}

//-------------------------------------------------------------------------
void URL::clear()
//-------------------------------------------------------------------------
{
	protocol.clear();
	user.clear();
	password.clear();
	domain.clear();
	port.clear();
	path.clear();
	pathparam.clear();
	query.clear();
	fragment.clear();
}

//-------------------------------------------------------------------------
bool URL::Serialize(KString& sTarget) const
//-------------------------------------------------------------------------
{
	return protocol.Serialize      (sTarget)
	        && user.Serialize      (sTarget)
	        && password.Serialize  (sTarget)
	        && domain.Serialize    (sTarget)
	        && port.Serialize      (sTarget)
	        && path.Serialize      (sTarget)
	        && pathparam.Serialize (sTarget)
	        && query.Serialize     (sTarget)
	        && fragment.Serialize  (sTarget);
}

} // namespace KURL

/** @} */ // End of group KURL

} // namespace dekaf2
