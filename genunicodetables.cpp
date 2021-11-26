#include <dekaf2/kstream.h>
#include <dekaf2/kwebclient.h>
#include <dekaf2/koptions.h>
#include <dekaf2/kstring.h>
#include <dekaf2/kstringstream.h>
#include <dekaf2/kctype.h>
#include <dekaf2/ksplit.h>
#include <dekaf2/kfrozen.h>
#include <dekaf2/dekaf2.h>
#include <vector>


using namespace dekaf2;

constexpr KStringView sURL = "https://www.unicode.org/Public/UCD/latest/ucd/UnicodeData.txt";

constexpr KStringView g_Help[] = {
	"",
	"genunicodetables - load and build the Unicode character type",
	"                   and conversion tables for dekaf2",
	"",
	"usage: genunicodetables [-annotate] -cpp <output.cpp>",
	"",
	"where <output.cpp> is the filename the tables are written into",
	"and -annotate adds the input data as comments",
	""
};

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class CaseFold
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
public:

	//-----------------------------------------------------------------------------
	CaseFold()
	//-----------------------------------------------------------------------------
	{
		FoldVec.push_back(0);
	}

	//-----------------------------------------------------------------------------
	size_t Add(Unicode::codepoint_t iCodepoint, Unicode::codepoint_t iFoldedTo, bool bToLower)
	//-----------------------------------------------------------------------------
	{
		int32_t iFoldDist { 0 };

		if (bToLower)
		{
			iFoldDist = iFoldedTo - iCodepoint;
		}
		else
		{
			iFoldDist = iCodepoint - iFoldedTo;
		}

		if (iFoldDist)
		{
			auto pos = std::find(FoldVec.begin(), FoldVec.end(), iFoldDist);

			if (pos != FoldVec.end())
			{
				return pos - FoldVec.begin();
			}
			else
			{
				if (FoldVec.size() > 255)
				{
					KErr.WriteLine("Error: CaseFoldVector too large for data struct");
					return 0;
				}
				else
				{
					FoldVec.push_back(iFoldDist);
					return FoldVec.size() - 1;
				}
			}
		}

		return 0;
	}

	//-----------------------------------------------------------------------------
	void Write(KOutStream& File)
	//-----------------------------------------------------------------------------
	{
		File.WriteLine("const int32_t KCodePoint::CaseFolds[]");
		File.WriteLine("{");

		int iCount { 0 };
		for (auto it : FoldVec)
		{
			File.FormatLine("\t{:6d} , // {:3d}", it, iCount++);
		}

		File.WriteLine("};");

		KOut.FormatLine(":: Wrote {} CaseFold values", FoldVec.size());
	}

private:

	std::vector<int32_t> FoldVec;

}; // CaseFold

class CaseFold CaseFold;

constexpr uint32_t CODEPOINT_MAX = 0xFFFF;

