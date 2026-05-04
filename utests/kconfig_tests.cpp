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
		writer["features"]          = KJSON2::array();
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
}
