/*
//
// DEKAF(tm): Lighter, Faster, Smarter(tm)
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
// |/|   OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR          |\|
// |\|   OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR        |\|
// |/|   OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE         |/|
// |\|   SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.            |\|
// |/|                                                                     |/|
// |/+---------------------------------------------------------------------+/|
// |\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/ |
// +-------------------------------------------------------------------------+
//
*/

#pragma once

/// @file kgeoip.h
/// IP geolocation (IPv4 and IPv6) through a self contained reader for the
/// MaxMind DB (.mmdb) binary format, e.g. the freely redistributable DB-IP
/// Lite databases (CC-BY-4.0). No third party library is required.
/// "MaxMind", "GeoIP" and "GeoLite2" are trademarks of MaxMind, Inc.
/// dekaf2 is not affiliated with or endorsed by MaxMind, Inc.

#include <dekaf2/core/init/kdefinitions.h>
#include <dekaf2/core/strings/kstring.h>
#include <dekaf2/core/strings/kstringview.h>
#include <dekaf2/core/errors/kerror.h>
#include <dekaf2/net/address/kipaddress.h>
#include <dekaf2/io/readwrite/kmemorymap.h>
#include <dekaf2/time/clock/ktime.h>
#include <cstdint>
#include <cstddef>
#include <vector>

DEKAF2_NAMESPACE_BEGIN

