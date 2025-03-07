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

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
struct Generic
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
//----------
public:
//----------

	Generic() = default;
	Generic(std::string sFamily)
	: sFamily(KString(std::move(sFamily)))
	{
	}

	/// returns the family string
	const KString& GetFamily() const { return sFamily; }

//----------
protected:
//----------

	KString sFamily { "Other" };

}; // Generic

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
struct Device : public Generic
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	Device() = default;
	Device (Generic generic, std::string model, std::string brand)
	: Generic(std::move(generic))
	, sModel(KString(std::move(model)))
	, sBrand(KString(std::move(brand)))
	{
	}

	/// returns the model string
	const KString& GetModel () const { return sModel; }
	/// returns the brand string
	const KString& GetBrand () const { return sBrand; }
	/// returns true if family is "Spider"
	bool           IsSpider () const { return sFamily == "Spider"; }

//----------
protected:
//----------

	KString sModel;
	KString sBrand;

}; // Device

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
struct Agent : public Generic
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	Agent () = default;
	Agent (Generic generic, std::string major, std::string minor, std::string patch)
	: Generic(std::move(generic))
	, sMajor(KString(std::move(major)))
	, sMinor(KString(std::move(minor)))
	, sPatch(KString(std::move(patch)))
	{
	}

	/// returns family and version
	KString        Get        () const;
	/// returns the full version string
	KString        GetVersion () const;
	/// returns major version string
	const KString& GetVersionMajor () const { return sMajor; }
	/// returns minor version string
	const KString& GetVersionMinor () const { return sMinor; }
	/// returns patch level version string
	const KString& GetVersionPatch () const { return sPatch; }

//----------
protected:
//----------

	KString sMajor;
	KString sMinor;
	KString sPatch;

}; // Agent

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class UserAgent
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	UserAgent () = default;
	UserAgent (Device device, Agent os, Agent browser)
	: device(std::move(device))
	, os(std::move(os))
	, browser(std::move(browser))
	{
	}

	/// returns browser and OS string
	KString       Get        () const;
	/// returns the Device object
	const Device& GetDevice  () const { return device;  }
	/// returns the OS object
	const Agent&  GetOS      () const { return os;      }
	/// returns the Browser object
	const Agent&  GetBrowser () const { return browser; }
	/// returns true if family is "Spider"
	bool          IsSpider   () const { return GetDevice().IsSpider(); }

//----------
protected:
//----------

	Device device;
	Agent  os;
	Agent  browser;

}; // UserAgent

enum class DeviceType { Unknown = 0, Desktop, Mobile, Tablet };

/// The general user agent parser. Returns all information found, including device, OS, and browser.
UserAgent  Get           (const KString& sUserAgent);
/// Use GetDevice if you are only interested in device information
Device     GetDevice     (const KString& sUserAgent);
/// Use GetOS if you are only interested in OS information
Agent      GetOS         (const KString& sUserAgent);
/// Use GetBrowser if you are only interested in browser information
Agent      GetBrowser    (const KString& sUserAgent);
/// Use GetDeviceType if you are only interested in the device type (desktop/mobile/tablet) -
/// this is by far the fastest parser, and it does not need to load the regexes.yaml
DeviceType GetDeviceType (const KString& sUserAgent);

};

DEKAF2_NAMESPACE_END
