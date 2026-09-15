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


#include <dekaf2/util/i18n/kstringcatalog.h>
#include <dekaf2/core/format/kformat.h>
#include <dekaf2/core/logging/klog.h>
#include <dekaf2/core/strings/kstringutils.h>
#include <dekaf2/io/readwrite/kreader.h>
#include <dekaf2/system/filesystem/kfilesystem.h>
#include <algorithm>

// the CLDR data, see from/cldr/update.sh
#include "../../../from/cldr/kcldr_likelysubtags.h"

DEKAF2_NAMESPACE_BEGIN

namespace {

//-----------------------------------------------------------------------------
// a subtag of letters or digits, as BCP 47 allows
bool IsSubtag(KStringView sPart)
//-----------------------------------------------------------------------------
{
	if (sPart.empty() || sPart.size() > 8)
	{
		return false;
	}

	for (auto ch : sPart)
	{
		if (!KASCII::kIsAlNum(ch))
		{
			return false;
		}
	}

	return true;

} // IsSubtag

struct Subtags
{
	KStringView sLanguage;
	KStringView sScript;
	KStringView sRegion;
};

//-----------------------------------------------------------------------------
// the language, script and region of a canonical tag - variants and extensions
// are left out
Subtags Split(KStringView sTag)
//-----------------------------------------------------------------------------
{
	Subtags Parts;

	for (auto sPart : sTag.Split("-"))
	{
		if (Parts.sLanguage.empty())
		{
			Parts.sLanguage = sPart;
		}
		else if (sPart.size() == 4 && KASCII::kIsAlpha(sPart.front()))
		{
			Parts.sScript = sPart;
		}
		else if ((sPart.size() == 2 && KASCII::kIsAlpha(sPart.front())) || (sPart.size() == 3 && KASCII::kIsDigit(sPart.front())))
		{
			Parts.sRegion = sPart;
		}
	}

	return Parts;

} // Split

//-----------------------------------------------------------------------------
// the script of a tag: the one in the tag, else the one CLDR expects for its
// language and region
KStringView ScriptOf(const Subtags& Parts)
//-----------------------------------------------------------------------------
{
	return Parts.sScript.empty() ? KStringCatalog::LikelyScript(Parts.sLanguage, Parts.sRegion) : Parts.sScript;

} // ScriptOf

} // end of anonymous namespace

//-----------------------------------------------------------------------------
KString KStringCatalog::CanonicalTag(KStringView sLanguage)
//-----------------------------------------------------------------------------
{
	KString sTag(sLanguage);
	sTag.Trim();
	sTag.Replace('_', '-');

	KString sCanonical;
	bool    bFirst = true;

	for (auto sPart : sTag.Split("-"))
	{
		if (!IsSubtag(sPart))
		{
			return {};
		}

		KString sSub(sPart);

		if (bFirst)
		{
			// the language: letters only, lowercase
			for (auto ch : sSub)
			{
				if (!KASCII::kIsAlpha(ch))
				{
					return {};
				}
			}

			sSub.MakeLowerASCII();
			bFirst = false;
		}
		else if (sSub.size() == 4 && KASCII::kIsAlpha(sSub.front()))
		{
			// a script: "Hant"
			sSub.MakeLowerASCII();
			sSub[0] = KASCII::kToUpper(sSub[0]);
		}
		else if (sSub.size() == 2 && KASCII::kIsAlpha(sSub.front()))
		{
			// a region: "AT"
			sSub.MakeUpperASCII();
		}

		if (!sCanonical.empty())
		{
			sCanonical += '-';
		}

		sCanonical += sSub;
	}

	return sCanonical;

} // CanonicalTag

//-----------------------------------------------------------------------------
KStringView KStringCatalog::LanguageOf(KStringView sLanguage)
//-----------------------------------------------------------------------------
{
	auto iDash = sLanguage.find_first_of("-_");
	return iDash == KStringView::npos ? sLanguage : sLanguage.substr(0, iDash);

} // LanguageOf

