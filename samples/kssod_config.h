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

// kssod_config.h — the operator's settings file
//
// Some settings belong to whoever runs the server, not to the administrators in
// the web UI. The mail relay is one: an administrator who could point it at a
// server of their own would read every password recovery mail, and could take
// over any account that way. These settings come from a JSON file that only the
// operator edits; the admin UI shows them but cannot change them. The file is
// read at startup, so a change needs a restart. By default it is config.json in
// the config directory of kssod, see KConfig.
//
//   {
//       "smtp": {
//           "url":       "smtps://mail.example.com:465",
//           "user":      "kssod",
//           "password":  "the relay password",
//           "from":      "kssod@example.com",
//           "from_name": "Example SSO"
//       }
//   }
//
// Every key is optional, except that an "smtp" object needs "url" and "from".
// An unknown key or a value of the wrong type is an error, so that a typo cannot
// silently switch a setting off. KConfig reads and writes the file: it accepts
// UTF-8, and UTF-16 or UTF-32 with a byte order mark, and it refuses anything
// after the JSON value.

#pragma once

#include "kssod_store.h"
#include <dekaf2/core/strings/kstring.h>
#include <dekaf2/core/strings/kstringview.h>

using namespace dekaf2;

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// the settings of the operator, see the file comment for the format
struct KSSOdOperatorConfig
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	/// the outgoing mail relay, empty if the file has no "smtp" object
	KSSOdSettingsStore::Smtp Smtp;

	/// the default settings file, config.json in the config directory
	static KString DefaultPath();

	/// read and check the file
	/// @returns false and fills sError if the file cannot be read or is invalid
	bool Load(KStringViewZ sFileName, KString& sError);

	/// write the file, readable and writable by its owner only
	/// @returns false and fills sError if the file cannot be written
	bool Save(KStringViewZ sFileName, KString& sError) const;

}; // KSSOdOperatorConfig