bool bAnnotate = false;

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
struct CodePoint
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	CodePoint() = default;
	CodePoint(KStringView sLine);

	void SetCategory(KStringView sCategory);

	Unicode::codepoint_t   iCodepoint { 0 };
	KStringView            sCategory  { "OtherNotAssigned" };
	KStringView            sType      { "Other" };
	KString                sOriginalLine {};
	std::size_t            iCaseFold  { 0 };
	bool                   bIsFirst   { false };
	bool                   bIsLast    { false };

	void clear();

	static constexpr std::pair<KStringView, KStringView> CategoriesList[] =
	{
		{ "Lu", "LetterUppercase"          },
		{ "Ll", "LetterLowercase"          },
		{ "Lt", "LetterTitlecase"          },
		{ "Lm", "LetterModifier"           },
		{ "Lo", "LetterOther"              },

		{ "Mn", "MarkNonspacing"           },
		{ "Mc", "MarkSpacingCombining"     },
		{ "Me", "MarkEnclosing"            },

		{ "Nd", "NumberDecimalDigit"       },
		{ "Nl", "NumberLetter"             },
		{ "No", "NumberOther"              },

		{ "Pc", "PunctuationConnector"     },
		{ "Pd", "PunctuationDash"          },
		{ "Ps", "PunctuationOpen"          },
		{ "Pe", "PunctuationClose"         },
		{ "Pi", "PunctuationInitialQuote"  },
		{ "Pf", "PunctuationFinalQote"     },
		{ "Po", "PunctuationOther"         },

		{ "Sm", "SymbolMath"               },
		{ "Sc", "SymbolCurrency"           },
		{ "Sk", "SymbolModifier"           },
		{ "So", "SymbolOther"              },

		{ "Zs", "SeparatorSpace"           },
		{ "Zl", "SeparatorLine"            },
		{ "Zp", "SeparatorParagraph"       },

		{ "Cc", "OtherControl"             },
		{ "Cf", "OtherFormat"              },
		{ "Cs", "OtherSurrogate"           },
		{ "Co", "OtherPrivateUse"          },
		{ "Cn", "OtherNotAssigned"         }
	};

	static constexpr auto Categories = frozen::make_unordered_map(CategoriesList);

	static constexpr std::pair<char, KStringView> TypeList[] =
	{
		{ 'L', "Letter"      },
		{ 'M', "Mark"        },
		{ 'N', "Number"      },
		{ 'P', "Punctuation" },
		{ 'S', "Symbol"      },
		{ 'Z', "Separator"   },
		{ 'C', "Other"       }
	};

	static constexpr auto Types = frozen::make_unordered_map(TypeList);

}; // CodePoint

//-----------------------------------------------------------------------------
void CodePoint::clear()
//-----------------------------------------------------------------------------
{
	*this = CodePoint{};
}

//-----------------------------------------------------------------------------
void CodePoint::SetCategory(KStringView _sCategory)
//-----------------------------------------------------------------------------
{
	auto it = Categories.find(_sCategory);
	if (it != Categories.end())
	{
		sCategory = it->second;
		auto ti = Types.find(_sCategory.front());
		if (ti != Types.end())
		{
			sType = ti->second;
		}
		else
		{
			KErr.FormatLine("type unknown: {}", _sCategory.front());
		}
	}
	else
	{
		KErr.FormatLine("category unknown: {}", _sCategory);
	}
}

//-----------------------------------------------------------------------------
CodePoint::CodePoint(KStringView sLine)
//-----------------------------------------------------------------------------
{
	if (bAnnotate)
	{
		sOriginalLine = sLine;
	}

	std::vector<KStringView> Part;

	if (kSplit(Part, sLine, ";") < 13)
	{
		KErr.FormatLine("invalid: {}", sLine);
	}
	else
	{
		iCodepoint    = Part[0].UInt32(true /* IsHex */);
		SetCategory(Part[2]);
		bIsFirst      = Part[1].ends_with("First>");
		bIsLast       = Part[1].ends_with("Last>");

		auto iToUpper = Part[12].UInt16(true);
		auto iToLower = Part[13].UInt16(true);

		if (iCodepoint <= CODEPOINT_MAX)
		{
			if (iToUpper)
			{
				iCaseFold = CaseFold.Add(iCodepoint, iToUpper, true);
			}
			else if (iToLower)
			{
				iCaseFold = CaseFold.Add(iCodepoint, iToLower, false);
			}
		}
	}
}

std::vector<CodePoint> CodePoints;

CodePoint FirstCodePointInRange;

//-----------------------------------------------------------------------------
bool ParseLine(KStringView sLine)
//-----------------------------------------------------------------------------
{
	CodePoint cp(sLine);

	if (cp.iCodepoint > CODEPOINT_MAX)
	{
		return true;
	}

	if (cp.bIsFirst)
	{
		FirstCodePointInRange = cp;
	}
	else if (cp.bIsLast)
	{
		if (FirstCodePointInRange.bIsFirst)
		{
			// fill the range from first to last..
			for (auto pos = FirstCodePointInRange.iCodepoint; pos <= cp.iCodepoint; ++pos)
			{
				CodePoint range;

				if (pos == cp.iCodepoint)
				{
					range = cp;
				}
				else
				{
					range = FirstCodePointInRange;
				}

				if (pos > FirstCodePointInRange.iCodepoint && pos < cp.iCodepoint)
				{
					range.sOriginalLine.clear();
				}

				range.iCodepoint = pos;

				CodePoints[pos] = std::move(range);
			}
		}
		else
		{
			KErr.WriteLine("had end of range but no valid start of range");
			return false;
		}
		FirstCodePointInRange.clear();
	}
	else
	{
		if (FirstCodePointInRange.bIsFirst)
		{
			KErr.WriteLine("have open start of range, but no end of range");
			return false;
		}
		CodePoints[cp.iCodepoint] = cp;
	}

	return true;

} // ParseLine


