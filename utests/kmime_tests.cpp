#include "catch.hpp"

#include <dekaf2/web/url/kmime.h>
#include <dekaf2/system/filesystem/kfilesystem.h>
#include <dekaf2/system/os/ksystem.h>
#include <dekaf2/io/streams/koutstringstream.h>
#include <dekaf2/data/json/kjson.h>
#include <dekaf2/util/id/kuuid.h>
#include <vector>

using namespace dekaf2;

namespace {

#if DEKAF2_HAS_CPP_14
KString Normalized(KStringView sInput)
{
	KString sOut { sInput };
	sOut.Replace("\r\n", "\n");
	return sOut;
}
#endif

void testMime(const KMIME& mime)
{
}

// a ZIP archive with entries that are stored without compression - enough for the
// detection of the formats that are ZIP archives
KString ZipArchive(const std::vector<std::pair<KStringView, KStringView>>& Entries)
{
	KString sArchive;

	auto Append16 = [&sArchive](uint16_t i)
	{
		sArchive += static_cast<char>(i & 0xFF);
		sArchive += static_cast<char>(i >> 8);
	};

	auto Append32 = [&](uint32_t i)
	{
		Append16(static_cast<uint16_t>(i & 0xFFFF));
		Append16(static_cast<uint16_t>(i >> 16));
	};

	for (const auto& Entry : Entries)
	{
		sArchive += "PK\x03\x04"_ksv;
		Append16(10);                                           // version
		Append16(0);                                            // flags
		Append16(0);                                            // stored
		Append32(0);                                            // time and date
		Append32(0);                                            // CRC, not checked
		Append32(static_cast<uint32_t>(Entry.second.size()));   // compressed size
		Append32(static_cast<uint32_t>(Entry.second.size()));   // size
		Append16(static_cast<uint16_t>(Entry.first.size()));    // name length
		Append16(0);                                            // extra length
		sArchive += Entry.first;
		sArchive += Entry.second;
	}

	return sArchive;
}

} // end of anonymous namespace

