/*
//
// DEKAF(tm): Lighter, Faster, Smarter (tm)
//
// Copyright (c) 2025, Ridgeware, Inc.
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
/// parse a http user agent header to obtain information like browser maker, version, OS, device category
class DEKAF2_PUBLIC KHTTPUserAgent
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	class DEKAF2_PUBLIC Generic
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
		DEKAF2_NODISCARD
		const KString& GetFamily() const { return sFamily; }

	//----------
	protected:
	//----------

		KString sFamily { "Other" };

	}; // Generic

	//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	class DEKAF2_PUBLIC Device : public Generic
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
		DEKAF2_NODISCARD
		const KString& GetModel () const { return sModel; }
		/// returns the brand string
		DEKAF2_NODISCARD
		const KString& GetBrand () const { return sBrand; }
		/// returns true if family is "Spider"
		DEKAF2_NODISCARD
		bool           IsSpider () const { return sFamily == "Spider"; }

	//----------
	protected:
	//----------

		KString sModel;
		KString sBrand;

	}; // Device

	//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	class DEKAF2_PUBLIC Agent : public Generic
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

		/// returns family and version as string
		DEKAF2_NODISCARD
		KString        GetString  () const;
		/// returns the full version string
		DEKAF2_NODISCARD
		KString        GetVersion () const;
		/// returns major version string
		DEKAF2_NODISCARD
		const KString& GetVersionMajor () const { return sMajor; }
		/// returns minor version string
		DEKAF2_NODISCARD
		const KString& GetVersionMinor () const { return sMinor; }
		/// returns patch level version string
		DEKAF2_NODISCARD
		const KString& GetVersionPatch () const { return sPatch; }

	//----------
	protected:
	//----------

		KString sMajor;
		KString sMinor;
		KString sPatch;

	}; // Agent

	enum class DeviceType { Unknown = 0, Desktop, Mobile, Tablet };

	/// default ctor
	KHTTPUserAgent() = default;
	/// construct around a KHTTP user agent string
	KHTTPUserAgent(KString sUserAgent) : m_sUserAgent(std::move(sUserAgent)) {}

	/// Returns browser and OS as string
	DEKAF2_NODISCARD
	KString       GetString();
	/// returns device information
	DEKAF2_NODISCARD
	const Device& GetDevice     ();
	/// returns OS information
	DEKAF2_NODISCARD
	const Agent&  GetOS         ();
	/// returns browser information
	DEKAF2_NODISCARD
	const Agent&  GetBrowser    ();
	/// returns device type (desktop/mobile/tablet) -
	/// this is by far the fastest parser, and it does not need to load the regexes.yaml
	DEKAF2_NODISCARD
	DeviceType    GetDeviceType ();
	/// returns true if family is "Spider"
	DEKAF2_NODISCARD
	bool          IsSpider      () { return GetDevice().IsSpider(); }

	/// call this static function BEFORE any use of this class to load a regex file other than
	/// the one at the default install location (/usr/local/share/dekaf2/..).
	/// @param sRegexPathName full path name for a yaml file with regex definitions for the parser
	/// @return false if user agent parsing was already initialized at time of call, or other initialization error, true otherwise
	static bool LoadRegexes(const KString& sRegexPathName);

	// purely internal enum class - needs be exposed for enum flag operations
	enum class Parsed
	{
		None    = 0,
		Device  = 1 << 1,
		OS      = 1 << 2,
		Browser = 1 << 3
	};

//----------
private:
//----------

	KString m_sUserAgent;
	Device  m_Device;
	Agent   m_OS;
	Agent   m_Browser;
	Parsed  m_Parsed { Parsed::None };

}; // KHTTPUserAgent

DEKAF2_ENUM_IS_FLAG(KHTTPUserAgent::Parsed)

DEKAF2_NAMESPACE_END
