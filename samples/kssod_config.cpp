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

// kssod_config.cpp — see kssod_config.h

#include "kssod_config.h"
#include <dekaf2/data/json/kconfig.h>
#include <dekaf2/core/format/kformat.h>

namespace {

//-----------------------------------------------------------------------------
/// copy the string members of jObject into the targets named in Keys. Any other
/// member, and any member that is not a string, is an error.
bool ReadStrings(const KJSON& jObject, KStringView sPath,
                 std::initializer_list<std::pair<KStringView, KString*>> Keys, KString& sError)
//-----------------------------------------------------------------------------
{
	for (auto it = jObject.begin(); it != jObject.end(); ++it)
	{
		KString* pTarget = nullptr;

		for (const auto& Key : Keys)
		{
			if (Key.first == it.key())
			{
				pTarget = Key.second;
				break;
			}
		}

		if (!pTarget)
		{
			sError = kFormat("unknown setting '{}.{}'", sPath, it.key());
			return false;
		}

		if (!it.value().is_string())
		{
			sError = kFormat("setting '{}.{}' has to be a string", sPath, it.key());
			return false;
		}

		*pTarget = it.value().String();
	}

	return true;

} // ReadStrings

} // anonymous namespace

//-----------------------------------------------------------------------------
KString KSSOdOperatorConfig::DefaultPath()
//-----------------------------------------------------------------------------
{
	return KConfig::DefaultPath();

} // DefaultPath

//-----------------------------------------------------------------------------
bool KSSOdOperatorConfig::Load(KStringViewZ sFileName, KString& sError)
//-----------------------------------------------------------------------------
{
	*this = KSSOdOperatorConfig{};

	KConfig Config(sFileName);

	if (!Config.Loaded())
	{
		sError = Config.HasError() ? KString(Config.Error())
		                           : kFormat("the settings file {} does not exist", sFileName);
		return false;
	}

	const auto& jConfig = Config.Get();

	if (!jConfig.is_object())
	{
		sError = kFormat("{}: the settings have to be a JSON object", sFileName);
		return false;
	}

	for (auto it = jConfig.begin(); it != jConfig.end(); ++it)
	{
		if (it.key() == "smtp")
		{
			if (!it.value().is_object())
			{
				sError = kFormat("{}: setting 'smtp' has to be an object", sFileName);
				return false;
			}

			if (!ReadStrings(it.value(), "smtp", {
				{ "url",       &Smtp.sURL      },
				{ "user",      &Smtp.sUser     },
				{ "password",  &Smtp.sPass     },
				{ "from",      &Smtp.sFrom     },
				{ "from_name", &Smtp.sFromName }
			}, sError))
			{
				sError = kFormat("{}: {}", sFileName, sError);
				return false;
			}

			if (!Smtp.IsConfigured())
			{
				sError = kFormat("{}: setting 'smtp' needs 'url' and 'from'", sFileName);
				return false;
			}
		}
		else
		{
			sError = kFormat("{}: unknown setting '{}'", sFileName, it.key());
			return false;
		}
	}

	return true;

} // Load

//-----------------------------------------------------------------------------
bool KSSOdOperatorConfig::Save(KStringViewZ sFileName, KString& sError) const
//-----------------------------------------------------------------------------
{
	KConfig Config(sFileName);

	// the file is written from these settings alone, not from what it held before
	Config.Get() = KJSON::object();

	if (Smtp.IsConfigured())
	{
		auto& jSmtp = Config["smtp"];

		jSmtp["url"]  = Smtp.sURL;
		if (!Smtp.sUser.empty())     jSmtp["user"]      = Smtp.sUser;
		if (!Smtp.sPass.empty())     jSmtp["password"]  = Smtp.sPass;
		jSmtp["from"] = Smtp.sFrom;
		if (!Smtp.sFromName.empty()) jSmtp["from_name"] = Smtp.sFromName;
	}

	// the file holds the relay password
	if (!Config.Save({}, 0600))
	{
		sError = Config.Error();
		return false;
	}

	return true;

} // Save