//-----------------------------------------------------------------------------
KStringView KStringCatalog::LikelyScript(KStringView sLanguage, KStringView sRegion)
//-----------------------------------------------------------------------------
{
	// the CLDR rows keyed by "zh" and "zh-TW", built once
	static const KUnorderedMap<KString, KStringView> s_Scripts = []()
	{
		KUnorderedMap<KString, KStringView> Scripts;

		for (const auto& Row : kCLDRLikelyScripts)
		{
			KString sKey(Row.sLanguage);

			if (*Row.sRegion)
			{
				sKey += '-';
				sKey += Row.sRegion;
			}

			Scripts.emplace(std::move(sKey), Row.sScript);
		}

		return Scripts;
	}();

	KString sKey(sLanguage);
	sKey.MakeLowerASCII();

	if (!sRegion.empty())
	{
		KString sUpper(sRegion);
		sUpper.MakeUpperASCII();

		auto it = s_Scripts.find(kFormat("{}-{}", sKey, sUpper));

		if (it != s_Scripts.end())
		{
			return it->second;
		}
	}

	auto it = s_Scripts.find(sKey);
	return it != s_Scripts.end() ? it->second : KStringView{};

} // LikelyScript

//-----------------------------------------------------------------------------
std::vector<KString> KStringCatalog::Fallbacks(KStringView sLanguage)
//-----------------------------------------------------------------------------
{
	std::vector<KString> Tags;

	auto sTag = CanonicalTag(sLanguage);

	if (sTag.empty())
	{
		return Tags;
	}

	auto Add = [&Tags](KString sCandidate)
	{
		if (std::find(Tags.begin(), Tags.end(), sCandidate) == Tags.end())
		{
			Tags.push_back(std::move(sCandidate));
		}
	};

	auto Parts   = Split(sTag);
	auto sLikely = LikelyScript(Parts.sLanguage, Parts.sRegion);
	auto sScript = Parts.sScript.empty() ? sLikely : Parts.sScript;

	Add(sTag);

	if (!Parts.sRegion.empty())
	{
		if (!sScript.empty())
		{
			// the tag with its script: "zh-Hant-TW" for "zh-TW"
			Add(kFormat("{}-{}-{}", Parts.sLanguage, sScript, Parts.sRegion));
		}

		if (sLikely.empty() || sScript == sLikely)
		{
			// the tag without the script the region implies anyway: "zh-TW" for
			// "zh-Hant-TW". A script that differs from the region's usual one stays
			// in the tag, "zh-Hans-TW" is not "zh-TW"
			Add(kFormat("{}-{}", Parts.sLanguage, Parts.sRegion));
		}
	}

	if (!sScript.empty())
	{
		Add(kFormat("{}-{}", Parts.sLanguage, sScript));
	}

	Add(KString(Parts.sLanguage));

	return Tags;

} // Fallbacks

//-----------------------------------------------------------------------------
KStringCatalog::KStringCatalog(KStringView sDefaultLanguage)
//-----------------------------------------------------------------------------
: m_sDefaultLanguage(CanonicalTag(sDefaultLanguage))
{
	if (m_sDefaultLanguage.empty())
	{
		kDebug(1, "not a language tag: '{}', using en", sDefaultLanguage);
		m_sDefaultLanguage = "en";
	}

} // ctor

//-----------------------------------------------------------------------------
bool KStringCatalog::AddLanguage(KStringView sLanguage, const KJSON& jStrings)
//-----------------------------------------------------------------------------
{
	auto sTag = CanonicalTag(sLanguage);

	if (sTag.empty())
	{
		kDebug(1, "not a language tag: '{}'", sLanguage);
		return false;
	}

	if (!jStrings.is_object())
	{
		kDebug(1, "the strings of {} are not a JSON object", sTag);
		return false;
	}

	auto& Messages = m_Languages[sTag];
	bool  bAllGood = true;

	for (auto it = jStrings.begin(); it != jStrings.end(); ++it)
	{
		const auto& sID    = it.key();
		const auto& jValue = it.value();

		if (!jValue.is_string())
		{
			// a catalog holds messages only - no objects between them
			kDebug(1, "{}: the value of '{}' is not a string, skipped", sTag, sID);
			bAllGood = false;
			continue;
		}

		KMessageFormat Message(jValue.String(), sTag);

		if (Message.HasError())
		{
			kDebug(1, "{}: '{}' does not parse: {}", sTag, sID, Message.Error());
			bAllGood = false;
		}

		// kept even when broken, so that Check() can report it and Get() still shows the text
		Messages[KString(sID)] = std::move(Message);
	}

	return bAllGood;

} // AddLanguage