//-----------------------------------------------------------------------------
bool WriteTables(KStringViewZ sFileName)
//-----------------------------------------------------------------------------
{
	KOutFile File(sFileName, std::ios::trunc);

	if (!File.is_open())
	{
		KErr.FormatLine("Cannot open {}", sFileName);
		return false;
	}

	File.WriteLine("// This file was generated by genunicodetables, the dekaf2 unicode");
	File.WriteLine("// table generator. Please do not edit it manually.");
	File.WriteLine();

	CaseFold.Write(File);

	File.WriteLine();

	File.WriteLine("const KCodePoint::Property KCodePoint::CodePoints[]");
	File.WriteLine("{");

	int iCount { 0 };

	for (auto& it : CodePoints)
	{
		File.FormatLine("\t{{ {:23s} , {:12s}, {:6d} }}, // {:5d} {}", it.sCategory, it.sType, it.iCaseFold, iCount++, it.sOriginalLine);
	}

	File.WriteLine("};");

	KOut.FormatLine(":: Wrote {} CodePoint values", CodePoints.size());

	File.WriteLine();

	return true;

} // WriteTables

//-----------------------------------------------------------------------------
// modifications to satisfy the C++ ctype model
void PatchTables()
//-----------------------------------------------------------------------------
{
	// we need to patch the unicode table such that 0x09..0x0d are of Type Separator
	// (they are Control characters, but we don't care for that type)
	// otherwise IsSpace() is incorrect
	CodePoints[0x09].sType = "Separator";
	CodePoints[0x0a].sType = "Separator";
	CodePoints[0x0b].sType = "Separator";
	CodePoints[0x0c].sType = "Separator";
	CodePoints[0x0d].sType = "Separator";

	//  133 is std::iswspace() on the mac, but not on Linux. So we leave it as is
	// 8203 is std::iswspace() on the mac, but not on Linux. So we leave it as is
	//  160 is !std::iswspace() on Linux, but std::iswpunct()
	CodePoints[160].sType = "Mark";

	// The tab character also needs the Category SeparatorSpace, otherwise
	// IsBlank() is incorrect
	CodePoints[0x09].sCategory = "SeparatorSpace";

} // PatchTable

//-----------------------------------------------------------------------------
int BuildTables(KStringViewZ sFileName)
//-----------------------------------------------------------------------------
{
	KString sUnicodeData = kHTTPGet(sURL);

	CodePoints.clear();
	CodePoints.resize(CODEPOINT_MAX+1);

	if (!sUnicodeData)
	{
		KErr.FormatLine("Cannot read from {} - abort", sURL);
		return 1;
	}

	KInStringStream iss(sUnicodeData);

	for (auto& sLine : iss)
	{
		if (!ParseLine(sLine))
		{
			return 1;
		}
	}

	PatchTables();

	if (!WriteTables(sFileName))
	{
		return 1;
	}

	return 0;

} // BuildTables

//-----------------------------------------------------------------------------
int main(int argc, char** argv)
//-----------------------------------------------------------------------------
{
	KInit(false).SetLocale();

	KOptions Options(false);

	Options.RegisterHelp(g_Help);

	int iError { 0 };

	Options.Option("annotate")([&]()
	{
		bAnnotate = true;
	});

	Options.Option("cpp", "need output file name")
	([&](KStringViewZ sFileName)
	{
		iError = BuildTables(sFileName);
	});

	if (Options.Parse(argc, argv, KOut))
	{
		return 1; // error
	}

	return iError;

} // main
