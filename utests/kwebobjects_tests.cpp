#include "catch.hpp"

#include <dekaf2/web/objects/kwebobjects.h>

using namespace dekaf2;

extern KString print_diff(KStringView s1, KStringView s2);

// This TEST_CASE both verifies and illustrates idiomatic KWebObjects
// usage with class-template-argument-deduction in `KHTMLNode::Add<T>()`
// (the bare `Add<html::RadioButton>(arg)` form). CTAD is a C++17
// feature (`__cpp_deduction_guides >= 201606`). On older toolchains
// (notably gcc 6 on Debian stretch, which is C++14-only) the test is
// skipped at the preprocessor level — the library still works with
// the explicit-spelling form (`Add<html::RadioButton<KString>>(arg)`),
// but the test prefers the modern shape to keep the example readable.
#if defined(__cpp_deduction_guides) && __cpp_deduction_guides >= 201606L

TEST_CASE("KWebObjects")
{

	SECTION("main")
	{
		static constexpr KStringView sSerialized = R"(<!DOCTYPE html>
<html lang="en">
	<head>
		<title>
			My First Web Page
		</title>
		<style>
body {
font-family: Verdana, sans-serif; background-color: powderblue
}

.nodecoration {
text-decoration: none
}

</style>
		<script charset="utf-8">
			function test()
			{
				return "test";
			}
		</script>
	</head>
	<body>
		<p>
			<img alt="this is a test image" class=".nodecoration" loading="lazy" src="http://some.image.url/at/a/path.png"><br>
			<img alt="this is a test image" class=".nodecoration" loading="lazy" src="http://some.image.url/at/a/path.png">
		</p>
		<div class=".nodecoration" id="FormDiv">
			<p>
				<form accept-charset="utf-8" action="MyForm">
					<button name="q" type="submit" value="button1">
						Schaltfläche 1
					</button><button name="q" type="submit" value="button2">
						Schaltfläche 2
					</button><button formenctype="multipart/form-data" formmethod="post" name="q" type="submit" value="button3">
						Schaltfläche 3
					</button>
				</form>
			</p>
			<p class=".nodecoration" id="TextPar">
				hello world<a href="/images/test.png"><img alt="preview" id="IMGID" src="/Images/test_small.png"></a>
			</p>
			<form accept-charset="utf-8" action="/Setup/">
				<fieldset>
					<legend>
						System Setup
					</legend>
					<label><input name="datadir" size="60" type="text" value="abcdef">shared data directory</label><br>
					<label><input name="outputdir" size="60" type="text">shared data directory</label>
					<div hidden></div>
					<div hidden></div>
				</fieldset>
				<fieldset>
					<legend>
						Email
					</legend>
					<label><input name="_fobj1" size="30" type="text">target email</label><br>
					<label><input name="mailfrom" size="30" type="text">sender email</label><br>
					<label><input name="_fobj2" size="30" type="text">smtp relay</label>
				</fieldset>
				<fieldset>
					<legend>
						Display
					</legend>
					<label><input max="0.99" min="0.01" name="confidence" step="0.025" type="number" value="0.55">confidence value, from 0.0 .. 1.0</label><br>
					<label><input id="maxconfid" max="0.99" min="0.01" name="maxconfidence" oninput="output1.value = Math.round(maxconfid.valueAsNumber * 100.0) + '%';" step="0.025" type="range" value="0.99">max confidence value, from 0.0 .. 1.0</label><output id="output1" name="output">0%</output><br>
					<label><input name="nodetect" type="checkbox">no object detection</label><br>
					<label><input checked name="nodisplay" type="checkbox">do not open camera windows</label><br>
					<label><input name="nooverlay" type="checkbox">no detection overlay</label><br>
					<label><input name="motion" type="checkbox">show motion</label><label><input max="1000" min="-1" name="_fobj3" step="1" type="number" value="1000">motion area</label><br>
					<label><input max="86400" min="1" name="interval" type="number" value="60">seconds between alarms</label><label><select name="selection" size="1">
							<option value="_fobj4">
								Sel1
							</option>
							<option value="_fobj5">
								Sel2
							</option>
							<option value="_fobj6">
								Sel3
							</option>
							<option selected value="_fobj7">
								Sel4
							</option>
						</select>please choose a value</label>
				</fieldset>
				<fieldset>
					<legend>
						Radios
					</legend>
					<label><input name="_fobj8" type="radio" value="red">red</label><label><input name="_fobj8" type="radio" value="green">green</label><label><input name="_fobj8" type="radio" value="blue">blue</label><label><input name="_fobj8" type="radio" value="yellow">yellow</label>
				</fieldset>
				<fieldset>
					<legend>
						Radios with enums
					</legend>
					<label><input name="_fobj9" type="radio" value="0">red</label><label><input name="_fobj9" type="radio" value="1">green</label><label><input name="_fobj9" type="radio" value="2">blue</label><label><input checked name="_fobj9" type="radio" value="3">yellow</label>
				</fieldset>
			</form>
		</div>
		<a href="link" title="title"><img alt="AltText" loading="lazy" src="another/link"></a>
		<a href="link" title="title"><img alt="AltText" loading="lazy" src="another/link"></a>
		<table>
			<tr>
				<td>
					&lt;d1&gt;
				</td>
				<td>
					d2
				</td>
				<td>
					d3
				</td>
			</tr>
		</table>
	</body>
</html>
)";
		enum COLOR { RED, GREEN, BLUE, YELLOW };

		struct Config
		{
			KString sDataDir { "this/is/preset" };
			KString sOutputDir;
			KString sMailTo;
			KString sMailFrom;
			KString sSMTPServer;
			KString sRadio     { "blue" };
			KString sSelection { "Sel2" };
			int     iMotionArea = 0;
			COLOR   Color    { GREEN };
			std::chrono::high_resolution_clock::duration dInterval { 0 };
			double m_fMinConfidence { 0.0 };
			double m_fMaxConfidence { 0.0 };
			bool bNoDetect   { false };
			bool bNoDisplay  { false };
			bool bNoOverlay  { false };
			bool bShowMotion { false };
		};

		Config m_Config;

		url::KQueryParms Parms;
		Parms.Add("datadir"   , "abcdef");
		Parms.Add("mailto"    , "xyz"   );
		Parms.Add("nodisplay" , "on"    );
		Parms.Add("interval"  , "60"    );
		Parms.Add("confidence", "0.55"  );
		Parms.Add("maxconfidence", "0,99"); // try different decimal point
		Parms.Add("_fobj3"    , "1001"  );  // this value is too large for the object (max = 1000)
		Parms.Add("selection" , "_fobj7");
		Parms.Add("_fobj9"    , "3"     );

		html::Page page("My First Web Page", "en");

		html::Class Class("body", "font-family: Verdana, sans-serif; background-color: powderblue");
		page.AddClass(Class);

		html::Class NoDecoration(".nodecoration", "text-decoration: none");
		page.AddClass(NoDecoration);

		page.Head().Add<html::Script>(KStringView(R"(
			function test()
			{
				return "test";
			}
		)"), "utf-8");

		// Helper to inject the image lazily into a parent — replaces the
		// pre-Phase-4 detached-build idiom (one html::Image used twice).
		auto AddImage = [&NoDecoration](KHTMLNode where)
		{
			where.Add<html::Image>("http://some.image.url/at/a/path.png",
			                       "this is a test image",
			                       NoDecoration)
			     .SetLoading(html::Image::LAZY);
		};

		auto body = page.Body();
		{
			auto par = body.Add<html::Paragraph>();
			AddImage(par);
			par.Add<html::Break>();
			AddImage(par);
		}

		auto div = body.Add<html::Div>(NoDecoration, "FormDiv");

		{
			auto par  = div.Add<html::Paragraph>();
			auto form = par.Add<html::Form>("MyForm");
			form.Add<html::Button>("Schaltfläche 1").SetName("q").SetValue("button1");
			form.Add<html::Button>("Schaltfläche 2").SetName("q").SetValue("button2");
			form.Add<html::Button>("Schaltfläche 3").SetName("q").SetValue("button3")
				.SetFormMethod(html::Button::POST).SetFormEncType(html::Button::FORMDATA);
		}

		{
			auto par = div.Add<html::Paragraph>(NoDecoration, "TextPar");
			par.AddText("hello world");
			auto link = par.Add<html::Link>("/images/test.png");
			link.Add<html::Image>("/Images/test_small.png", "preview", "", "IMGID");
		}

		{
			auto form = div.Add<html::Form>("/Setup/");

			kDebug(2, "start");
			{
				auto group = form.Add<html::FieldSet>("System Setup");
				group.Add<html::TextInput>(m_Config.sDataDir, "datadir")
				     .SetLabelAfter("shared data directory").SetSize(60);
				group.Add<html::Break>();
				auto ti = group.Add<html::TextInput>(m_Config.sOutputDir, "outputdir");
				ti.SetLabelAfter("shared data directory").SetSize(60);
				CHECK ( ti.Serialize().size() == 82 );
				group.Add<html::Div>().SetHidden(true);
				auto div2 = group.Add<html::Div>();
				div2.SetHidden(true);
				CHECK ( div2.Serialize().size() == 20 );
			}

			{
				auto group = form.Add<html::FieldSet>("Email");
				group.Add<html::TextInput>(m_Config.sMailTo                                  ).SetLabelAfter("target email").SetSize(30);
				group.Add<html::Break>();
				group.Add<html::TextInput>(m_Config.sMailFrom   , "mailfrom"   ).SetLabelAfter("sender email").SetSize(30);
				group.Add<html::Break>();
				group.Add<html::TextInput>(m_Config.sSMTPServer                              ).SetLabelAfter("smtp relay" ).SetSize(30);
			}

			{
				auto group = form.Add<html::FieldSet>("Display");
				group.Add<html::NumericInput>(m_Config.m_fMinConfidence, "confidence")
				     .SetLabelAfter("confidence value, from 0.0 .. 1.0").SetRange(0.01, 0.99).SetStep(0.0250);
				group.Add<html::Break>();
				group.Add<html::NumericInput>(m_Config.m_fMaxConfidence, "maxconfidence", "", "maxconfid")
				     .SetLabelAfter("max confidence value, from 0.0 .. 1.0")
				     .SetRange(0.01, 0.99)
				     .SetStep(0.0250)
				     .SetType(html::Input::INPUTTYPE::RANGE)
				     .SetAttribute("oninput", "output1.value = Math.round(maxconfid.valueAsNumber * 100.0) + '%';");
				group.Add<html::Output>("output",
				                        KStringView(kFormat("{}%", std::round(m_Config.m_fMaxConfidence * 100.0))),
				                        "",
				                        "output1");
				group.Add<html::Break>();
				group.Add<html::CheckBox>(m_Config.bNoDetect   , "nodetect" ).SetLabelAfter("no object detection"       );
				group.Add<html::Break>();
				group.Add<html::CheckBox>(m_Config.bNoDisplay  , "nodisplay").SetLabelAfter("do not open camera windows");
				group.Add<html::Break>();
				group.Add<html::CheckBox>(m_Config.bNoOverlay  , "nooverlay").SetLabelAfter("no detection overlay"      );
				group.Add<html::Break>();
				group.Add<html::CheckBox>(m_Config.bShowMotion , "motion"   ).SetLabelAfter("show motion"               );
				group.Add<html::NumericInput>(m_Config.iMotionArea).SetLabelAfter("motion area").SetMin(-1).SetMax(1000).SetStep(1);
				group.Add<html::Break>();
				group.Add<html::DurationInput>(m_Config.dInterval, "interval")
				     .SetLabelAfter("seconds between alarms").SetRange(1, 86400);
				group.Add<html::Selection>(m_Config.sSelection)
				     .SetName("selection")
				     .SetOptions("Sel1,Sel2,Sel3,Sel4")
				     .SetLabelAfter("please choose a value");
			}

			{
				auto group = form.Add<html::FieldSet>("Radios");
				group.Add<html::RadioButton>(m_Config.sRadio).SetValue("red"   ).SetLabelAfter("red"   );
				group.Add<html::RadioButton>(m_Config.sRadio).SetValue("green" ).SetLabelAfter("green" );
				group.Add<html::RadioButton>(m_Config.sRadio).SetValue("blue"  ).SetLabelAfter("blue"  );
				group.Add<html::RadioButton>(m_Config.sRadio).SetValue("yellow").SetLabelAfter("yellow");
			}

			{
				auto group = form.Add<html::FieldSet>("Radios with enums");
				group.Add<html::RadioButton>(m_Config.Color).SetValue(RED   ).SetLabelAfter("red"   );
				group.Add<html::RadioButton>(m_Config.Color).SetValue(GREEN ).SetLabelAfter("green" );
				group.Add<html::RadioButton>(m_Config.Color).SetValue(BLUE  ).SetLabelAfter("blue"  );
				group.Add<html::RadioButton>(m_Config.Color).SetValue(YELLOW).SetLabelAfter("yellow");
			}

			page.Generate();
			page.Synchronize(Parms);

			auto AddOuterLink = [](KHTMLNode where)
			{
				auto link = where.Add<html::Link>("link");
				link.SetTitle("title");
				link.Add<html::Image>("another/link", "AltText").SetLoading(html::Image::LAZY);
			};

			AddOuterLink(body);
			body.Add<html::LineBreak>();
			AddOuterLink(body);
			body.Add<html::LineBreak>();

			auto table = body.Add<html::Table>();
			auto row1  = table.Add<html::TableRow>();
			row1.Add<html::TableData>().AddText("<d1>");
			row1.Add<html::TableData>().AddText("d2");
			row1.Add<html::TableData>().AddText("d3");
		}

		KString sCRLF = page.Print();
		sCRLF.Replace("\r\n", "\n");

		CHECK( sCRLF == sSerialized );
		CHECK( m_Config.m_fMinConfidence == 0.55 );
		CHECK( m_Config.m_fMaxConfidence == 0.99 );
		CHECK( m_Config.bNoDisplay       == true );
		CHECK( m_Config.iMotionArea      == 1000 );

		auto sDiff = print_diff(sCRLF, sSerialized);

		if (!sDiff.empty())
		{
			FAIL_CHECK ( sDiff );
		}

	}
}

