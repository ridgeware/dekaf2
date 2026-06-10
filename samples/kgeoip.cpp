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

// sample: resolve IPv4/IPv6 addresses to a location with KGeoIP, reading a MaxMind DB
// (.mmdb) file (e.g. a freely redistributable DB-IP Lite database), and print the result
// as a KFormTable. The addresses come from the command line, or - with no <ip> argument -
// from stdin, one or more (whitespace separated) per line.
//
// The visual styles (ascii/bold/thin/double/rounded box, spaced, markdown, html) buffer
// all rows so they can drop empty columns and measure widths. The machine-readable styles
// (csv, vertical, json, ndjson) carry a fixed maximal schema and stream one record at a
// time, so "tail -f access.log | kgeoip -f ndjson -db ..." enriches the stream live.
// Markdown joins the streaming category when reading from stdin or with -noheader: the
// rows then print unpadded - jagged but valid markdown, which a renderer aligns anyway.
//
//   kgeoip -db dbip-city-lite.mmdb [-l de] [-f markdown] 8.8.8.8 2001:218::1
//   tail -f access.log | kgeoip -db dbip-city-lite.mmdb -f ndjson
//   kgeoip -db dbip-city-lite.mmdb -f csv --noheader < ips.txt

#include <dekaf2/util/cli/koptions.h>
#include <dekaf2/core/init/dekaf2.h>          // KInit
#include <dekaf2/core/errors/kerror.h>        // KError
#include <dekaf2/core/format/kformat.h>       // kFormat / kPrintLine / kWriteLine / kWrite
#include <dekaf2/core/format/kformtable.h>    // KFormTable
#include <dekaf2/core/strings/kjoin.h>        // kJoined
#include <dekaf2/core/strings/ksplit.h>       // kSplit
#include <dekaf2/io/readwrite/kwriter.h>      // KErr
#include <dekaf2/io/readwrite/kreader.h>      // KIn
#include <dekaf2/net/geo/kgeoip.h>            // KGeoIP
#include <functional>
#include <vector>

