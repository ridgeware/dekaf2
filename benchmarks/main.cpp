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

#include <dekaf2/core/init/dekaf2.h>
#include <dekaf2/core/logging/klog.h>
#include <dekaf2/time/duration/kprof.h>
#include <dekaf2/core/strings/kstringview.h>
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
extern void kwebobjects_bench();
extern void kutf8_bench();
extern void kfindsetofchars_bench();
extern void krow_bench();
extern void kurl_bench();
extern void ksplit_bench();
extern void khtmlentity_bench();
extern void kbase64_bench();
extern void kurlencode_bench();
extern void kreplace_bench();
extern void khash_bench();
extern void ktime_bench();
extern void kmemsearch_bench();

using namespace dekaf2;

struct BenchEntry
{
	const char* name;
	void (*fn)();
};

int main(int argc, char* argv[])
{
	KLog::getInstance().SetName("bench");
	Dekaf::getInstance().SetMultiThreading();
	Dekaf::getInstance().SetUnicodeLocale();

	FILE* fpJSON = nullptr;
	KStringView sFilter;

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
		else if (sArg == "--filter" && ii + 1 < argc)
		{
			sFilter = argv[++ii];
		}
		else if (sArg == "--list")
		{
			// fall through: handled below after the table is in scope
		}
		else if (sArg == "--help" || sArg == "-h")
		{
			std::fprintf(stdout,
				"usage: benchmarks [options]\n"
				"\n"
				"options:\n"
				"  --json <file>     write results as JSON to <file>\n"
				"  --filter <name>   run only benches whose name contains <name>\n"
				"  --list            list all benchmark functions and exit\n"
				"  --help            show this help\n"
			);
			return 0;
		}
		else
		{
			std::fprintf(stderr, "unknown argument: %s\n", argv[ii]);
			return 1;
		}
	}

	const BenchEntry kBenches[] = {
		{ "kxml",            &kxml_bench            },
		{ "khtmlparser",     &khtmlparser_bench     },
		{ "kwebobjects",     &kwebobjects_bench     },
		{ "kbitfields",      &kbitfields_bench      },
		{ "shared_ptr",      &shared_ptr_bench      },
		{ "kprops",          &kprops_bench          },
		{ "kwriter",         &kwriter_bench         },
		{ "kreader",         &kreader_bench         },
		{ "kfindsetofchars", &kfindsetofchars_bench },
		{ "std_string",      &std_string_bench      },
		{ "kstring",         &kstring_bench         },
		{ "kstringview",     &kstringview_bench     },
		{ "kcasestring",     &kcasestring_bench     },
		{ "other",           &other_bench           },
		{ "kutf8",           &kutf8_bench           },
		{ "krow",            &krow_bench            },
		{ "kurl",            &kurl_bench            },
		{ "ksplit",          &ksplit_bench          },
		{ "khtmlentity",     &khtmlentity_bench     },
		{ "kbase64",         &kbase64_bench         },
		{ "kurlencode",      &kurlencode_bench      },
		{ "kreplace",        &kreplace_bench        },
		{ "khash",           &khash_bench           },
		{ "ktime",           &ktime_bench           },
		{ "kmemsearch",      &kmemsearch_bench      },
	};

	for (int ii = 1; ii < argc; ++ii)
	{
		if (KStringView(argv[ii]) == "--list")
		{
			for (const auto& e : kBenches)
			{
				std::fprintf(stdout, "%s\n", e.name);
			}
			return 0;
		}
	}

	for (const auto& e : kBenches)
	{
		if (sFilter.empty() || KStringView(e.name).Contains(sFilter))
		{
			std::fprintf(stderr, "=== %s ===\n", e.name);
			e.fn();
		}
	}

	kProfFinalize();

	if (fpJSON)
	{
		std::fclose(fpJSON);
	}

	return 0;
}
