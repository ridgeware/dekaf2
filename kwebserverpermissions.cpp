/*
//
// DEKAF(tm): Lighter, Faster, Smarter (tm)
//
// Copyright (c) 2026, Ridgeware, Inc.
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
*/

#include "kwebserverpermissions.h"
#include "kstringutils.h"
#include "kreader.h"
#include "klog.h"
#include <algorithm>

DEKAF2_NAMESPACE_BEGIN

//-----------------------------------------------------------------------------
KWebServerPermissions::Perms KWebServerPermissions::ParsePermissions(KStringView sPermissions)
//-----------------------------------------------------------------------------
{
	Perms iPerms = None;

	for (auto sPart : sPermissions.Split("|"))
	{
		sPart.Trim();
		auto sLower = kToLowerASCII(sPart);

		switch (sLower.Hash())
		{
			case "read"_hash:
				iPerms |= Read;
				break;

			case "write"_hash:
				iPerms |= Write;
				break;

			case "erase"_hash:
			case "delete"_hash:
				iPerms |= Erase;
				break;

			case "browse"_hash:
				iPerms |= Browse;
				break;

			case "all"_hash:
				iPerms |= All;
				break;

			case "none"_hash:
				iPerms = None;
				break;

			default:
				kDebug(1, "unknown permission flag: '{}'", sLower);
				break;
		}
	}

	return iPerms;

} // ParsePermissions

//-----------------------------------------------------------------------------
KString KWebServerPermissions::SerializePermissions(Perms iPerms)
//-----------------------------------------------------------------------------
{
	if (iPerms == None) return "none";
	if (iPerms == All)  return "all";

	KString sResult;

	auto Append = [&sResult](KStringView sFlag)
	{
		if (!sResult.empty()) sResult += '|';
		sResult += sFlag;
	};

	if (iPerms & Read)   Append("read");
	if (iPerms & Write)  Append("write");
	if (iPerms & Erase)  Append("erase");
	if (iPerms & Browse) Append("browse");

	return sResult;

} // SerializePermissions

//-----------------------------------------------------------------------------
KWebServerPermissions::Perms KWebServerPermissions::MethodToPermission(KHTTPMethod Method)
//-----------------------------------------------------------------------------
{
	switch (Method)
	{
		case KHTTPMethod::GET:
		case KHTTPMethod::HEAD:
		case KHTTPMethod::OPTIONS:
		case KHTTPMethod::PROPFIND:
			return Read;

		case KHTTPMethod::PUT:
		case KHTTPMethod::POST:
		case KHTTPMethod::PATCH:
		case KHTTPMethod::MKCOL:
		case KHTTPMethod::COPY:
			return Write;

		case KHTTPMethod::DELETE:
			return Erase;

		case KHTTPMethod::MOVE:
			return Write | Erase;

		default:
			return None;
	}

} // MethodToPermission

//-----------------------------------------------------------------------------
KWebServerPermissions::KWebServerPermissions(const KJSON& jConfig)
//-----------------------------------------------------------------------------
{
	SetDirectoryPermissions(jConfig);

} // ctor

//-----------------------------------------------------------------------------
void KWebServerPermissions::SetDirectoryPermissions(const KJSON& jConfig)
//-----------------------------------------------------------------------------
{
	// read default permissions
	const auto& sPerms = kjson::GetStringRef(jConfig, "permissions");

	if (!sPerms.empty())
	{
		m_DefaultPerms = ParsePermissions(sPerms);
		kDebug(2, "default permissions: {}", SerializePermissions(m_DefaultPerms));
	}

	// read per-directory permissions
	auto it = jConfig.find("directories");

	if (it != jConfig.end() && it->is_object())
	{
		for (auto jit = it->begin(); jit != it->end(); ++jit)
		{
			if (jit->is_string())
			{
				AddDirectoryPermission(jit.key(), ParsePermissions(kjson::GetStringRef(*it, jit.key())));
			}
		}
	}

} // SetDirectoryPermissions

//-----------------------------------------------------------------------------
void KWebServerPermissions::AddDirectoryPermission(KString sPath, Perms iPerms)
//-----------------------------------------------------------------------------
{
	kDebug(2, "path '{}': {}", sPath, SerializePermissions(iPerms));

	// ensure path starts with /
	if (!sPath.empty() && sPath.front() != '/')
	{
		sPath.insert(0, 1, '/');
	}

	// remove trailing slash for consistent matching (except for root "/")
	if (sPath.size() > 1 && sPath.back() == '/')
	{
		sPath.remove_suffix(1);
	}

	m_DirPerms.push_back({ std::move(sPath), iPerms });

	// keep sorted by path length descending (longest first)
	std::sort(m_DirPerms.begin(), m_DirPerms.end(),
		[](const DirEntry& a, const DirEntry& b)
		{
			return a.sPath.size() > b.sPath.size();
		}
	);

} // AddDirectoryPermission