using namespace dekaf2;

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class GeoIP : public KErrorBase
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	//-----------------------------------------------------------------------------
	int Main (int argc, char** argv)
	//-----------------------------------------------------------------------------
	{
		KInit (false);          // no signal handler thread
		SetThrowOnError (true); // turn our own errors into exceptions

		KOptions Options (true, KLog::STDOUT, /*bThrow*/true);
		Options.AllowAdHocArgs ();
		Options.SetBriefDescription ("an IP to location resolver");
		Options.SetAdditionalArgDescription ("[<ip> ...]   (with no <ip> the addresses are read from stdin)");

		// bare (positional) arguments are the IP addresses to look up - collect them during
		// the parse, no matter where they appear relative to the options. (The ad hoc
		// mechanism only catches unknown options, a leading bare command would otherwise be
		// flagged as "excess argument".)
		std::vector<KStringViewZ> ArgAddresses;

		Options.UnknownCommand ([&](KOptions::ArgList& Args)
		{
			while (!Args.empty()) { ArgAddresses.push_back (Args.pop()); }
		});

		auto iRetval = Options.Parse (argc, argv, KOut);

		KStringViewZ sDatabase = Options ("db <file>         : path to the .mmdb file", "");
		KStringView  sLanguage = Options ("l,language <code> : language for localized names (en, de, ja, ...)", "en");
		KStringView  sFormat   = Options (kFormat ("f,format <style> : output style ({}), default ascii", KFormTable::GetSupportedStyles()), "ascii");
		bool         bQuiet    = Options ("q,quiet           : do not print the database header line", false);
		bool         bNoHeader = Options ("noheader          : omit the column header row (all output styles)", false);
		std::size_t  iMaxWidth = Options ("w,maxwidth <n>    : maximum column width for the visual styles (0 = unlimited), default 40", 40);

		if (!Options.Check()) return iRetval ? iRetval : 1;

		if (sDatabase.empty())
		{
			throw KError { "usage: kgeoip -db <file.mmdb> [-l <lang>] [-f <style>] [<ip> ...]\n"
			               "       with no <ip> arguments the addresses are read from stdin, "
			               "one or more (whitespace separated) per line" };
		}

		if (!KFormTable::IsKnownStyle (sFormat))
		{
			throw KError { kFormat ("unknown output style '{}', supported: {}", sFormat, KFormTable::GetSupportedStyles()) };
		}

		const auto eStyle = KFormTable::StringToStyle (sFormat);

		// the machine-readable styles (csv, vertical, json, ndjson) carry a fixed maximal
		// schema - every column, always - and need no look-ahead, so they emit one record at
		// a time and stream (a continuous "tail -f | kgeoip" produces output as it arrives).
		// The visual styles buffer all rows to drop empty columns and to measure widths.
		// Markdown sits in between: with <ip> arguments and a header it buffers and aligns
		// the raw text, but in noheader/pipe operation it joins the streaming category and
		// prints unpadded - jagged but valid markdown, which a renderer aligns anyway.
		const bool bFromStdin = ArgAddresses.empty();
		const bool bStreaming = (eStyle & (KFormTable::CSV | KFormTable::Vertical
		                                | KFormTable::JSON | KFormTable::NDJSON)) != 0
		                     || (eStyle == KFormTable::Markdown && (bNoHeader || bFromStdin));

		KGeoIP DB;
		DB.SetDefaultLanguage (sLanguage);

		if (!DB.Open (sDatabase))
		{
			throw KError { DB.Error() };
		}

		// a human banner would corrupt a machine-readable stream (it is not valid CSV/JSON),
		// so it is only shown for the buffered, visual styles
		if (!bQuiet && !bStreaming)
		{
			kWriteLine ();
			kPrintLine ("# {} (built {:%Y-%m-%d}) - {} language(s): {}",
			            DB.GetDatabaseType(), DB.GetBuildTime(), DB.Languages().size(), kJoined (DB.Languages()));
			kWriteLine ();
		}

		// the IP source is the positional arguments if any were given, otherwise stdin -
		// read one or more whitespace separated IPs per line, so both "one IP per line" and
		// "several per line" input streams work. This driver hands each IP to a callback that
		// either prints it right away (streaming) or buffers it (visual styles).
		auto ForEachIP = [&](const std::function<void(KStringView)>& fnEmit)
		{
			if (!ArgAddresses.empty())
			{
				for (auto sIP : ArgAddresses) { fnEmit (sIP); }
			}
			else
			{
				for (auto& sLine : KIn)
				{
					std::vector<KStringView> Tokens;
					kSplit (Tokens, sLine, " \t"); // whitespace delimited, empty tokens collapsed
					for (auto sToken : Tokens) { fnEmit (sToken); }
				}
			}
		};

		// candidate columns - a column is only shown if at least one row carries a value,
		// so a lean database (DB-IP Lite) automatically yields a narrower table than a rich
		// one (MaxMind GeoIP2). Each cell is produced by the column's extractor.
		using Cell = std::function<KString(KStringView, const KGeoIP::LocationView&)>;
		struct Column { KStringView sHeader; KFormTable::Alignment Align; Cell Get; };

		const std::vector<Column> Candidates =
		{
			{ "IP",       KFormTable::Left,   [](KStringView sIP, const KGeoIP::LocationView&  ) { return KString (sIP);            } },
			{ "CC",       KFormTable::Left,   [](KStringView,     const KGeoIP::LocationView& L) { return KString (L.sCountryISO);  } },
			{ "Country",  KFormTable::Left,   [](KStringView,     const KGeoIP::LocationView& L) { return KString (L.sCountryName); } },
			{ "Cont",     KFormTable::Left,   [](KStringView,     const KGeoIP::LocationView& L) { return KString (L.sContinent);   } },
			{ "City",     KFormTable::Left,   [](KStringView,     const KGeoIP::LocationView& L) { return KString (L.sCity);        } },
			{ "Region",   KFormTable::Left,   [](KStringView,     const KGeoIP::LocationView& L) -> KString
				{
					std::vector<KStringView> Names;
					Names.reserve (L.Subdivisions.size());
					for (const auto& Subdivision : L.Subdivisions) { Names.push_back (Subdivision.sName); }
					return kJoined (Names, " / ");
				}
			},
			{ "Postal",   KFormTable::Left,   [](KStringView,     const KGeoIP::LocationView& L) { return KString (L.sPostalCode);  } },
			{ "Lat",      KFormTable::Right,  [](KStringView,     const KGeoIP::LocationView& L) { return (L.nLatitude != 0.0 || L.nLongitude != 0.0) ? kFormat ("{:.4f}", L.nLatitude)  : KString{}; } },
			{ "Lon",      KFormTable::Right,  [](KStringView,     const KGeoIP::LocationView& L) { return (L.nLatitude != 0.0 || L.nLongitude != 0.0) ? kFormat ("{:.4f}", L.nLongitude) : KString{}; } },
			{ "Acc/km",   KFormTable::Right,  [](KStringView,     const KGeoIP::LocationView& L) { return L.iAccuracyRadius ? kFormat ("{}", L.iAccuracyRadius) : KString{}; } },
			{ "TimeZone", KFormTable::Left,   [](KStringView,     const KGeoIP::LocationView& L) { return KString (L.sTimeZone);    } },
			{ "EU",       KFormTable::Center, [](KStringView,     const KGeoIP::LocationView& L) { return L.bIsInEU ? KString ("yes") : KString{}; } },
		};

		// --- streaming path: fixed maximal schema, emit (and flush) one record at a time ---
		if (bStreaming)
		{
			KFormTable Table (KOut);
			Table.SetStyle (eStyle);
			Table.SetPrintHeader (!bNoHeader);

			for (const auto& Col : Candidates)
			{
				Table.AddColDef (KFormTable::ColDef (KString (Col.sHeader), 0, Col.Align));
			}

			// CSV and markdown are the streaming styles with a visible header row that should
			// appear up front and even when no records follow; the others have no header
			// (json/ndjson) or only an invisible measuring pass run lazily on the first row
			// (vertical), so forcing it there would do nothing useful (and would make empty
			// json dump "null")
			if (eStyle == KFormTable::CSV || eStyle == KFormTable::Markdown)
			{
				Table.PrintHeader ();
				KOut.Flush (); // make the header visible before the first record arrives
			}

			std::vector<KString> Cells;

			ForEachIP ([&](KStringView sIP)
			{
				auto View = DB.LookupView (sIP);
				Cells.clear ();
				for (const auto& Col : Candidates) { Cells.push_back (Col.Get (sIP, View)); }
				Table.PrintRow (Cells);
				KOut.Flush (); // per record, so "tail -f | kgeoip" stays responsive
			});

			Table.Close ();
			KOut.Flush ();

			return 0;
		}

		// --- buffered path: collect all rows so empty columns can be dropped and widths   ---
		// --- measured. LookupView is allocation-free and its views stay valid while DB is ---
		// --- open; the owned IP strings in Rows keep the IP column's views alive too.     ---
		std::vector<std::pair<KString, KGeoIP::LocationView>> Rows;

		ForEachIP ([&](KStringView sIP)
		{
			Rows.emplace_back (KString (sIP), DB.LookupView (sIP));
		});

		KString    sTable;
		KFormTable Table (sTable);
		Table.SetStyle (eStyle);
		Table.SetPrintHeader (!bNoHeader);

		if (iMaxWidth)
		{
			// 0 = unlimited: KFormTable's default is no width cap, so simply skip the call.
			// Content wider than the cap is cut and flagged with a trailing '>' marker.
			Table.SetMaxColWidth (iMaxWidth);
		}

		// keep only the columns that carry data, and remember each row's cells in that order
		std::vector<std::vector<KString>> RowCells (Rows.size());

		for (const auto& Col : Candidates)
		{
			std::vector<KString> ColumnCells;
			ColumnCells.reserve (Rows.size());
			bool bAnyValue = false;

			for (const auto& Row : Rows)
			{
				ColumnCells.push_back (Col.Get (Row.first, Row.second));
				if (!ColumnCells.back().empty()) { bAnyValue = true; }
			}

			if (!bAnyValue) { continue; } // drop the empty column -> DB-adaptive, variable output

			// width 0 = auto: AddColDef seeds it to the header width, the dry pass (if the
			// style needs it - see WantDryMode() below) grows it to the widest data cell
			Table.AddColDef (KFormTable::ColDef (KString (Col.sHeader), 0, Col.Align));

			for (std::size_t i = 0; i < Rows.size(); ++i)
			{
				RowCells[i].push_back (std::move (ColumnCells[i]));
			}
		}

		// the rows are in memory, so the measuring pass is trivial; KFormTable decides via
		// WantDryMode() whether the chosen style needs it (box / spaced / markdown do)
		auto PrintRows = [&]
		{
			for (auto& Cells : RowCells) { Table.PrintRow (Cells); }
		};

		if (Table.WantDryMode())
		{
			Table.DryMode (true);
			PrintRows ();
			Table.DryMode (false);
		}

		PrintRows ();
		Table.Close ();

		kWrite (sTable);

		return 0;

	} // Main

}; // GeoIP

//-----------------------------------------------------------------------------
int main (int argc, char** argv)
//-----------------------------------------------------------------------------
{
	try
	{
		return GeoIP().Main (argc, argv);
	}
	catch (const std::exception& ex)
	{
		KErr.FormatLine (">> kgeoip: {}", ex.what());
	}

	return 1;
}
