#include "catch.hpp"

#include <dekaf2/kdiff.h>
#include <dekaf2/kencode.h>

using namespace dekaf2;

TEST_CASE("KDiff")
{
	SECTION("KDiffToASCII")
	{
		KStringView sText1    { "This is a fat cat"   };
		KStringView sText2    { "That is a black hat" };
		KStringView sExpected { "Th[-is][+at] is a [-fat c][+black h]at" };

		auto sDiff = KDiffToASCII(sText1, sText2);
		CHECK ( sDiff == sExpected );
	}

	SECTION("KDiffToHTML")
	{
		KStringView sText1    { "This is a fat cat"   };
		KStringView sText2    { "That is a black hat" };
		KStringView sExpected { "Th<del>is</del><ins>at</ins> is a <del>fat c</del><ins>black h</ins>at" };

		auto sDiff = KDiffToHTML(sText1, sText2);
		CHECK ( sDiff == sExpected );
	}

	SECTION("KDiffToASCII UTF8 1")
	{
		KStringView sText1    { "Gehäusegröße und tränenüberströmte Geräteüberhohung mit Gänsefüßchen der Ölüberschusslaender" };
		KStringView sText2    { "Gehausegrosse wer tränenüberströmte Gerateüberhöhung mit Gänsefuesschen der Ölüberschussländer" };
		KStringView sExpected { "Geh[-ä][+a]usegr[-öße und][+osse wer] tränenüberströmte Ger[-ä][+a]teüberh[-o][+ö]hung mit Gänsef[-üß][+uess]chen der Ölüberschussl[-ae][+ä]nder" };

		auto sDiff = KDiffToASCII(sText1, sText2);
		CHECK ( sDiff == sExpected );
	}

	SECTION("KDiffToASCII UTF8 2")
	{
		KStringView sText1    { "Dès Noël où un zéphyr haï me vêt de glaçons würmiens, je dine d’exquis rôtis de bœuf au kir à l’aÿ d’âge mur & cetera !" };
		KStringView sText2    { "Des Noel ou un zéphyr hai me vet de glacons würmiens, je dîne d’exquis rôtis de bœuf au kir à l’ay d’age mûr & cætera !" };
		KStringView sExpected { "D[-è][+e]s No[-ë][+e]l o[-ù][+u] un zéphyr ha[-ï][+i] me v[-ê][+e]t de gla[-ç][+c]ons würmiens, je d[-i][+î]ne d’exquis rôtis de bœuf au kir à l’a[-ÿ][+y] d’[-â][+a]ge m[-u][+û]r & c[-e][+æ]tera !" };

		auto sDiff = KDiffToASCII(sText1, sText2);
		CHECK ( sDiff == sExpected );
	}

	SECTION("KDiffToASCII2")
	{
		KString sText1    { "This is a fat cat"   };
		KString sText2    { "That is a black hat" };
		KStringView sExpected1 { "Th[-is] is a [-fat c]at" };
		KStringView sExpected2 { "Th[+at] is a [+black h]at" };

		auto iCount = KDiffToASCII2(sText1, sText2);
		CHECK ( iCount == 4  );
		CHECK ( sText1 == sExpected1 );
		CHECK ( sText2 == sExpected2 );
	}

	SECTION("KDiffToHTML2")
	{
		KString sText1    { "This is a fat cat"   };
		KString sText2    { "That is a black hat" };
		KStringView sExpected1 { "Th<del>is</del> is a <del>fat c</del>at" };
		KStringView sExpected2 { "Th<ins>at</ins> is a <ins>black h</ins>at" };

		auto iCount = KDiffToHTML2(sText1, sText2);
		CHECK ( iCount == 4  );
		CHECK ( sText1 == sExpected1 );
		CHECK ( sText2 == sExpected2 );
	}

	SECTION("GetUnifiedDiff")
	{
		KStringView sText1    { "This is a fat cat"   };
		KStringView sText2    { "That is a black hat" };
		KStringView sExpected = R"(@@ -1,17 +1,19 @@
 Th
-is
+at
  is a
-fat c
+black h
 at
)";

		KDiff Differ(sText1, sText2);

		auto sDiff = Differ.GetUnifiedDiff();
		// the editor deletes all trailing spaces in sExpected.., remove them in the diff as well
		sDiff.Replace(" \n", "\n");
		CHECK ( sDiff == sExpected );
		CHECK ( Differ.GetLevenshteinDistance() == 9 );
	}

	SECTION("GetUnifiedDiff line/character mode")
	{
		KStringView sText1    { "First line\nThis is a fat cat\nAnother line"   };
		KStringView sText2    { "First line\nThat is a black hat\nAnother line" };
		KStringView sExpected = R"(@@ -10,21 +10,23 @@
 e%0ATh
-is
+at
  is a
-fat c
+black h
 at%0AA
)";

		KDiff Differ(sText1, sText2, KDiff::DiffMode::LineThenCharacter);

		auto sDiff = Differ.GetUnifiedDiff();
		// the editor deletes all trailing spaces in sExpected.., remove them in the diff as well
		sDiff.Replace(" \n", "\n");
		CHECK ( sDiff == sExpected );
		CHECK ( Differ.GetLevenshteinDistance() == 9 );
	}

	SECTION("GetUnifiedDiff line mode")
	{
		KStringView sText1    { "First line\nThis is a fat cat\nAnother line"   };
		KStringView sText2    { "First line\nThat is a black hat\nAnother line" };
		KStringView sExpected = R"(@@ -10,21 +10,23 @@
 e%0ATh
-is
+at
  is a
-fat c
+black h
 at%0AA
)";

		KDiff Differ(sText1, sText2, KDiff::DiffMode::Line);

		auto sDiff = Differ.GetUnifiedDiff();
		// the editor deletes all trailing spaces in sExpected.., remove them in the diff as well
		sDiff.Replace(" \n", "\n");
		CHECK ( sDiff == sExpected );
		CHECK ( Differ.GetLevenshteinDistance() == 9 );
	}
}