//-----------------------------------------------------------------------------
bool KStringCatalog::AddLanguage(KStringView sLanguage, KStringView sJSON)
//-----------------------------------------------------------------------------
{
	KJSON   jStrings;
	KString sError;

	if (!kjson::Parse(jStrings, sJSON, sError))
	{
		kDebug(1, "{}: {}", sLanguage, sError);
		return false;
	}

	return AddLanguage(sLanguage, jStrings);

} // AddLanguage

//-----------------------------------------------------------------------------
void KStringCatalog::Merge(const KStringCatalog& Other)
//-----------------------------------------------------------------------------
{
	for (const auto& Language : Other.m_Languages)
	{
		auto& Messages = m_Languages[Language.first];

		for (const auto& Message : Language.second)
		{
			Messages[Message.first] = Message.second;
		}
	}

} // Merge

//-----------------------------------------------------------------------------
bool KStringCatalog::LoadDirectory(KStringViewZ sDirectory)
//-----------------------------------------------------------------------------
{
	KDirectory Dir(sDirectory, KFileType::FILE);
	Dir.Sort();

	bool bLoaded = false;

	for (const auto& File : Dir)
	{
		auto sName = File.Filename();

		if (!sName.ends_with(".json"))
		{
			continue;
		}

		auto sTag = CanonicalTag(sName.substr(0, sName.size() - 5));

		if (sTag.empty() || sName.contains("locales"))
		{
			// not a language file - e.g. a manifest
			kDebug(2, "skipping {}, not a language file", File.Path());
			continue;
		}

		KString sJSON;
		{
			KInFile In(File.Path());

			if (!In.is_open() || !In.ReadRemaining(sJSON))
			{
				kDebug(1, "cannot read {}", File.Path());
				continue;
			}
		}

		KJSON   jStrings;
		KString sError;

		if (!kjson::Parse(jStrings, sJSON, sError))
		{
			kDebug(1, "{}: {}", File.Path(), sError);
			continue;
		}

		AddLanguage(sTag, jStrings);
		bLoaded = true;
	}

	if (!bLoaded)
	{
		kDebug(1, "no language files in {}", sDirectory);
	}

	return bLoaded;

} // LoadDirectory

//-----------------------------------------------------------------------------
std::vector<KString> KStringCatalog::GetLanguages() const
//-----------------------------------------------------------------------------
{
	std::vector<KString> Languages;
	Languages.reserve(m_Languages.size());

	for (const auto& Language : m_Languages)
	{
		Languages.push_back(Language.first);
	}

	std::sort(Languages.begin(), Languages.end());
	return Languages;

} // GetLanguages

//-----------------------------------------------------------------------------
bool KStringCatalog::HasLanguage(KStringView sLanguage) const
//-----------------------------------------------------------------------------
{
	for (const auto& sTag : Fallbacks(sLanguage))
	{
		if (m_Languages.contains(sTag))
		{
			return true;
		}
	}

	return false;

} // HasLanguage