/// @addtogroup net_geo
/// @{

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Looks up the geographic location of an IPv4 or IPv6 address in a MaxMind DB
/// (.mmdb) file. The format is read directly - no external dependency. The
/// database is memory mapped, so several threads and processes
/// share a single copy and lookups are lock free and allocation light.
///
/// Recommended free data: DB-IP Lite (City or Country), licensed CC-BY-4.0.
/// Note the attribution requirement of that license when displaying results.
class DEKAF2_PUBLIC KGeoIP : public KErrorBase
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	/// language-independent numeric identity of a location: the GeoNames IDs plus
	/// the numeric location data. This is the shared base of LocationView and
	/// Location. Comparable across results regardless of display language.
	struct LocationIDs
	{
		uint32_t              iContinentGeoNameID { 0 };
		uint32_t              iCountryGeoNameID   { 0 };
		uint32_t              iCityGeoNameID      { 0 };
		std::vector<uint32_t> SubdivisionGeoNameIDs;         ///< most general first
		double                nLatitude           { 0.0 };   ///< approximate latitude
		double                nLongitude          { 0.0 };   ///< approximate longitude
		uint32_t              iAccuracyRadius     { 0 };     ///< accuracy radius in km
		bool                  bIsInEU             { false }; ///< country.is_in_european_union

		/// true if no location at all was resolved
		DEKAF2_NODISCARD
		bool empty () const
		{
			return iContinentGeoNameID == 0 && iCountryGeoNameID == 0 && iCityGeoNameID == 0;
		}

		/// same place, independent of display language (compares GeoNames IDs; the
		/// approximate coordinates are deliberately not part of the identity)
		DEKAF2_NODISCARD
		bool operator== (const LocationIDs& other) const
		{
			return iContinentGeoNameID   == other.iContinentGeoNameID
			    && iCountryGeoNameID     == other.iCountryGeoNameID
			    && iCityGeoNameID        == other.iCityGeoNameID
			    && SubdivisionGeoNameIDs == other.SubdivisionGeoNameIDs;
		}
		DEKAF2_NODISCARD
		bool operator!= (const LocationIDs& other) const { return !operator==(other); }

	}; // LocationIDs

	/// subdivision (region / state / county) code + name, as zero-copy views;
	/// index-aligned with LocationIDs::SubdivisionGeoNameIDs
	struct SubdivisionView
	{
		KStringView sISO;  ///< ISO-3166-2 subdivision code, e.g. "CA", "ENG"
		KStringView sName; ///< localized subdivision name
	};

	/// subdivision code + name, owned; index-aligned with SubdivisionGeoNameIDs
	struct Subdivision
	{
		KString sISO;  ///< ISO-3166-2 subdivision code, e.g. "CA", "ENG"
		KString sName; ///< localized subdivision name
	};

	/// a lookup result as zero-copy views into the memory mapped database. Valid
	/// only while this KGeoIP stays open and the database is not reloaded - copy
	/// into a Location object (owned) to keep it beyond that.
	struct LocationView : LocationIDs
	{
		KStringView sContinent;     ///< continent code, e.g. "EU"
		KStringView sContinentName; ///< localized continent name
		KStringView sCountryISO;    ///< ISO-3166-1 alpha-2 country code, e.g. "DE"
		KStringView sCountryName;   ///< localized country name
		KStringView sCity;          ///< localized city name
		KStringView sPostalCode;    ///< postal code
		KStringView sTimeZone;      ///< IANA time zone, e.g. "Europe/Berlin"
		std::vector<SubdivisionView> Subdivisions; ///< most general first

		/// true if neither a country nor a city was resolved
		DEKAF2_NODISCARD
		bool empty () const { return sCountryISO.empty() && sCity.empty(); }

	}; // LocationView

	/// a lookup result with owned strings - safe to store and to keep beyond the
	/// lifetime of the database
	struct Location : LocationIDs
	{
		KString sContinent;     ///< continent code, e.g. "EU"
		KString sContinentName; ///< localized continent name
		KString sCountryISO;    ///< ISO-3166-1 alpha-2 country code, e.g. "DE"
		KString sCountryName;   ///< localized country name
		KString sCity;          ///< localized city name
		KString sPostalCode;    ///< postal code
		KString sTimeZone;      ///< IANA time zone, e.g. "Europe/Berlin"
		std::vector<Subdivision> Subdivisions; ///< most general first

		Location () = default;
		/// build owned strings from a LocationView, copying the numeric identity base
		explicit Location (const LocationView& View) : Location (LocationIDs(View),            View) {}
		/// build owned strings from a LocationView, moving the numeric identity base
		/// (the strings are still copied - a view never owns its bytes)
		explicit Location (LocationView&& View)      : Location (LocationIDs(std::move(View)), View) {}

		/// true if neither a country nor a city was resolved
		DEKAF2_NODISCARD
		bool empty () const { return sCountryISO.empty() && sCity.empty(); }

	//----------
	private:
	//----------

		/// delegated target: move the prepared identity base in, construct the owned
		/// strings from the view in the init list, build the subdivisions in the body
		Location (LocationIDs&& Identity, const LocationView& View);

	}; // Location

	/// default constructor, does not open a database
	KGeoIP () = default;

	/// constructs and immediately opens the given .mmdb file - check is_open() / Error()
	explicit KGeoIP (KStringViewZ sDatabaseFile)
	{
		Open (sDatabaseFile);
	}

	/// open (memory map) a MaxMind DB (.mmdb) file
	/// @param sDatabaseFile path to the .mmdb file
	/// @return true on success, false on error (see Error())
	bool Open (KStringViewZ sDatabaseFile);

	/// close the database, releasing the mapping
	void Close ();

	/// is a database currently open?
	DEKAF2_NODISCARD
	bool is_open () const
	{
		return m_File.is_open();
	}

	/// look up an address, returning owned strings (safe to store).
	/// @param sLanguage language for localized names; empty uses the default
	/// (see SetDefaultLanguage()). Fallback chain: requested -> default -> first
	/// language the record carries.
	/// @return the Location, which is empty() if the address was not found
	DEKAF2_NODISCARD
	Location Lookup (const KIPAddress& IP, KStringView sLanguage = KStringView{}) const;
	DEKAF2_NODISCARD
	Location Lookup (KStringView sIP,      KStringView sLanguage = KStringView{}) const;

	/// look up an address, returning zero-copy views into the database. The views
	/// are valid only while this KGeoIP stays open and is not reloaded. Cheapest
	/// for read-and-discard; copy into a Location to keep the result.
	DEKAF2_NODISCARD
	LocationView LookupView (const KIPAddress& IP, KStringView sLanguage = KStringView{}) const;
	DEKAF2_NODISCARD
	LocationView LookupView (KStringView sIP,      KStringView sLanguage = KStringView{}) const;

	/// look up only the numeric identity (GeoNames IDs + coordinates). Language
	/// independent, decodes no strings, and the result is freely storable.
	DEKAF2_NODISCARD
	LocationIDs LookupIDs (const KIPAddress& IP) const;
	DEKAF2_NODISCARD
	LocationIDs LookupIDs (KStringView sIP) const;

	/// the database type string from the metadata, e.g. "DBIP-City-Lite" or "GeoLite2-Country"
	DEKAF2_NODISCARD
	KStringView GetDatabaseType () const
	{
		return m_sDatabaseType;
	}

	/// the build time of the database, taken from its metadata
	DEKAF2_NODISCARD
	KUnixTime GetBuildTime () const
	{
		return m_tBuildTime;
	}

	/// the IP version the database was built for (4 or 6 - a value of 6 also serves IPv4)
	DEKAF2_NODISCARD
	uint16_t GetIPVersion () const
	{
		return m_iIPVersion;
	}

	/// the languages the database carries localized names in (from its metadata),
	/// e.g. "en", "de", "ja", "zh-CN". May be empty for a country-only database.
	DEKAF2_NODISCARD
	const std::vector<KString>& Languages () const
	{
		return m_Languages;
	}

	/// set the default language for localized names (city, country). Used when a
	/// lookup does not request a language, and as the fallback when a requested
	/// language is missing from a record. Defaults to "en". Kept across Open/Close.
	/// The tag is normalized to BCP-47 case (e.g. "PT-br" -> "pt-BR").
	KGeoIP& SetDefaultLanguage (KStringView sLanguage)
	{
		m_sDefaultLanguage = NormalizeLanguage (sLanguage);
		return *this;
	}

	/// the configured default language (initially "en")
	DEKAF2_NODISCARD
	KStringView GetDefaultLanguage () const
	{
		return m_sDefaultLanguage;
	}