#endif // __cpp_deduction_guides

// Focused test for html::RadioButton — verifies that:
//   1. four radios sharing one backing variable get the same auto-assigned
//      `name` attribute after Page::Generate(),
//   2. Page::Synchronize() flips `checked` to the radio whose `value` matches
//      the supplied query-parm value (and removes it from the others),
//   3. the backing variable is updated to the query-parm value,
//   4. the same works for an enum-typed backing variable.
//
// Uses explicit template spelling so the test runs on C++14 too (no CTAD).
TEST_CASE("KWebObjects.RadioButton")
{
	SECTION("KString-backed group — Synchronize selects matching radio")
	{
		KString sChoice { "blue" };

		html::Page page("Radio Test");
		auto form = page.Body().Add<html::Form>("/submit");

		auto group = form.Add<html::FieldSet>("Colour");
		group.Add<html::RadioButton<KString>>(sChoice).SetValue("red"   ).SetLabelAfter("red"   );
		group.Add<html::RadioButton<KString>>(sChoice).SetValue("green" ).SetLabelAfter("green" );
		group.Add<html::RadioButton<KString>>(sChoice).SetValue("blue"  ).SetLabelAfter("blue"  );
		group.Add<html::RadioButton<KString>>(sChoice).SetValue("yellow").SetLabelAfter("yellow");

		page.Generate();   // auto-assign names — all four share one `name`

		// All four radios should share the same auto-assigned `name`
		// attribute (because they share one backing variable).
		auto sPreSync = page.Serialize();
		INFO( sPreSync );
		auto iName1 = sPreSync.find("name=\"");
		auto iName2 = sPreSync.find("name=\"", iName1 + 1);
		REQUIRE( iName1 != KString::npos );
		REQUIRE( iName2 != KString::npos );
		auto sName1 = sPreSync.substr(iName1, sPreSync.find('"', iName1 + 6) - iName1 + 1);
		auto sName2 = sPreSync.substr(iName2, sPreSync.find('"', iName2 + 6) - iName2 + 1);
		CHECK( sName1 == sName2 );

		// Now pretend a form-submission picks "red".
		url::KQueryParms Parms;
		Parms.Add(KStringView{sName1}.substr(6, sName1.size() - 7),   // strip name=" ... "
		          "red");
		page.Synchronize(Parms);

		// Backing variable updated to the submitted value.
		CHECK( sChoice == "red" );

		auto sOut = page.Serialize();
		INFO( sOut );
		// Exactly one radio is checked, and it's the red one.
		std::size_t iCheckedCount = 0;
		KStringView v{sOut};
		for (auto pos = v.find("checked"); pos != KStringView::npos;
		     pos = v.find("checked", pos + 1))
		{
			++iCheckedCount;
		}
		CHECK( iCheckedCount == 1 );
		// Confirm the checked one sits next to value="red".
		auto iRed = sOut.find(R"(value="red")");
		REQUIRE( iRed != KString::npos );
		auto sNearRed = sOut.substr(iRed > 50 ? iRed - 50 : 0,
		                            sOut.find('>', iRed) - (iRed > 50 ? iRed - 50 : 0) + 1);
		CHECK( sNearRed.contains("checked") );
	}

	SECTION("initial backing-variable value is reflected in `checked`")
	{
		// Mirror of how CheckBox treats its initial state: the radio whose
		// `value` matches the backing variable should be initially checked
		// on first render (before any Synchronize). This currently FAILS —
		// kept here as the regression marker for the missing initial-state
		// hookup in RadioButton's ctor.
		KString sChoice { "blue" };

		html::Page page("Radio Initial Test");
		auto form  = page.Body().Add<html::Form>("/submit");
		auto group = form.Add<html::FieldSet>("Colour");
		group.Add<html::RadioButton<KString>>(sChoice).SetValue("red"  ).SetLabelAfter("red"  );
		group.Add<html::RadioButton<KString>>(sChoice).SetValue("green").SetLabelAfter("green");
		group.Add<html::RadioButton<KString>>(sChoice).SetValue("blue" ).SetLabelAfter("blue" );

		page.Generate();   // just assigns names — no Synchronize

		auto sOut = page.Serialize();
		INFO( sOut );
		// Exactly one radio (the blue one) should be checked, reflecting
		// the initial sChoice = "blue".
		std::size_t iCheckedCount = 0;
		KStringView v{sOut};
		for (auto pos = v.find("checked"); pos != KStringView::npos;
		     pos = v.find("checked", pos + 1))
		{
			++iCheckedCount;
		}
		CHECK( iCheckedCount == 1 );

		// And it should be the blue one.
		auto iBlue = sOut.find(R"(value="blue")");
		REQUIRE( iBlue != KString::npos );
		auto iTagStart = sOut.rfind('<', iBlue);
		auto iTagEnd   = sOut.find ('>', iBlue);
		REQUIRE( iTagStart != KString::npos );
		REQUIRE( iTagEnd   != KString::npos );
		auto sTag = sOut.substr(iTagStart, iTagEnd - iTagStart + 1);
		CHECK( sTag.contains("checked") );
	}

	SECTION("enum-backed initial state — value passed to SetValue matches backing variable")
	{
		enum Colour { RED = 0, GREEN = 1, BLUE = 2, YELLOW = 3 };
		Colour eChoice { GREEN };

		html::Page page("Enum Radio Initial Test");
		auto form  = page.Body().Add<html::Form>("/submit");
		auto group = form.Add<html::FieldSet>("Colour");
		group.Add<html::RadioButton<Colour>>(eChoice).SetValue(RED  ).SetLabelAfter("red");
		group.Add<html::RadioButton<Colour>>(eChoice).SetValue(GREEN).SetLabelAfter("green");
		group.Add<html::RadioButton<Colour>>(eChoice).SetValue(BLUE ).SetLabelAfter("blue");

		page.Generate();

		auto sOut = page.Serialize();
		INFO( sOut );
		std::size_t iCheckedCount = 0;
		KStringView v{sOut};
		for (auto pos = v.find("checked"); pos != KStringView::npos;
		     pos = v.find("checked", pos + 1))
		{
			++iCheckedCount;
		}
		CHECK( iCheckedCount == 1 );
		auto iGreen = sOut.find(R"(value="1")");
		REQUIRE( iGreen != KString::npos );
		auto iTagStart = sOut.rfind('<', iGreen);
		auto iTagEnd   = sOut.find ('>', iGreen);
		REQUIRE( iTagStart != KString::npos );
		REQUIRE( iTagEnd   != KString::npos );
		auto sTag = sOut.substr(iTagStart, iTagEnd - iTagStart + 1);
		CHECK( sTag.contains("checked") );
	}

	SECTION("enum-backed group — Synchronize picks matching enum value")
	{
		enum Colour { RED = 0, GREEN = 1, BLUE = 2, YELLOW = 3 };
		Colour eChoice { BLUE };

		html::Page page("Enum Radio Test");
		auto form  = page.Body().Add<html::Form>("/submit");
		auto group = form.Add<html::FieldSet>("Colour");
		group.Add<html::RadioButton<Colour>>(eChoice).SetValue(RED   ).SetLabelAfter("red");
		group.Add<html::RadioButton<Colour>>(eChoice).SetValue(GREEN ).SetLabelAfter("green");
		group.Add<html::RadioButton<Colour>>(eChoice).SetValue(BLUE  ).SetLabelAfter("blue");
		group.Add<html::RadioButton<Colour>>(eChoice).SetValue(YELLOW).SetLabelAfter("yellow");

		page.Generate();
		auto sPre = page.Serialize();
		auto iName = sPre.find("name=\"");
		REQUIRE( iName != KString::npos );
		auto sNameQuoted = sPre.substr(iName, sPre.find('"', iName + 6) - iName + 1);
		KString sName(KStringView{sNameQuoted}.substr(6, sNameQuoted.size() - 7));

		url::KQueryParms Parms;
		Parms.Add(sName, "1");   // GREEN
		page.Synchronize(Parms);

		CHECK( eChoice == GREEN );

		auto sOut = page.Serialize();
		std::size_t iCheckedCount = 0;
		KStringView v{sOut};
		for (auto pos = v.find("checked"); pos != KStringView::npos;
		     pos = v.find("checked", pos + 1))
		{
			++iCheckedCount;
		}
		CHECK( iCheckedCount == 1 );
		auto iGreen = sOut.find(R"(value="1")");
		REQUIRE( iGreen != KString::npos );
	}
}

