#include <benchmark/benchmark.h>
#include <dekaf2/kstring.h>
#include <dekaf2/ksystem.h>

using namespace dekaf2;

void BM_test(benchmark::State& state)
{
	for (auto _ : state)
	{
		KString empty_string;
		if ((kRandom() & 1) == 0) empty_string += "abc";
		benchmark::DoNotOptimize(empty_string);
	}
}

BENCHMARK(BM_test);

int main(int argc, char** argv)
{
  benchmark::Initialize(&argc, argv);
  benchmark::RunSpecifiedBenchmarks();
  benchmark::Shutdown();
  return 0;
}
