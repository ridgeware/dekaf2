#include "catch.hpp"
#include <dekaf2/data/json/kconfig.h>
#include <dekaf2/system/filesystem/kfilesystem.h>
#include <dekaf2/io/readwrite/kreader.h>
#include <dekaf2/core/format/kformat.h>

using namespace dekaf2;

TEST_CASE("KConfig")
{
	SECTION("default path")
	{
		KConfig cfg;
		CHECK ( !cfg.Path().empty() );
		CHECK ( cfg.Path().ends_with("config.json") );
	}

	SECTION("explicit path ctor with missing file: Loaded() is false, path remembered")
	{
		KConfig cfg("/path/that/does/not/exist/config.json");
		CHECK ( cfg.Path() == "/path/that/does/not/exist/config.json" );
		CHECK ( !cfg.Loaded() );
	}

	SECTION("Load returns false for missing file")
	{
		KTempDir Tmp;
		auto sFile = kFormat("{}{}missing.json", Tmp.Name(), kDirSep);

		KConfig cfg(sFile);
		CHECK ( !cfg.Loaded() );
		// the path should still be remembered for subsequent Save()
		CHECK ( cfg.Path() == sFile );
	}

	SECTION("Save then construct-and-load roundtrip")
	{
		KTempDir Tmp;
		auto sFile = kFormat("{}{}config.json", Tmp.Name(), kDirSep);

		// write (ctor attempts to load the not-yet-existing file -> no-op)
		KConfig writer(sFile);
		CHECK ( !writer.Loaded() );
		writer["display"]["format"] = "ascii";
		writer["display"]["width"]  = 120;
		writer["features"]          = KJSON::array();
		writer["features"]         += "alpha";
		writer["features"]         += "beta";
		CHECK ( writer.Save() );

		// construct -> auto-load
		KConfig reader(sFile);
		CHECK ( reader.Loaded() );

		CHECK ( reader["display"]["format"].String() == "ascii" );
		CHECK ( reader["display"]["width"].UInt64()  == 120     );
		CHECK ( reader["features"].size()            == 2       );
		CHECK ( reader["features"][0].String()       == "alpha" );
	}

	SECTION("operator() never inserts")
	{
		KConfig cfg;
		const KConfig& cref = cfg;
		// reading an absent key via operator() must not modify the JSON
		auto& v = cref("absent");
		CHECK ( v.empty() );
		CHECK ( !cfg.Get().contains("absent") );
	}

	SECTION("operator[] inserts on miss")
	{
		KConfig cfg;
		cfg["created"] = true;
		CHECK ( cfg.Get().contains("created") );
		CHECK ( cfg["created"].Bool() );
	}

	SECTION("Save creates parent directory")
	{
		KTempDir Tmp;
		auto sFile = kFormat("{}{}sub{}deep{}cfg.json", Tmp.Name(), kDirSep, kDirSep, kDirSep);

		KConfig cfg(sFile);
		cfg["x"] = 42;
		CHECK ( cfg.Save() );
		CHECK ( kFileExists(sFile) );
	}

	SECTION("SetPath does not trigger load")
	{
		KConfig cfg;
		cfg.SetPath("/tmp/some.json");
		CHECK ( cfg.Path() == "/tmp/some.json" );
		// SetPath is a plain setter; the user must call Load() explicitly.
	}

	SECTION("JSON Pointer access via operator[] and operator()")
	{
		KConfig cfg;
		cfg["/display/format"] = "ascii";
		cfg["/display/width"]  = 120;

		// read back via the same pointer syntax
		CHECK ( cfg("/display/format").String() == "ascii" );
		CHECK ( cfg("/display/width").UInt64()  == 120     );

		// also reachable via plain-key chaining
		CHECK ( cfg("display")("format").String() == "ascii" );

		// underlying structure is properly nested
		CHECK ( cfg.Get().contains("display") );
		CHECK ( cfg.Get()["display"].contains("format") );
	}

	SECTION("dotted notation access")
	{
		KConfig cfg;
		cfg[".db.host"] = "localhost";
		cfg[".db.port"] = 5432;

		CHECK ( cfg(".db.host").String() == "localhost" );
		CHECK ( cfg(".db.port").UInt64() == 5432        );
		CHECK ( cfg("/db/host").String() == "localhost" );
	}

	SECTION("operator() never inserts even with pointer")
	{
		KConfig cfg;
		const KConfig& cref = cfg;
		auto& v = cref("/missing/path");
		CHECK ( v.empty() );
		CHECK ( !cfg.Get().contains("missing") );
	}

	SECTION("Save of empty/untouched config writes {} not null")
	{
		KTempDir Tmp;
		auto sFile = kFormat("{}{}empty.json", Tmp.Name(), kDirSep);

		KConfig cfg(sFile);
		CHECK ( cfg.Save() );

		KString sContents = kReadAll(sFile);
		CHECK ( sContents.contains("{") );
		CHECK ( !sContents.contains("null") );

		// must be loadable back
		KConfig reader(sFile);
		CHECK ( reader.Load() );
		CHECK ( reader.Get().is_object() );
	}

	SECTION("a missing file is no error")
	{
		KTempDir Tmp;
		KConfig cfg(kFormat("{}{}missing.json", Tmp.Name(), kDirSep));
		CHECK ( !cfg.Loaded() );
		CHECK ( !cfg.HasError() );
	}

	SECTION("a parse error names the file and the position")
	{
		KTempDir Tmp;
		auto sFile = kFormat("{}{}broken.json", Tmp.Name(), kDirSep);
		REQUIRE ( kWriteFile(sFile, "{\n\t\"a\": \n}\n") );

		KConfig cfg(sFile);
		CHECK ( !cfg.Loaded() );
		CHECK ( cfg.HasError() );
		CHECK ( cfg.Error().contains(sFile) );
		CHECK ( cfg.Error().contains("line 3") );
		CHECK ( cfg.Get().is_null() );
	}

	SECTION("content after the JSON value is an error")
	{
		KTempDir Tmp;
		auto sFile = kFormat("{}{}trailing.json", Tmp.Name(), kDirSep);

		// a second object, and the leftover of a merge conflict
		for (KStringView sTrailer : { "{\"b\": 2}\n", "<<<<<<< HEAD\n" })
		{
			REQUIRE ( kWriteFile(sFile, kFormat("{{\"a\": 1}}\n{}", sTrailer)) );
			KConfig cfg(sFile);
			CHECK ( !cfg.Loaded() );
			CHECK ( cfg.HasError() );
		}

		// white space after the value is fine
		REQUIRE ( kWriteFile(sFile, "{\"a\": 1}\n\n  \t\n") );
		KConfig cfg(sFile);
		CHECK ( cfg.Loaded() );
		CHECK ( cfg("a").UInt64() == 1 );
	}

	SECTION("UTF-8 with byte order mark, and UTF-16 with byte order mark")
	{
		KTempDir Tmp;
		auto sFile = kFormat("{}{}encoded.json", Tmp.Name(), kDirSep);

		REQUIRE ( kWriteFile(sFile, "\xEF\xBB\xBF{\"name\": \"Gr\xC3\xBC\xC3\x9F\"}\n") );
		{
			KConfig cfg(sFile);
			CHECK ( cfg.Loaded() );
			CHECK ( cfg("name").String() == "Gr\xC3\xBC\xC3\x9F" );
		}

		// what Windows PowerShell 5.1 writes with > or Out-File: UTF-16 LE with BOM
		KString sUTF16 { "\xFF\xFE" };
		for (auto ch : KStringView("{\"name\": \"Gr")) { sUTF16 += ch; sUTF16 += '\0'; }
		sUTF16 += "\xFC"; sUTF16 += '\0'; // u umlaut
		sUTF16 += "\xDF"; sUTF16 += '\0'; // sharp s
		for (auto ch : KStringView("\"}\r\n"))   { sUTF16 += ch; sUTF16 += '\0'; }
		REQUIRE ( kWriteFile(sFile, sUTF16) );
		{
			KConfig cfg(sFile);
			CHECK ( cfg.Loaded() );
			CHECK ( cfg("name").String() == "Gr\xC3\xBC\xC3\x9F" );

			// saving writes UTF-8 without byte order mark
			CHECK ( cfg.Save() );
			CHECK ( kReadAll(sFile).starts_with("{") );
		}
	}

	SECTION("Save reports a failure")
	{
		KTempDir Tmp;
		auto sBlocker = kFormat("{}{}file", Tmp.Name(), kDirSep);
		REQUIRE ( kWriteFile(sBlocker, "x") );

		// the parent of the target is a regular file
		KConfig cfg(kFormat("{}{}config.json", sBlocker, kDirSep));
		cfg["a"] = 1;
		CHECK ( !cfg.Save() );
		CHECK ( cfg.HasError() );
		CHECK ( !cfg.Error().empty() );
	}

#ifdef DEKAF2_IS_UNIX
	SECTION("Save with a file mode")
	{
		KTempDir Tmp;
		auto sFile = kFormat("{}{}secret.json", Tmp.Name(), kDirSep);

		KConfig cfg(sFile);
		cfg["password"] = "not for others";
		CHECK ( cfg.Save({}, 0600) );
		CHECK ( (kGetMode(sFile) & 0777) == 0600 );

		// an existing, wider file is narrowed
		REQUIRE ( kChangeMode(sFile, 0644) );
		CHECK ( cfg.Save({}, 0600) );
		CHECK ( (kGetMode(sFile) & 0777) == 0600 );
		CHECK ( !cfg.HasError() );
	}
#endif
}
