/*
//
// DEKAF(tm): Lighter, Faster, Smarter (tm)
//
// Copyright (c) 2017, Ridgeware, Inc.
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

#include "kstringview.h"
#include "kstring.h"

DEKAF2_NAMESPACE_BEGIN

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
namespace KHTTPUserAgent
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

struct Generic
{
	Generic() = default;
	Generic(std::string sFamily)
	: sFamily(KString(std::move(sFamily)))
	{
	}

	KString sFamily { "Other" };
};

struct Device : Generic
{
	Device() = default;
	Device (Generic generic, std::string model, std::string brand)
	: Generic(std::move(generic))
	, sModel(KString(std::move(model)))
	, sBrand(KString(std::move(brand)))
	{
	}

	KString sModel;
	KString sBrand;
};

struct Agent : Generic
{
	Agent () = default;
	Agent (Generic generic, std::string major, std::string minor, std::string patch)
	: Generic(std::move(generic))
	, sMajor(KString(std::move(major)))
	, sMinor(KString(std::move(minor)))
	, sPatch(KString(std::move(patch)))
	{
	}

	KString Get        () const;
	KString GetVersion () const;

	KString sMajor;
	KString sMinor;
	KString sPatch;
};

struct UserAgent
{
	UserAgent () = default;
	UserAgent (Device Device, Agent OS, Agent Browser)
	: Device(std::move(Device))
	, OS(std::move(OS))
	, Browser(std::move(Browser))
	{
	}

	KString Get      () const;
	bool    IsSpider () const { return Device.sFamily == "Spider"; }
	bool    IsBot    () const { return IsSpider();                 }

	Device Device;
	Agent  OS;
	Agent  Browser;
};

enum class DeviceType { Unknown = 0, Desktop, Mobile, Tablet };

UserAgent  Get           (const KString& sUserAgent);
Device     GetDevice     (const KString& sUserAgent);
Agent      GetOS         (const KString& sUserAgent);
Agent      GetBrowser    (const KString& sUserAgent);
DeviceType GetDeviceType (const KString& sUserAgent);

};

DEKAF2_NAMESPACE_END