// CheckBox parallels RadioButton's test. The pre-Phase-4 CheckBox kept a
// `Boolean& m_bValue` reference member; the arena version replaced it with
// `khtml::InteractiveBinding::pResult` (a `void*` into the document's
// side-map), which is functionally equivalent — the binding holds the
// address of the caller's variable, and pfnSync/pfnReset mutate through it.
// These four sections lock that equivalence in.
TEST_CASE("KWebObjects.CheckBox")
{
	SECTION("initial true — `checked` attribute is emitted")
	{
		bool bFlag { true };

		html::Page page("CheckBox Initial True");
		page.Body().Add<html::CheckBox<bool>>(bFlag, "flag").SetLabelAfter("on");

		auto sOut = page.Serialize();
		INFO( sOut );
		CHECK( sOut.contains("checked") );
	}

	SECTION("initial false — no `checked` attribute")
	{
		bool bFlag { false };

		html::Page page("CheckBox Initial False");
		page.Body().Add<html::CheckBox<bool>>(bFlag, "flag").SetLabelAfter("on");

		auto sOut = page.Serialize();
		INFO( sOut );
		CHECK_FALSE( sOut.contains("checked") );
	}

	SECTION("Synchronize with matching parm — variable becomes true, checked emitted")
	{
		bool bFlag { false };

		html::Page page("CheckBox Sync Set");
		page.Body().Add<html::CheckBox<bool>>(bFlag, "flag").SetLabelAfter("on");
		page.Generate();

		url::KQueryParms Parms;
		Parms.Add("flag", "on");
		page.Synchronize(Parms);

		CHECK( bFlag == true );
		auto sOut = page.Serialize();
		CHECK( sOut.contains("checked") );
	}

	SECTION("Synchronize without parm — variable becomes false (form-submit semantics)")
	{
		// HTML form-submit convention: unchecked checkboxes don't submit
		// their name at all. So `Synchronize` with no matching parm is
		// the signal that the checkbox is unchecked — clear the variable.
		bool bFlag { true };

		html::Page page("CheckBox Sync Reset");
		page.Body().Add<html::CheckBox<bool>>(bFlag, "flag").SetLabelAfter("on");
		page.Generate();

		url::KQueryParms Parms;
		Parms.Add("unrelated", "x");
		page.Synchronize(Parms);

		CHECK( bFlag == false );
		auto sOut = page.Serialize();
		CHECK_FALSE( sOut.contains("checked") );
	}
}