//-----------------------------------------------------------------------------
bool KWebServerPermissions::AddUser(KStringView sUserEntry)
//-----------------------------------------------------------------------------
{
	auto Parts = sUserEntry.Split(":");

	if (Parts.size() < 3)
	{
		kDebug(1, "invalid user entry format, need at least user:password:/path - got: {}", sUserEntry);
		return false;
	}

	KString sUsername(Parts[0]);
	UserEntry Entry;
	Entry.sPassword = Parts[1];
	Entry.sPath     = Parts[2];

	// ensure path starts with /
	if (!Entry.sPath.empty() && Entry.sPath.front() != '/')
	{
		Entry.sPath.insert(0, 1, '/');
	}

	if (Parts.size() >= 4)
	{
		Entry.iPerms = ParsePermissions(Parts[3]);
	}
	else
	{
		// default: read|browse
		Entry.iPerms = Read | Browse;
	}

	kDebug(2, "user '{}', path '{}': {}", sUsername, Entry.sPath, SerializePermissions(Entry.iPerms));

	m_Users.push_back({ std::move(sUsername), std::move(Entry) });

	return true;

} // AddUser

//-----------------------------------------------------------------------------
bool KWebServerPermissions::LoadUsers(KStringViewZ sPathname)
//-----------------------------------------------------------------------------
{
	KInFile InFile(sPathname);

	if (!InFile.is_open())
	{
		kDebug(1, "cannot open users file: {}", sPathname);
		return false;
	}

	bool bSuccess = true;

	for (auto& sLine : InFile)
	{
		sLine.Trim();

		if (!sLine.empty() && !sLine.starts_with('#'))
		{
			if (!AddUser(sLine))
			{
				bSuccess = false;
			}
		}
	}

	return bSuccess;

} // LoadUsers

//-----------------------------------------------------------------------------
bool KWebServerPermissions::Authenticate(KStringView sUsername, KStringView sPassword) const
//-----------------------------------------------------------------------------
{
	for (const auto& Entry : m_Users)
	{
		if (Entry.first == sUsername && Entry.second.sPassword == sPassword)
		{
			return true;
		}
	}

	return false;

} // Authenticate

//-----------------------------------------------------------------------------
KWebServerPermissions::Perms KWebServerPermissions::LookupDirectory(KStringView sPath) const
//-----------------------------------------------------------------------------
{
	// longest-prefix-match: m_DirPerms is sorted by path length descending
	for (const auto& Entry : m_DirPerms)
	{
		if (sPath.starts_with(Entry.sPath))
		{
			// make sure this is a real prefix match at a path boundary
			if (sPath.size() == Entry.sPath.size() ||
			    Entry.sPath == "/"                  ||
			    sPath[Entry.sPath.size()] == '/')
			{
				return Entry.iPerms;
			}
		}
	}

	return m_DefaultPerms;

} // LookupDirectory

//-----------------------------------------------------------------------------
KWebServerPermissions::Perms KWebServerPermissions::LookupUser(KStringView sUsername, KStringView sPath) const
//-----------------------------------------------------------------------------
{
	Perms iBestPerms    = None;
	std::size_t iBestLen = 0;
	bool  bFound        = false;

	for (const auto& Entry : m_Users)
	{
		if (Entry.first != sUsername)
		{
			continue;
		}

		if (sPath.starts_with(Entry.second.sPath))
		{
			// path boundary check
			if (sPath.size() == Entry.second.sPath.size() ||
			    Entry.second.sPath == "/"                  ||
			    sPath[Entry.second.sPath.size()] == '/')
			{
				if (Entry.second.sPath.size() > iBestLen)
				{
					iBestLen  = Entry.second.sPath.size();
					iBestPerms = Entry.second.iPerms;
					bFound    = true;
				}
			}
		}
	}

	if (!bFound)
	{
		return None;
	}

	return iBestPerms;

} // LookupUser

//-----------------------------------------------------------------------------
KWebServerPermissions::Perms KWebServerPermissions::GetDirectoryPermissions(KStringView sPath) const
//-----------------------------------------------------------------------------
{
	return LookupDirectory(sPath);

} // GetDirectoryPermissions

//-----------------------------------------------------------------------------
KWebServerPermissions::Perms KWebServerPermissions::Resolve(KStringView sUsername, KStringView sPath) const
//-----------------------------------------------------------------------------
{
	Perms iDirPerms = LookupDirectory(sPath);

	if (m_Users.empty())
	{
		// no users configured - directory permissions are the effective permissions
		return iDirPerms;
	}

	if (sUsername.empty())
	{
		// users are configured but no user was authenticated
		return None;
	}

	Perms iUserPerms = LookupUser(sUsername, sPath);

	// effective = intersection
	return iDirPerms & iUserPerms;

} // Resolve

//-----------------------------------------------------------------------------
bool KWebServerPermissions::IsAllowed(KStringView sUsername, KHTTPMethod Method, KStringView sPath) const
//-----------------------------------------------------------------------------
{
	Perms iRequired  = MethodToPermission(Method);
	Perms iEffective = Resolve(sUsername, sPath);

	return (iEffective & iRequired) == iRequired;

} // IsAllowed

DEKAF2_NAMESPACE_END
