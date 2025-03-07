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
#include "kformat.h"
#include "UaParser.h"
#include <mutex>

DEKAF2_NAMESPACE_BEGIN

namespace KHTTPUserAgent {

namespace {

std::unique_ptr<::uap_cpp::UserAgentParser> g_UserAgentParser;

//-----------------------------------------------------------------------------
bool InitUserAgentParser()
//-----------------------------------------------------------------------------
{
	static std::once_flag s_once;

	std::call_once(s_once, []
	{
		try
		{
			auto sRegexFile = kReadLink(kFormat("/usr/local/{}/uap-core/regexes.yaml", DEKAF2_SHARED_DIRECTORY), true);
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
Device MakeDevice(::uap_cpp::Device device)
//-----------------------------------------------------------------------------
{
	return Device(Generic(std::move(device.family)),
				  std::move(device.model),
				  std::move(device.brand));
}

//-----------------------------------------------------------------------------
Agent MakeAgent(::uap_cpp::Agent agent)
//-----------------------------------------------------------------------------
{
	return Agent(Generic(std::move(agent.family)),
				 std::move(agent.major),
				 std::move(agent.minor),
				 std::move(agent.patch));
}

//-----------------------------------------------------------------------------
UserAgent MakeUserAgent(::uap_cpp::UserAgent ua)
//-----------------------------------------------------------------------------
{
	return UserAgent(MakeDevice(std::move(ua.device)),
					 MakeAgent(std::move(ua.os)),
					 MakeAgent(std::move(ua.browser)));
}

} // end of anonymous namespace

//-----------------------------------------------------------------------------
KString Agent::GetVersion() const
//-----------------------------------------------------------------------------
{
	return kFormat("{}.{}.{}",
				   sMajor.empty() ? "0" : sMajor,
				   sMinor.empty() ? "0" : sMinor,
				   sPatch.empty() ? "0" : sPatch
				);
}

//-----------------------------------------------------------------------------
KString Agent::Get() const
//-----------------------------------------------------------------------------
{
	return kFormat("{} {}", sFamily, GetVersion());
}

//-----------------------------------------------------------------------------
KString UserAgent::Get () const
//-----------------------------------------------------------------------------
{
	return kFormat("{}/{}", Browser.Get(), OS.Get());
}

//-----------------------------------------------------------------------------
UserAgent Get (const KString& sUserAgent)
//-----------------------------------------------------------------------------
{
	if (!InitUserAgentParser())
	{
		return {};
	}

	return MakeUserAgent(g_UserAgentParser->parse(sUserAgent.str()));

} // Get

//-----------------------------------------------------------------------------
Device GetDevice (const KString& sUserAgent)
//-----------------------------------------------------------------------------
{
	if (!InitUserAgentParser())
	{
		return {};
	}

	return MakeDevice(g_UserAgentParser->parse_device(sUserAgent.str()));

} // GetDevice

//-----------------------------------------------------------------------------
Agent GetOS (const KString& sUserAgent)
//-----------------------------------------------------------------------------
{
	if (!InitUserAgentParser())
	{
		return {};
	}

	return MakeAgent(g_UserAgentParser->parse_os(sUserAgent.str()));

} // GetOS

//-----------------------------------------------------------------------------
Agent GetBrowser (const KString& sUserAgent)
//-----------------------------------------------------------------------------
{
	if (!InitUserAgentParser())
	{
		return {};
	}

	return MakeAgent(g_UserAgentParser->parse_browser(sUserAgent.str()));

} // GetBrowser

//-----------------------------------------------------------------------------
DeviceType GetDeviceType (const KString& sUserAgent)
//-----------------------------------------------------------------------------
{
	switch (::uap_cpp::UserAgentParser::device_type(sUserAgent.str()))
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

} // end of namespace KHTTPUserAgent

DEKAF2_NAMESPACE_END
