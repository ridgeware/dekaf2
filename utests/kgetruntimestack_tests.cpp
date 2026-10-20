#include "catch.hpp"
#include <dekaf2/system/os/kgetruntimestack.h>
#include <dekaf2/core/strings/kstring.h>

using namespace dekaf2;

TEST_CASE("kGetRuntimeStack")
{
	SECTION("backtrace")
	{
#ifndef DEKAF2_HAS_MUSL
		KString sTrace = kGetRuntimeStack();
		INFO  ( sTrace );
		CHECK ( sTrace.Split('\n').size() >= 5 );
#endif
	}

	SECTION("short backtrace")
	{
#ifndef DEKAF2_HAS_MUSL
		// on Windows from CaptureStackBackTrace() and DbgHelp - without a symbol
		// file (PDB) the frames name the module and the offset in it
		KString sTrace = kGetBacktrace();
		INFO  ( sTrace );
		CHECK ( sTrace.Split('\n').size() >= 5 );
#endif
	}

}

TEST_CASE("kNormalizeFunctionName")
{
	SECTION("function signatures in the formats of gcc and clang")
	{
		// gcc
		CHECK ( kNormalizeFunctionName("dekaf2::KString dekaf2::KRSACert::GetPEM()")                              == "KRSACert::GetPEM"   );
		CHECK ( kNormalizeFunctionName("static bool dekaf2::KTLSContext::SetDefaultGroups(dekaf2::KStringView)")  == "KTLSContext::SetDefaultGroups" );
		CHECK ( kNormalizeFunctionName("std::vector<dekaf2::KString> dekaf2::kResolveHostToList(KStringView, bool, bool, std::size_t)") == "kResolveHostToList" );
		CHECK ( kNormalizeFunctionName("const dekaf2::KString& dekaf2::KCache<Key, Value>::Get(const Key&) [with Key = int; Value = dekaf2::KString]") == "KCache::Get" );
		// clang
		CHECK ( kNormalizeFunctionName("dekaf2::KString &dekaf2::KString::append(dekaf2::KStringView)")           == "KString::append"    );
		CHECK ( kNormalizeFunctionName("dekaf2::KString *dekaf2::KCache<int, dekaf2::KString>::Find(const int &)") == "KCache::Find"      );
		CHECK ( kNormalizeFunctionName("auto dekaf2::KWebServer::Run()::(anonymous class)::operator()() const")   == "KWebServer::Run"    );
		// operators, self types, and names without a return type or parameters
		CHECK ( kNormalizeFunctionName("dekaf2::KFoo::self_type& dekaf2::KFoo::operator+=(const dekaf2::KFoo::self_type&)") == "KFoo::operator+=" );
		CHECK ( kNormalizeFunctionName("int main(int, char **)")                                                  == "main"               );
		CHECK ( kNormalizeFunctionName("main")                                                                    == "main"               );
	}

	SECTION("only whole names are removed, and std:: stays in a function name")
	{
		CHECK ( kNormalizeFunctionName("void dekaf2::KFoo::selfTest()")                                           == "KFoo::selfTest"      );
		CHECK ( kNormalizeFunctionName("int notdekaf2::Foo::Bar()")                                               == "notdekaf2::Foo::Bar" );
		CHECK ( kNormalizeFunctionName("void std::thread::join()")                                                == "std::thread::join"   );
	}

	SECTION("the signature of a warning")
	{
		KString sOut;
		detail::AppendWithoutNoise(sOut, "std::vector<dekaf2::KString> dekaf2::kResolveHostToList(KStringView, bool, bool, std::size_t)", true);
		CHECK ( sOut == "vector<KString> kResolveHostToList(KStringView, bool, bool, size_t)" );

		sOut.clear();
		detail::AppendWithoutNoise(sOut, "dekaf2::KFoo::self_type& dekaf2::KFoo::operator+=(const dekaf2::KFoo::self_type&)", true);
		CHECK ( sOut == "KFoo& KFoo::operator+=(const KFoo&)" );

		sOut.clear();
		detail::AppendWithoutNoise(sOut, "int mystd::Foo::selfTest(std::string)", true);
		CHECK ( sOut == "int mystd::Foo::selfTest(string)" );

		sOut.clear();
		detail::AppendWithoutNoise(sOut, "int Foo::Bar(std::string)", false);
		CHECK ( sOut == "int Foo::Bar(std::string)" );
	}

	SECTION("appending to a string")
	{
		KString sOut = "prefix: ";
		kNormalizeFunctionName(sOut, "dekaf2::KString dekaf2::KRSACert::GetPEM()");
		CHECK ( sOut == "prefix: KRSACert::GetPEM" );
		kNormalizeFunctionName(sOut, "dekaf2::KString &dekaf2::KString::append(dekaf2::KStringView)");
		CHECK ( sOut == "prefix: KRSACert::GetPEMKString::append" );
	}
}