//----------
private:
//----------

	/// read the left (iBit == 0) or right (iBit == 1) record of a search tree node
	DEKAF2_NODISCARD
	uint32_t ReadRecord (uint32_t iNode, uint32_t iBit) const;

	/// walk the search tree for the given address bits, returning the final record value
	DEKAF2_NODISCARD
	uint32_t FindNode (const uint8_t* pAddress, int iStartBit, int iBitCount, uint32_t iStartNode) const;

	/// normalize a language tag to BCP-47 case conventions (language subtag lower case,
	/// region subtag upper case), e.g. "PT-br" -> "pt-BR", so we match the database keys
	DEKAF2_NODISCARD
	static KString NormalizeLanguage (KStringView sLanguage);

	/// resolve an address to its data section offset; false if not found
	DEKAF2_NODISCARD
	bool FindDataOffset (const KIPAddress& IP, std::size_t& iDataOffset) const;

	/// decode a record in a single pass into a LocationView. The numeric LocationIDs
	/// base is always filled; the (zero-copy) string fields only when bWantStrings is
	/// true (LookupView). With bWantStrings == false (LookupIDs) no strings are read.
	/// Localized names use sLanguage with fallback default -> first available.
	void DecodeRecord (std::size_t iDataOffset, LocationView& View, KStringView sLanguage, bool bWantStrings) const;

	KConstMemoryMap m_File;                       ///< the memory mapped database file
	const uint8_t*  m_pTree          { nullptr }; ///< start of the search tree (== file start)
	const uint8_t*  m_pDataSection   { nullptr }; ///< start of the data section
	std::size_t     m_iDataSize      { 0 };       ///< size of the data section in bytes
	std::size_t     m_iRecordLength  { 0 };       ///< bytes per search tree node (= record_size / 4)
	uint32_t        m_iNodeCount     { 0 };       ///< number of search tree nodes
	uint32_t        m_iIPv4StartNode { 0 };       ///< node where the IPv4 subtree starts (ip_version 6)
	uint16_t        m_iRecordSize    { 0 };       ///< bits per tree record (24, 28 or 32)
	uint16_t        m_iIPVersion     { 0 };       ///< 4 or 6
	KUnixTime       m_tBuildTime;                 ///< build time from the metadata
	KString         m_sDatabaseType;              ///< database type string from the metadata

	// localized-name configuration (m_sDefaultLanguage is kept across Open/Close)
	std::vector<KString> m_Languages;                 ///< languages the database carries (from metadata)
	KString              m_sDefaultLanguage { "en" }; ///< default / fallback language for localized names

}; // KGeoIP

/// @}

DEKAF2_NAMESPACE_END