//-----------------------------------------------------------------------------
KString KStringCatalog::Negotiate(KStringView sAcceptLanguage) const
//-----------------------------------------------------------------------------
{
	// the requested tags with their weights, "de-AT,de;q=0.9,en;q=0.8"
	struct Wanted { KString sTag; double dQuality; };
	std::vector<Wanted> Wants;

	for (auto sEntry : sAcceptLanguage.Split(","))
	{
		auto   Parts    = sEntry.Split(";");
		double dQuality = 1.0;

		for (std::size_t i = 1; i < Parts.size(); ++i)
		{
			auto sParm = Parts[i];
			sParm.Trim();

			if (sParm.starts_with("q="))
			{
				dQuality = sParm.substr(2).Double();
			}
		}

		if (Parts.empty() || dQuality <= 0)
		{
			continue;
		}

		auto sTag = Parts[0];
		sTag.Trim();

		if (sTag == "*")
		{
			Wants.push_back({ m_sDefaultLanguage, dQuality });
		}
		else if (auto sCanonical = CanonicalTag(sTag); !sCanonical.empty())
		{
			Wants.push_back({ std::move(sCanonical), dQuality });
		}
	}

	std::stable_sort(Wants.begin(), Wants.end(), [](const Wanted& a, const Wanted& b) { return a.dQuality > b.dQuality; });

	auto Languages = GetLanguages();

	for (const auto& Want : Wants)
	{
		// the tag as requested and its fallbacks: "zh-TW", "zh-Hant-TW", "zh-Hant", "zh"
		for (const auto& sTag : Fallbacks(Want.sTag))
		{
			if (m_Languages.contains(sTag))
			{
				return sTag;
			}
		}

		// else a regional variant of the language serves the request: "fr-FR" for
		// "fr". Of several variants the one in the requested script: "zh-TW" for
		// "zh-Hant" when "zh-CN" and "zh-TW" are there
		auto    Parts   = Split(Want.sTag);
		auto    sScript = ScriptOf(Parts);
		KString sVariant;

		for (const auto& sHave : Languages)
		{
			if (LanguageOf(sHave) != Parts.sLanguage)
			{
				continue;
			}

			if (ScriptOf(Split(sHave)) == sScript)
			{
				return sHave;
			}

			if (sVariant.empty())
			{
				sVariant = sHave;
			}
		}

		if (!sVariant.empty())
		{
			return sVariant;
		}
	}

	return m_sDefaultLanguage;

} // Negotiate

//-----------------------------------------------------------------------------
std::vector<const KStringCatalog::Messages*> KStringCatalog::Chain(KStringView sLanguage) const
//-----------------------------------------------------------------------------
{
	std::vector<const Messages*> Result;

	auto Add = [&](KStringView sTag)
	{
		auto it = m_Languages.find(sTag);

		if (it != m_Languages.end() && std::find(Result.begin(), Result.end(), &it->second) == Result.end())
		{
			Result.push_back(&it->second);
		}
	};

	for (const auto& sTag : Fallbacks(sLanguage))
	{
		Add(sTag);
	}

	Add(m_sDefaultLanguage);
	return Result;

} // Chain

//-----------------------------------------------------------------------------
const KMessageFormat* KStringCatalog::Find(KStringView sLanguage, KStringView sID) const
//-----------------------------------------------------------------------------
{
	for (auto* Messages : Chain(sLanguage))
	{
		auto it = Messages->find(sID);

		if (it != Messages->end())
		{
			return &it->second;
		}
	}

	ReportMissing(sLanguage, sID);
	return nullptr;

} // Find

//-----------------------------------------------------------------------------
void KStringCatalog::ReportMissing(KStringView sLanguage, KStringView sID) const
//-----------------------------------------------------------------------------
{
	// once per identifier, the log is no place for every request
	std::lock_guard<std::mutex> Lock(m_ReportMutex);

	if (m_Reported.insert(KString(sID)).second)
	{
		kDebug(1, "no message '{}' in any language for {}", sID, sLanguage);
	}

} // ReportMissing

//-----------------------------------------------------------------------------
KStringView KStringCatalog::Get(KStringView sLanguage, KStringView sID) const
//-----------------------------------------------------------------------------
{
	auto* Message = Find(sLanguage, sID);
	return Message ? KStringView(Message->GetMessage()) : sID;

} // Get

//-----------------------------------------------------------------------------
KString KStringCatalog::Format(KStringView sLanguage, KStringView sID, const KJSON& jArgs) const
//-----------------------------------------------------------------------------
{
	auto* Message = Find(sLanguage, sID);

	if (!Message)
	{
		return sID;
	}

	if (Message->HasError())
	{
		// a message that did not parse shows as it is
		return Message->GetMessage();
	}

	return Message->Format(jArgs);

} // Format

//-----------------------------------------------------------------------------
KJSON KStringCatalog::GetNamespace(KStringView sLanguage, KStringView sPrefix) const
//-----------------------------------------------------------------------------
{
	KString sDotted(sPrefix);

	if (!sDotted.empty() && !sDotted.ends_with('.'))
	{
		sDotted += '.';
	}

	// the least specific language first, so that the specific ones override
	auto  Languages = Chain(sLanguage);
	KJSON jResult   = KJSON::object();

	for (auto it = Languages.rbegin(); it != Languages.rend(); ++it)
	{
		for (const auto& Message : **it)
		{
			if (Message.first.starts_with(sDotted))
			{
				jResult[KString(KStringView(Message.first).substr(sDotted.size()))] = Message.second.GetMessage();
			}
		}
	}

	return jResult;

} // GetNamespace

