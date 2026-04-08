//
// dekaf2 benchmarks
//
// NOTE: benchmarks must always be run on Release builds!
//
// Building:
//   cmake -S <dekaf2-source> -B <build-dir> -DCMAKE_BUILD_TYPE=Release
//   cmake --build <build-dir> --target benchmarks
//
// Running:
//   <build-dir>/benchmarks/benchmarks                         # text output only
//   <build-dir>/benchmarks/benchmarks --json results.json     # text + JSON output
//
// Comparing two runs:
//   python3 benchmarks/compare_benchmarks.py baseline.json current.json
//   python3 benchmarks/compare_benchmarks.py baseline.json current.json --ranges benchmarks/benchmark_ranges.json
//   python3 benchmarks/compare_benchmarks.py baseline.json current.json --threshold 5.0
//
// Exit codes of compare script:
//   0 = all ok
//   1 = regressions or range violations detected
//   2 = usage error
//

#include <dekaf2/dekaf2.h>
#include <dekaf2/klog.h>
#include <dekaf2/kprof.h>
#include <dekaf2/kstringview.h>
#include <cstdio>

extern void other_bench();
extern void kbitfields_bench();
extern void shared_ptr_bench();
extern void kprops_bench();
extern void kwriter_bench();
extern void kreader_bench();
extern void std_string_bench();
extern void kstring_bench();
extern void kstringview_bench();
extern void kcasestring_bench();
extern void kxml_bench();
extern void khtmlparser_bench();
extern void kutf8_bench();
extern void kfindsetofchars_bench();
extern void krow_bench();

using namespace dekaf2;

int main(int argc, char* argv[])
{
	KLog::getInstance().SetName("bench");
	Dekaf::getInstance().SetMultiThreading();
	Dekaf::getInstance().SetUnicodeLocale();

	FILE* fpJSON = nullptr;

	for (int ii = 1; ii < argc; ++ii)
	{
		KStringView sArg(argv[ii]);

		if (sArg == "--json" && ii + 1 < argc)
		{
			fpJSON = std::fopen(argv[++ii], "w");

			if (!fpJSON)
			{
				std::fprintf(stderr, "cannot open '%s' for writing\n", argv[ii]);
				return 1;
			}

			kProfSetJSON(fpJSON);
		}
		else if (sArg == "--help" || sArg == "-h")
		{
			std::fprintf(stdout,
				"usage: benchmarks [options]\n"
				"\n"
				"options:\n"
				"  --json <file>  write results as JSON to <file>\n"
				"  --help         show this help\n"
			);
			return 0;
		}
		else
		{
			std::fprintf(stderr, "unknown argument: %s\n", argv[ii]);
			return 1;
		}
	}

	kxml_bench();
	khtmlparser_bench();
	kbitfields_bench();
	shared_ptr_bench();
	kprops_bench();
	kwriter_bench();
	kreader_bench();
	kfindsetofchars_bench();
	std_string_bench();
	kstring_bench();
	kstringview_bench();
	kcasestring_bench();
 	other_bench();
	kutf8_bench();
	krow_bench();

	kProfFinalize();

	if (fpJSON)
	{
		std::fclose(fpJSON);
	}

	return 0;
}
