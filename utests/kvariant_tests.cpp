#include "catch.hpp"

#include <dekaf2/kvariant.h>

using namespace dekaf2;

TEST_CASE("KVariant") {

	SECTION("forward decl")
	{
		struct FWD;

		struct NotFWD { };

		using Test = std::vector<var::variant<FWD, NotFWD>>;

		struct FWD { };

		Test test;
	}
}