// Regression test for the C++ overload-resolution pitfall:
//   SetAttribute("onchange", "setSomeParam(this.value)")
// Without SFINAE on the bool overload, the string literal (const char*)
// would match the bool overload via the standard pointer-to-bool
// conversion (which outranks the user-defined conversion to KStringView),
// silently emitting `onchange` as a bare boolean attribute instead of the
// JS handler. SetAttribute's bool overload is now constrained to *exactly*
// bool via SFINAE so string literals fall through to the KStringView path.
TEST_CASE("KWebObjects.SetAttribute overload resolution")
{
	html::Page page("overload test");
	auto p = page.Body().Add<html::Paragraph>();

	SECTION("string literal goes to KStringView overload, not bool")
	{
		p.SetAttribute("onchange", "setSomeParam(this.value)");
		auto sOut = page.Serialize();
		CHECK      ( sOut.contains(R"JS(onchange="setSomeParam(this.value)")JS") );
		CHECK_FALSE( sOut.contains(R"(onchange="true")")                         );
	}

	SECTION("exact bool still hits the boolean-attribute overload")
	{
		// HTML5 boolean attribute (in the s_BooleanAttributes table):
		// gets emitted bare on `true`, removed on `false`.
		p.SetAttribute("hidden", true);
		// Non-HTML5-boolean: emits `name="true"` / `name="false"`.
		p.SetAttribute("data-flag", true);
		auto sOut = page.Serialize();

		CHECK( sOut.contains("hidden")                  );  // bare
		CHECK_FALSE( sOut.contains(R"(hidden="true")") );
		CHECK( sOut.contains(R"(data-flag="true")")    );  // value-style
	}

	SECTION("arithmetic types hit the generic kFormat template")
	{
		p.SetAttribute("data-count", 42);
		p.SetAttribute("data-ratio", 0.5);
		auto sOut = page.Serialize();
		CHECK( sOut.contains(R"(data-count="42")")  );
		CHECK( sOut.contains(R"(data-ratio="0.5")") );
	}
}
