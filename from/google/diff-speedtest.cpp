
#include <dekaf2/koptions.h>
#include <dekaf2/kstring.h>
#include <dekaf2/kdiffer.h>
#include <dekaf2/kreader.h>
#include <dekaf2/ktimer.h>
#include <dekaf2/kfilesystem.h>

using namespace dekaf2;

struct Options
{
	KStringViewZ sInputFile1 { "speedtest1.txt" };
	KStringViewZ sInputFile2 { "speedtest2.txt" };
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

	for (int i = 0; i < iMax; ++i)
	{
		KStopTime Timer;
		KDiffToASCII(sText1, sText2);
		iUSecs += Timer.microseconds();
	}

	KOut.FormatLine("elapsed time (avg of {}): {} us", iMax, iUSecs / iMax);
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