TEST_CASE("KMIME")
{
	SECTION("assignment")
	{
		KMIME a;
		a = "abc";
		a = "abc"_ks;
		a = "abc"_ksv;
		a = "abc"_ksz;
		a = std::string("abc");

		testMime("abc"_ks);
		testMime("abc"_ksv);
		testMime("abc"_ksz);
	}

	SECTION("by extension")
	{
		KMIME a;

		CHECK( a.ByExtension("test.txt") );
		CHECK( a == KMIME::TEXT_UTF8 );

		CHECK( a.ByExtension("/folder/test.dir/test.txt") );
		CHECK( a == KMIME::TEXT_UTF8 );

		CHECK( a.ByExtension(".txt") );
		CHECK( a == KMIME::TEXT_UTF8 );

		CHECK( a.ByExtension("txt") );
		CHECK( a == KMIME::TEXT_UTF8 );

		CHECK( a.ByExtension(".html") );
		CHECK( a == KMIME::HTML_UTF8 );

		CHECK( a.ByExtension(".HTML") );
		CHECK( a == KMIME::HTML_UTF8 );

		CHECK( a.ByExtension("css") );
		CHECK( a == KMIME::CSS );
	}

	SECTION("compressors")
	{
		KMIME a;

		CHECK ( a.ByExtension("archive.tar.gz") );
		CHECK ( a == KMIME::GZIP );
		CHECK ( a.ByExtension("archive.tgz") );
		CHECK ( a == KMIME::GZIP );
		CHECK ( a.ByExtension("archive.tar.xz") );
		CHECK ( a == KMIME::XZ );
		CHECK ( a.ByExtension("archive.txz") );
		CHECK ( a == KMIME::XZ );
		CHECK ( a.ByExtension("archive.tbz2") );
		CHECK ( a == KMIME::BZ2 );
		CHECK ( a.ByExtension("archive.tzst") );
		CHECK ( a == KMIME::ZSTD );
		CHECK ( a.ByExtension("data.file.br") );
		CHECK ( a == KMIME::BR );

		// already compressed - never compressed again on the fly
		for (auto sType : { KMIME::GZIP, KMIME::XZ, KMIME::BR, KMIME::BZ2, KMIME::ZSTD })
		{
			KMIME m(sType);
			CHECK ( m.IsCompressible() == false );
		}

		// a plain tar is not compressed
		KMIME t(KMIME::TAR);
		CHECK ( t.IsCompressible() );

		// media and fonts with a compression of their own
		for (auto sFile : { "movie.webm", "image.webp", "font.woff", "font.woff2" })
		{
			KMIME m;
			CHECK ( m.ByExtension(sFile) );
			CHECK ( m.IsCompressible() == false );
		}
	}

	SECTION("kGetMIMETypeOfData")
	{
		KString sTar(512, '\0');
		sTar.replace(257, 5, "ustar");

		std::vector<std::pair<KString, KStringView>> Samples
		{
			{ "%PDF-1.7\n%\xE2\xE3\xCF\xD3\n"_ksv,                                  KMIME::PDF        },
			{ "\x89PNG\r\n\x1A\n\0\0\0\x0DIHDR"_ksv,                               KMIME::PNG        },
			{ "\xFF\xD8\xFF\xE0\0\x10JFIF\0"_ksv,                                  KMIME::JPEG       },
			{ "GIF89a\x01\0\x01\0"_ksv,                                            KMIME::GIF        },
			{ "II*\0\x08\0\0\0"_ksv,                                               KMIME::TIFF       },
			{ "\0\0\0\x0CjP  \r\n\x87\n"_ksv,                                      KMIME::JPEG2000   },
			{ "BM\x3A\0\0\0\0\0\0\0\x36\0\0\0\x28\0\0\0"_ksv,                      KMIME::BMP        },
			{ "\0\0\1\0\1\0\x10\x10\0\0\1\0\x20\0\x68\x04\0\0\x16\0\0\0\0"_ksv,    KMIME::ICON       },
			{ "RIFF\x24\0\0\0WEBPVP8 "_ksv,                                        KMIME::WEBP       },
			{ "RIFF\x24\0\0\0WAVEfmt "_ksv,                                        KMIME::WAV        },
			{ "RIFF\x24\0\0\0AVI LIST"_ksv,                                        KMIME::AVI        },
			{ "OggS\0\x02\0\0\0\0\0\0\0\0\x01\0\0\0\0\0\0\0\0\0\0\0\x01\x1E\x01vorbis"_ksv, KMIME::OGA },
			{ "ID3\x04\0\0\0\0\0\0"_ksv,                                           KMIME::MP3        },
			{ "\xFF\xFB\x90\x64"_ksv,                                              KMIME::MP3        },
			{ "\xFF\xF1\x50\x80"_ksv,                                              KMIME::AAC        },
			{ "\0\0\0\030ftypmp42\0\0\0\0"_ksv,                                    KMIME::MP4        },
			{ "\x1A\x45\xDF\xA3\x9F\x42\x86\x81\x01\x42\xF7\x81\x01\x42\x82\x84webm"_ksv, KMIME::WEBM },
			{ "MThd\0\0\0\x06"_ksv,                                                KMIME::MIDI       },
			{ ZipArchive({{ "hello.txt", "hello" }}),                              KMIME::ZIP        },
			{ ZipArchive({{ "mimetype", KMIME::ODT }, { "content.xml", "<x/>" }}), KMIME::ODT        },
			{ ZipArchive({{ "mimetype", KMIME::EPUB }}),                           KMIME::EPUB       },
			{ ZipArchive({{ "[Content_Types].xml", "<x/>" }, { "word/document.xml", "<x/>" }}), KMIME::DOCX },
			{ ZipArchive({{ "[Content_Types].xml", "<x/>" }, { "xl/workbook.xml", "<x/>" }}),   KMIME::XLSX },
			{ ZipArchive({{ "META-INF/MANIFEST.MF", "Manifest-Version: 1.0" }}),   KMIME::JAR        },
			{ "\x1F\x8B\x08\0"_ksv,                                                KMIME::GZIP       },
			{ "BZh91AY&SY"_ksv,                                                    KMIME::BZ2        },
			{ "\xFD\x37zXZ\0\0\x04"_ksv,                                           KMIME::XZ         },
			{ "\x28\xB5\x2F\xFD\x04\0"_ksv,                                        KMIME::ZSTD       },
			{ "7z\xBC\xAF\x27\x1C\0\x04"_ksv,                                      KMIME::SEVENZIP   },
			{ "Rar!\x1A\x07\x01\0"_ksv,                                            KMIME::RAR        },
			{ sTar,                                                                KMIME::TAR        },
			{ "\0asm\x01\0\0\0"_ksv,                                               KMIME::WASM       },
			{ "wOFF\0\1\0\0"_ksv,                                                  KMIME::WOFF       },
			{ "wOF2\0\1\0\0"_ksv,                                                  KMIME::WOFF2      },
			{ "OTTO\0\x0A"_ksv,                                                    KMIME::OTF        },
			{ "\0\1\0\0\0\x0A\0\x80"_ksv,                                          KMIME::TTF        },
			{ "FWS\x0A"_ksv,                                                       KMIME::SWF        },
			{ "\xD0\xCF\x11\xE0\xA1\xB1\x1A\xE1\0\0W\0o\0r\0d\0D\0o\0c\0u\0m\0e\0n\0t\0"_ksv, KMIME::DOC },
			{ "\xD0\xCF\x11\xE0\xA1\xB1\x1A\xE1\0\0"_ksv,                          "application/x-ole-storage" },
			{ "{\\rtf1\\ansi"_ksv,                                                 KMIME::RTF        },
			{ "Hello, world.\n"_ksv,                                               KMIME::TEXT_UTF8  },
			{ "Gr\xC3\xBC\xC3\x9F" "e\n"_ksv,                                      KMIME::TEXT_UTF8  },
			{ "\xEF\xBB\xBFwith a byte order mark"_ksv,                            KMIME::TEXT_UTF8  },
			// the read part of a file may end within a character
			{ "cut at the end \xC3"_ksv,                                           KMIME::TEXT_UTF8  },
			{ "\xFF\xFEh\0i\0"_ksv,                                                KMIME::TEXT_PLAIN },
			{ "<!DOCTYPE html>\n<html><body></body></html>"_ksv,                  KMIME::HTML_UTF8  },
			{ "  <html><body></body></html>"_ksv,                                  KMIME::HTML_UTF8  },
			{ "<?xml version=\"1.0\"?>\n<root/>"_ksv,                              KMIME::XML        },
			{ "<?xml version=\"1.0\"?>\n<svg xmlns=\"http://www.w3.org/2000/svg\"/>"_ksv, KMIME::SVG },
			{ "<svg xmlns=\"http://www.w3.org/2000/svg\"/>"_ksv,                   KMIME::SVG        },
			{ "{\"name\":\"test\"}"_ksv,                                           KMIME::JSON       },
			{ "[ 1, 2, 3 ]"_ksv,                                                   KMIME::JSON       },
			// an INI section is no JSON
			{ "[section]\nkey=value\n"_ksv,                                        KMIME::TEXT_UTF8  },
			{ "#!/bin/sh\necho\n"_ksv,                                             KMIME::SH         },
			{ "#!/bin/bash\necho\n"_ksv,                                           KMIME::SH         },
			{ "#! /usr/bin/env -S zsh -f\necho\n"_ksv,                             KMIME::SH         },
			{ "#!/bin/tcsh\necho\n"_ksv,                                           KMIME::CSH        },
			{ "#!/usr/bin/env python3\nprint()\n"_ksv,                             KMIME::PYTHON     },
			{ "#!/usr/bin/env PYTHONPATH=lib python3.12\nprint()\n"_ksv,          KMIME::PYTHON     },
			{ "#!/usr/bin/env node\nconsole.log()\n"_ksv,                         KMIME::JAVASCRIPT },
			// no shell, though the names contain sh
			{ "#!/usr/bin/fish\necho\n"_ksv,                                       KMIME::TEXT_UTF8  },
			{ "#!/opt/shared/bin/perl\nprint\n"_ksv,                               KMIME::TEXT_UTF8  },
		};

		for (const auto& Sample : Samples)
		{
			INFO ( Sample.second );
			CHECK ( kGetMIMETypeOfData(Sample.first) == Sample.second );
		}

		// binary data without a known signature, an image in the ISO media format, and nothing
		for (KStringView sData : { "\x01\x02\x03\x04\x05"_ksv, "\0\0\0\030ftypheic\0\0\0\0"_ksv, ""_ksv })
		{
			CHECK ( kGetMIMETypeOfData(sData) == KMIME::NONE );
		}
	}

	SECTION("kGetMIMETypeOfFile")
	{
		KTempDir Dir;
		auto sName = kFormat("{}/test.json", Dir.Name());
		REQUIRE ( kWriteFile(sName, "{ \"name\": \"test\" }") );

		CHECK ( kGetMIMETypeOfFile(sName) == KMIME::JSON );
		CHECK ( kGetMIMETypeOfFile(kFormat("{}/missing.json", Dir.Name())) == KMIME::NONE );
	}

	SECTION("by inspection, remembered for the extension")
	{
		// with the file command, or by the signature
		KTempDir Dir;
		auto sName = kFormat("{}/test.dekaf2pdf", Dir.Name());
		REQUIRE ( kWriteFile(sName, "%PDF-1.4\n%\xE2\xE3\xCF\xD3\n1 0 obj\n<< >>\nendobj\n") );

		KMIME mime;
		CHECK ( mime.ByExtension("other.dekaf2pdf") == false );
		CHECK ( mime.ByInspection(sName) );
		CHECK ( mime == KMIME::PDF );
		CHECK ( mime.ByExtension("other.dekaf2pdf") );
		CHECK ( mime == KMIME::PDF );
	}

#ifdef DEKAF2_IS_WINDOWS
	SECTION("by extension from the registry")
	{
		// Windows registers image/bmp for .bmp, which the own table does not know
		KMIME mime;
		CHECK ( mime.ByExtension("image.bmp") );
		CHECK ( mime == KMIME::BMP );

		// no type is registered - also when the remembered lookup answers
		CHECK ( mime.ByExtension("file.dekaf2noext", KMIME::BINARY) == false );
		CHECK ( mime == KMIME::BINARY );
		CHECK ( mime.ByExtension("file.dekaf2noext", KMIME::BINARY) == false );
		CHECK ( mime == KMIME::BINARY );
	}
#endif

#ifndef DEKAF2_IS_WINDOWS
	SECTION("by inspection")
	{
		if (kWhich("file").empty())
		{
			WARN("'file' command not found, skipping ByInspection tests");
		}
		else
		{
			KMIME a;

			KTempFile<KOutFile> TempFile("zzz");

			TempFile->Write(R"({"name":"test","short_name":"short_test"})");
			TempFile.Close();

			CHECK( a.ByExtension( "aa.zzz") == false );
			CHECK( a == KMIME::NONE );

			CHECK( a.ByInspection(TempFile.Name()) );

			{
				// a file name with whitespace and quotes - it must reach the file
				// command as one argument
				KTempDir Dir;
				auto sName = kFormat("{}/name with spaces 'and' \"quotes\".zzz2", Dir.Name());
				REQUIRE ( kWriteFile(sName, "<html><head></head><body><p>test</p></body></html>") );
				KMIME b;
				CHECK ( b.ByInspection(sName) );
				CHECK ( b == "text/html" );
			}

			// for older OS/file utility versions json will not be detected,
			// but fail to text/plain

			if (a == KMIME::JSON)
			{
				CHECK( a == KMIME::JSON );

				CHECK( a.ByExtension( "aa.zzz") );
				CHECK( a == KMIME::JSON );

				KTempFile<std::ofstream> TempFile2("zzz");
				KStringView sOut(R"(<html><head></head><body><p>test</p></body></html>)");
				TempFile2->write(sOut.data(), sOut.size());
				TempFile2.Close();

				CHECK( a.ByInspection(TempFile2.Name()) );
				CHECK( a == "text/html" );

				CHECK( a.ByExtension( "aa.zzz") );
				CHECK( a == KMIME::JSON );
			}
			else
			{
				CHECK( a == KMIME::TEXT_PLAIN );
			}
		}
	}
#endif

	SECTION("parts")
	{
		KMIME mime="application/vnd.api+json";

		CHECK ( mime.Type()      == "application" );
		CHECK ( mime.SubType()   == "api"         );
		CHECK ( mime.Tree()      == "vnd"         );
		CHECK ( mime.Suffix()    == "json"        );
		CHECK ( mime.Parameter() == ""            );

		mime="application/vnd.api+json+xml";

		CHECK ( mime.Type()      == "application" );
		CHECK ( mime.SubType()   == "api"         );
		CHECK ( mime.Tree()      == "vnd"         );
		CHECK ( mime.Suffix()    == "json+xml"    );
		CHECK ( mime.Parameter() == ""            );

		mime="application/vnd.api+json+xml;  type=any";

		CHECK ( mime.Type()      == "application" );
		CHECK ( mime.SubType()   == "api"         );
		CHECK ( mime.Tree()      == "vnd"         );
		CHECK ( mime.Suffix()    == "json+xml"    );
		CHECK ( mime.Parameter() == "type=any"    );

		mime="application/vnd.second.api+json+xml;  type=any";

		CHECK ( mime.Type()      == "application" );
		CHECK ( mime.SubType()   == "api"         );
		CHECK ( mime.Tree()      == "vnd.second"  );
		CHECK ( mime.Suffix()    == "json+xml"    );
		CHECK ( mime.Parameter() == "type=any"    );

		mime="application/vnd.api+json+xml;type=any";

		CHECK ( mime.Type()      == "application" );
		CHECK ( mime.SubType()   == "api"         );
		CHECK ( mime.Tree()      == "vnd"         );
		CHECK ( mime.Suffix()    == "json+xml"    );
		CHECK ( mime.Parameter() == "type=any"    );

		mime = "application/vnd.openxmlformats-officedocument.wordprocessingml.document";

		CHECK ( mime.Type()      == "application" );
		CHECK ( mime.SubType()   == "document"    );
		CHECK ( mime.Tree()      == "vnd.openxmlformats-officedocument.wordprocessingml" );
		CHECK ( mime.Suffix()    == ""            );
		CHECK ( mime.Parameter() == ""            );

		mime = "image/svg+xml";

		CHECK ( mime.Type()      == "image"       );
		CHECK ( mime.SubType()   == "svg"         );
		CHECK ( mime.Tree()      == ""            );
		CHECK ( mime.Suffix()    == "xml"         );
		CHECK ( mime.Parameter() == ""            );

		mime = "text/html; charset=UTF-8";

		CHECK ( mime.Type()      == "text"        );
		CHECK ( mime.SubType()   == "html"        );
		CHECK ( mime.Tree()      == ""            );
		CHECK ( mime.Suffix()    == ""            );
		CHECK ( mime.Parameter() == "charset=UTF-8" );

		mime = "application/x-www-form-urlencoded";

		CHECK ( mime.Type()      == "application" );
		CHECK ( mime.SubType()   == "x-www-form-urlencoded");
		CHECK ( mime.Tree()      == ""            );
		CHECK ( mime.Suffix()    == ""            );
		CHECK ( mime.Parameter() == ""            );

		mime = "text/sgml.html+xml; charset=UTF-8";

		CHECK ( mime.Type()      == "text"        );
		CHECK ( mime.SubType()   == "html"        );
		CHECK ( mime.Tree()      == "sgml"        );
		CHECK ( mime.Suffix()    == "xml"         );
		CHECK ( mime.Parameter() == "charset=UTF-8" );
	}

// C++11 has problems destroying the KMIMEMultiPartFormData ..
#if DEKAF2_HAS_CPP_14
	SECTION("KMIMEMultiPart")
	{
		KString sRegex = "KMIME=_l[0-9]+_[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}";

		KMIMEMultiPartFormData Parts;
		Parts += KMIMEText("TheName", "TheValue");
		Parts += KMIMEFile("", "This is file1 Русский\nWith line2\n", "file1.txt", KMIME::TEXT_UTF8);
		Parts += KMIMEFile("", "This is file2 Русский\nwith line2\n", "file2.jpg");
		Parts += KMIMEPart(KJSON
						   {
							{ "message", "important"               },
							{ "parts"  , { "one", "two", "three" } }
						   }.dump(),
						   KMIME::JSON);

		// serialize for SMTP
		auto sFormData = Normalized(Parts.Serialize(false));
		sFormData.ReplaceRegex(sRegex, "KMIME=_l[LEVEL]_[UUID]");
		KString sExpected1 = Normalized(R"(Content-Type: multipart/form-data;
 boundary="KMIME=_l[LEVEL]_[UUID]"

--KMIME=_l[LEVEL]_[UUID]
Content-Type: text/plain; charset=UTF-8
Content-Transfer-Encoding: quoted-printable
Content-Disposition: form-data; name="TheName"

TheValue
--KMIME=_l[LEVEL]_[UUID]
Content-Type: text/plain; charset=UTF-8
Content-Transfer-Encoding: quoted-printable
Content-Disposition: form-data; filename="file1.txt"

This is file1 =D0=A0=D1=83=D1=81=D1=81=D0=BA=D0=B8=D0=B9
With line2

--KMIME=_l[LEVEL]_[UUID]
Content-Type: image/jpeg
Content-Transfer-Encoding: base64
Content-Disposition: form-data; filename="file2.jpg"

VGhpcyBpcyBmaWxlMiDQoNGD0YHRgdC60LjQuQp3aXRoIGxpbmUyCg==
--KMIME=_l[LEVEL]_[UUID]
Content-Type: application/json
Content-Transfer-Encoding: base64
Content-Disposition: inline

eyJtZXNzYWdlIjoiaW1wb3J0YW50IiwicGFydHMiOlsib25lIiwidHdvIiwidGhyZWUiXX0=
--KMIME=_l[LEVEL]_[UUID]--
)");
		CHECK ( sFormData == sExpected1 );

		// serialize for HTTP
		sFormData = Normalized(Parts.Serialize(true));
		KString sContentType = Parts.ContentType().Serialize();
		KString sExpectedContentType = Normalized((R"(multipart/form-data; boundary="KMIME=_l[LEVEL]_[UUID]")"));
		KString sExpected2 = Normalized(R"(--KMIME=_l[LEVEL]_[UUID]
Content-Type: text/plain; charset=UTF-8
Content-Disposition: form-data; name="TheName"

TheValue
--KMIME=_l[LEVEL]_[UUID]
Content-Type: text/plain; charset=UTF-8
Content-Disposition: form-data; filename="file1.txt"

This is file1 Русский
With line2

--KMIME=_l[LEVEL]_[UUID]
Content-Type: image/jpeg
Content-Disposition: form-data; filename="file2.jpg"

This is file2 Русский
with line2

--KMIME=_l[LEVEL]_[UUID]
Content-Type: application/json
Content-Disposition: inline

{"message":"important","parts":["one","two","three"]}
--KMIME=_l[LEVEL]_[UUID]--
)");
		sContentType.ReplaceRegex(sRegex, "KMIME=_l[LEVEL]_[UUID]");
		sFormData   .ReplaceRegex(sRegex, "KMIME=_l[LEVEL]_[UUID]");
		CHECK ( sContentType == sExpectedContentType );
		CHECK ( sFormData    == sExpected2 );
	}
#endif // (CPP 11)
}
