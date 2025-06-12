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

#include "kuseragent.h"

#if DEKAF2_HAS_USER_AGENT_PARSER

#include "kformat.h"
#include "kfilesystem.h"
#include "klog.h"
#include "UaParser.h"
#include <mutex>

DEKAF2_NAMESPACE_BEGIN

namespace {

std::unique_ptr<::uap_cpp::UserAgentParser> g_UserAgentParser;

//-----------------------------------------------------------------------------
bool InitUserAgentParser(const KString& sRegexFile = kReadLink(kFormat("/usr/local/{}/uap-core/regexes.yaml", DEKAF2_SHARED_DIRECTORY), true))
//-----------------------------------------------------------------------------
{
	static std::once_flag s_once;

	std::call_once(s_once, [&sRegexFile]
	{
		try
		{
			kDebug (3, "initializing from {}", sRegexFile);
			g_UserAgentParser = std::make_unique<::uap_cpp::UserAgentParser>(sRegexFile.str());
		}
		catch (const std::exception& ex)
		{
			kDebug(1, ex.what());
		}
	});

	return bool(g_UserAgentParser);

} // InitUserAgentParser

//-----------------------------------------------------------------------------
KHTTPUserAgent::Device MakeDevice(::uap_cpp::Device device)
//-----------------------------------------------------------------------------
{
	return KHTTPUserAgent::Device(KHTTPUserAgent::Generic(std::move(device.family)),
				  std::move(device.model),
				  std::move(device.brand));
}

//-----------------------------------------------------------------------------
KHTTPUserAgent::Agent MakeAgent(::uap_cpp::Agent agent)
//-----------------------------------------------------------------------------
{
	return KHTTPUserAgent::Agent(KHTTPUserAgent::Generic(std::move(agent.family)),
				 std::move(agent.major),
				 std::move(agent.minor),
				 std::move(agent.patch));
}

} // end of anonymous namespace

//-----------------------------------------------------------------------------
KString KHTTPUserAgent::Agent::GetVersion() const
//-----------------------------------------------------------------------------
{
	return kFormat("{}.{}.{}",
				   sMajor.empty() ? "0" : sMajor,
				   sMinor.empty() ? "0" : sMinor,
				   sPatch.empty() ? "0" : sPatch
				);
}

//-----------------------------------------------------------------------------
KString KHTTPUserAgent::Agent::GetString() const
//-----------------------------------------------------------------------------
{
	return kFormat("{} {}", sFamily, GetVersion());
}

//-----------------------------------------------------------------------------
KString KHTTPUserAgent::GetString ()
//-----------------------------------------------------------------------------
{
	return kFormat("{}/{}", GetBrowser().GetString(), GetOS().GetString());
}

//-----------------------------------------------------------------------------
const KHTTPUserAgent::Device& KHTTPUserAgent::GetDevice ()
//-----------------------------------------------------------------------------
{
	if ((m_Parsed & Parsed::Device) != Parsed::Device)
	{
		m_Parsed |= Parsed::Device;

		if (InitUserAgentParser())
		{
			m_Device = MakeDevice(g_UserAgentParser->parse_device(m_sUserAgent.str()));
		}
	}

	return m_Device;

} // GetDevice

//-----------------------------------------------------------------------------
const KHTTPUserAgent::Agent& KHTTPUserAgent::GetOS ()
//-----------------------------------------------------------------------------
{
	if ((m_Parsed & Parsed::OS) != Parsed::OS)
	{
		m_Parsed |= Parsed::OS;

		if (InitUserAgentParser())
		{
			m_OS = MakeAgent(g_UserAgentParser->parse_os(m_sUserAgent.str()));
		}
	}

	return m_OS;

} // GetOS

//-----------------------------------------------------------------------------
const KHTTPUserAgent::Agent& KHTTPUserAgent::GetBrowser ()
//-----------------------------------------------------------------------------
{
	if ((m_Parsed & Parsed::Browser) != Parsed::Browser)
	{
		m_Parsed |= Parsed::Browser;

		if (InitUserAgentParser())
		{
			m_Browser = MakeAgent(g_UserAgentParser->parse_browser(m_sUserAgent.str()));
		}
	}

	return m_Browser;

} // GetBrowser

//-----------------------------------------------------------------------------
KHTTPUserAgent::DeviceType KHTTPUserAgent::GetDeviceType ()
//-----------------------------------------------------------------------------
{
	switch (::uap_cpp::UserAgentParser::device_type(m_sUserAgent.str()))
	{
		case ::uap_cpp::DeviceType::kMobile:
			return DeviceType::Mobile;

		case ::uap_cpp::DeviceType::kTablet:
			return DeviceType::Tablet;

		case ::uap_cpp::DeviceType::kDesktop:
			return DeviceType::Desktop;

		case ::uap_cpp::DeviceType::kUnknown:
			return DeviceType::Unknown;
	}
	// gcc ..
	return DeviceType::Unknown;
}

//-----------------------------------------------------------------------------
bool KHTTPUserAgent::LoadRegexes(const KString& sRegexPathName)
//-----------------------------------------------------------------------------
{
	if (g_UserAgentParser)
	{
		kDebug(1, "user agent parser already initialized, cannot load file: {}", sRegexPathName);
		return false;
	}

	return InitUserAgentParser(sRegexPathName);

} // SetRegexPathname

DEKAF2_NAMESPACE_END

#endif
