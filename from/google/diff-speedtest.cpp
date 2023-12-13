
#include <dekaf2/koptions.h>
#include <dekaf2/kstring.h>
#include <dekaf2/kdiff.h>
#include <dekaf2/kreader.h>
#include <dekaf2/kduration.h>
#include <dekaf2/kfilesystem.h>

using namespace DEKAF2_NAMESPACE_NAME;

struct Options
{
	KStringViewZ sInputFile1 { "speedtest1.txt" };
	KStringViewZ sInputFile2 { "speedtest2.txt" };
	KStringViewZ sOutputFile;
	KDiff::DiffMode mode     { KDiff::DiffMode::Character  };
	KDiff::Sanitation san    { KDiff::Sanitation::Semantic };
	bool         bLineMode   { false };
	bool         bUnified    { false };
};


Options Options;
KOptions CLI(false);

void Test()
{
	auto sText1 = kReadAll(Options.sInputFile1);
	auto sText2 = kReadAll(Options.sInputFile2);

	// warmup
	KDiffToASCII(sText2, sText1);

	// do it
	std::size_t iUSecs { 0 };
	static constexpr int iMax = 5000;

	KDiff Diff;

	for (int i = 0; i < iMax; ++i)
	{
		KStopTime Timer;
		Diff.CreateDiff(sText1, sText2, Options.mode, Options.san);
		iUSecs += Timer.microseconds();
	}

	KOut.FormatLine("elapsed time (avg of {}): {} us", iMax, iUSecs / iMax);

	if (Options.sOutputFile)
	{
		if (Options.bUnified)
		{
			if (Options.sOutputFile != "-")
			{
				kWriteFile(Options.sOutputFile, Diff.GetUnifiedDiff());
			}
			else
			{
				KOut.Write(Diff.GetUnifiedDiff());
			}
		}
		else
		{
			if (Options.sOutputFile != "-")
			{
				kWriteFile(Options.sOutputFile, Diff.GetTextDiff());
			}
			else
			{
				KOut.Write(Diff.GetTextDiff());
			}
		}
	}
}

void VerifyFile(KStringViewZ sFilename)
{
	if (!kFileExists(sFilename))
	{
		throw KOptions::WrongParameterError(kFormat("unknown file: {}", sFilename));
	}
}

int Main(int argc, char** argv)
{
	CLI.Throw();
	CLI.SetBriefDescription("diff speed test").SetAdditionalArgDescription("<file1> <file2>");
	CLI.RegisterUnknownCommand([&](KOptions::ArgList& Commands)
	{
		if (Commands.size() != 2)
		{
			throw KOptions::Error("can only process exactly two files at once");
		}
		Options.sInputFile1 = Commands.pop();
		Options.sInputFile2 = Commands.pop();
		VerifyFile(Options.sInputFile1);
		VerifyFile(Options.sInputFile2);
	});

	CLI.Option("l,line").Help("set line mode")
	([&]()
	{
		Options.mode = KDiff::DiffMode::Line;
	});

	CLI.Option("s,speedup").Help("set fast character mode (line, then character)")
	([&]()
	{
		Options.mode = KDiff::DiffMode::LineThenCharacter;
	});

	CLI.Option("o,output").Help("set output file")
	([&](KStringViewZ sArg)
	{
		Options.sOutputFile = sArg;
	});

	CLI.Option("u,unified").Help("set unified diff mode")
	([&]()
	{
		Options.bUnified = true;
	});

	CLI.Parse(argc, argv, KOut);

	if (!kFileExists(Options.sInputFile2))
	{
		CLI.Help(KOut);
	}

	Test();

	return 0;
}

int main(int argc, char** argv)
{
	try
	{
		return Main (argc, argv);
	}
	catch (const KException& ex)
	{
		KErr.FormatLine(">> {}: {}", CLI.GetProgramName(), ex.what());
	}
	catch (const std::exception& ex)
	{
		KErr.FormatLine(">> {}: {}", CLI.GetProgramName(), ex.what());
	}
	return 1;
}
