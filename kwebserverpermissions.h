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

#pragma once

/// @file kwebserverpermissions.h
/// per-directory and per-user permission control for web/file servers

#include "kdefinitions.h"
#include "kstring.h"
#include "kstringview.h"
#include "khttp_method.h"
#include "kjson.h"
#include <vector>

DEKAF2_NAMESPACE_BEGIN

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Permission flags for web server access control. Can be combined with bitwise OR.
/// Directory permissions define the structural maximum for a path.
/// User permissions define what an authenticated user may do.
/// The effective permission is the intersection of both.
class DEKAF2_PUBLIC KWebServerPermissions
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	using Perms = uint16_t;

	enum Permission : Perms
	{
		None      = 0,
		Read      = 1 << 0,  // GET, HEAD
		Write     = 1 << 1,  // PUT, POST (file upload)
		Erase     = 1 << 2,  // DELETE
		Autoindex = 1 << 3,  // generate directory index when no index.html exists
		Browse    = 1 << 4,  // allow directory access (list contents)
		All       = Read | Write | Erase | Autoindex | Browse,
	};

	/// default constructor - default permission is READ | BROWSE
	KWebServerPermissions() = default;

	/// construct from a JSON configuration object.
	/// reads "permissions" (default permissions string) and "directories" (object with path:permissions pairs)
	/// @param jConfig the JSON configuration, e.g. {"permissions":"read|browse","directories":{"/uploads":"read|write|browse|erase"}}
	KWebServerPermissions(const KJSON& jConfig);

	/// set directory permissions from a JSON configuration object
	/// @param jConfig JSON with "permissions" and/or "directories" properties
	void SetDirectoryPermissions(const KJSON& jConfig);

	/// add a single directory permission entry
	/// @param sPath the directory path (prefix match)
	/// @param iPerms the permission flags
	void AddDirectoryPermission(KString sPath, Perms iPerms);

	/// set the default permissions that apply when no directory-specific entry matches
	/// @param iPerms the permission flags
	void SetDefaultPermissions(Perms iPerms) { m_DefaultPerms = iPerms; }

	/// get the default permissions
	DEKAF2_NODISCARD
	Perms GetDefaultPermissions() const { return m_DefaultPerms; }

	/// add a user entry from a configuration string.
	/// format: user:password:/path:permissions
	/// where permissions is a | separated list of: read, write, erase, autoindex, browse, all, none
	/// @param sUserEntry the user configuration string
	/// @return true if the entry was parsed successfully
	bool AddUser(KStringView sUserEntry);

	/// load user entries from a file, one entry per line.
	/// empty lines and lines starting with # are skipped.
	/// @param sFilePath path to the users file
	/// @return true if the file was loaded successfully
	bool LoadUsers(KStringViewZ sFilePath);

	/// returns true if any users are configured (meaning authentication is required)
	DEKAF2_NODISCARD
	bool HasUsers() const { return !m_Users.empty(); }

	/// authenticate a user with password
	/// @param sUsername the username
	/// @param sPassword the password
	/// @return true if the user exists and the password matches for at least one entry
	DEKAF2_NODISCARD
	bool Authenticate(KStringView sUsername, KStringView sPassword) const;

	/// resolve effective permissions for a user on a given path.
	/// if no users are configured, returns the directory permissions.
	/// if users are configured, returns directory permissions ∩ user permissions.
	/// @param sUsername the authenticated username (empty if no auth)
	/// @param sPath the request path
	/// @return the effective permission flags
	DEKAF2_NODISCARD
	Perms Resolve(KStringView sUsername, KStringView sPath) const;

	/// check if a specific HTTP method is allowed for a user on a path
	/// @param sUsername the authenticated username (empty if no auth)
	/// @param Method the HTTP method
	/// @param sPath the request path
	/// @return true if the method is allowed
	DEKAF2_NODISCARD
	bool IsAllowed(KStringView sUsername, KHTTPMethod Method, KStringView sPath) const;

	/// get the directory-level permissions for a path (ignoring user permissions)
	/// @param sPath the request path
	/// @return the directory permission flags
	DEKAF2_NODISCARD
	Perms GetDirectoryPermissions(KStringView sPath) const;

	/// convert an HTTP method to the required permission flag
	/// @param Method the HTTP method
	/// @return the required permission flag(s)
	DEKAF2_NODISCARD
	static Perms MethodToPermission(KHTTPMethod Method);

	/// parse a permissions string like "read|write|browse" into flags
	/// @param sPermissions the permissions string
	/// @return the parsed permission flags
	DEKAF2_NODISCARD
	static Perms ParsePermissions(KStringView sPermissions);

	/// serialize permission flags to a human-readable string
	/// @param iPerms the permission flags
	/// @return a string like "read|write|browse"
	DEKAF2_NODISCARD
	static KString SerializePermissions(Perms iPerms);

//------
private:
//------

	struct UserEntry
	{
		KString sPassword;
		KString sPath;
		Perms   iPerms { Read | Browse };
	};

	struct DirEntry
	{
		KString sPath;
		Perms   iPerms { Read | Browse };
	};

	/// longest-prefix-match lookup in directory permissions
	Perms LookupDirectory(KStringView sPath) const;

	/// longest-prefix-match lookup in user permissions.
	/// returns NONE if user not found or no path matches.
	Perms LookupUser(KStringView sUsername, KStringView sPath) const;

	// sorted by path length descending (longest first) for prefix matching
	std::vector<DirEntry> m_DirPerms;

	// user entries: key = username, one user can have multiple path entries
	std::vector<std::pair<KString, UserEntry>> m_Users;

	Perms m_DefaultPerms { Read | Browse };

}; // KWebServerPermissions

DEKAF2_NAMESPACE_END
