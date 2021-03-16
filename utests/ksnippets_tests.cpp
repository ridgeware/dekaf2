#include "catch.hpp"
#include <dekaf2/ksnippets.h>
#include <vector>

using namespace dekaf2;

TEST_CASE("KSnippets")
{
	SECTION("Basics")
	{
		constexpr KStringView sInput =
R"(
{{Start
<hmtl>
<head>
<title>Test</title>
</head>
<body>
}}

{{Link
<a href="{{Image}}" title="{{Name}}">{{LinkText}}</a>
}}{{End </body>}}
)";

		constexpr KStringView sExpected =
R"(<hmtl>
<head>
<title>Test</title>
</head>
<body>
<a href="TheImage.png" title="The Image">Image One</a>
</body>)";

		KTempDir TempDir;
		auto sFilename = kFormat("{}{}{}", TempDir.Name(), kDirSep, "test.file");

		CHECK ( kWriteFile(sFilename, sInput) == true );

		KVariables Var;
		Var.insert("Image"    , "TheImage.png");
		Var.insert("Name"     , "The Image");
		Var.insert("LinkText" , "Image One");

		KSnippets Snippets;
		KString sOut;
		Snippets.SetOutput(sOut);

		if (!Snippets.AddFile(sFilename))
		{
			Snippets += sInput;
		}

		Snippets.Compose("Start");
		Snippets("Link", Var);
		Snippets("End");

		CHECK ( sOut == sExpected );

	}
}