//-----------------------------------------------------------------------------
KStringCatalog::Language KStringCatalog::For(KStringView sLanguage) const
//-----------------------------------------------------------------------------
{
	return Language(*this, sLanguage);

} // For

//-----------------------------------------------------------------------------
KJSON KStringCatalog::Check() const
//-----------------------------------------------------------------------------
{
	KJSON jResult = KJSON::object();

	auto Default = m_Languages.find(m_sDefaultLanguage);

	if (Default == m_Languages.end())
	{
		jResult["error"] = kFormat("the default language {} is not in the catalog", m_sDefaultLanguage);
		return jResult;
	}

	for (const auto& Language : m_Languages)
	{
		std::vector<KString> Missing;
		std::vector<KString> Surplus;
		std::vector<KString> Invalid;
		std::vector<KString> Arguments;

		bool bIsDefault = &Language.second == &Default->second;

		for (const auto& Message : Language.second)
		{
			if (Message.second.HasError())
			{
				Invalid.push_back(Message.first);
			}

			auto Reference = Default->second.find(Message.first);

			if (Reference == Default->second.end())
			{
				if (!bIsDefault)
				{
					Surplus.push_back(Message.first);
				}
			}
			else if (!Message.second.HasError() && !Reference->second.HasError())
			{
				// the same arguments, in any order
				auto Have = Message.second.GetArguments();
				auto Want = Reference->second.GetArguments();
				std::sort(Have.begin(), Have.end());
				std::sort(Want.begin(), Want.end());

				if (Have != Want)
				{
					Arguments.push_back(Message.first);
				}
			}
		}

		if (!bIsDefault)
		{
			for (const auto& Reference : Default->second)
			{
				if (!Language.second.contains(Reference.first))
				{
					Missing.push_back(Reference.first);
				}
			}
		}

		KJSON jFindings = KJSON::object();

		struct Finding
		{
			KStringView           sName;
			std::vector<KString>* pList;
		};

		const Finding Findings[] = { { "missing", &Missing }, { "surplus", &Surplus }, { "invalid", &Invalid }, { "arguments", &Arguments } };

		// sorted, so that the report is stable
		for (const auto& F : Findings)
		{
			if (!F.pList->empty())
			{
				std::sort(F.pList->begin(), F.pList->end());
				jFindings[F.sName] = *F.pList;
			}
		}

		if (!jFindings.empty())
		{
			jResult[Language.first] = std::move(jFindings);
		}
	}

	return jResult;

} // Check

//-----------------------------------------------------------------------------
KStringCatalog::Language::Language(const KStringCatalog& Catalog, KStringView sLanguage)
//-----------------------------------------------------------------------------
: m_Catalog(&Catalog)
, m_sLanguage(CanonicalTag(sLanguage))
{
	if (m_sLanguage.empty())
	{
		m_sLanguage = Catalog.GetDefaultLanguage();
	}

} // Language ctor

//-----------------------------------------------------------------------------
KStringView KStringCatalog::Language::operator()(KStringView sID) const
//-----------------------------------------------------------------------------
{
	return m_Catalog ? m_Catalog->Get(m_sLanguage, sID) : sID;

} // operator()

//-----------------------------------------------------------------------------
KString KStringCatalog::Language::Format(KStringView sID, const KJSON& jArgs) const
//-----------------------------------------------------------------------------
{
	return m_Catalog ? m_Catalog->Format(m_sLanguage, sID, jArgs) : KString(sID);

} // Format

//-----------------------------------------------------------------------------
KJSON KStringCatalog::Language::GetNamespace(KStringView sPrefix) const
//-----------------------------------------------------------------------------
{
	return m_Catalog ? m_Catalog->GetNamespace(m_sLanguage, sPrefix) : KJSON::object();

} // GetNamespace

DEKAF2_NAMESPACE_END
