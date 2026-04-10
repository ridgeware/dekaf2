#include "catch.hpp"
#include <dekaf2/core/types/kdefinitions.h>
#include <dekaf2/core/strings/kstring.h>
#include <iostream>

using namespace dekaf2;

namespace {

// Build the feature report at file scope using preprocessor conditionals,
// as some compilers reject undefined __cpp_* tokens even in dead branches.

struct FeatureReport
{
	KString sDefined;
	KString sUndefined;

	FeatureReport()
	{
		// core language features
#ifdef __cpp_constexpr
		sDefined += kFormat("  __cpp_constexpr = {}\n", __cpp_constexpr);
#else
		sUndefined += "  __cpp_constexpr\n";
#endif
#ifdef __cpp_char8_t
		sDefined += kFormat("  __cpp_char8_t = {}\n", __cpp_char8_t);
#else
		sUndefined += "  __cpp_char8_t\n";
#endif
#ifdef __cpp_concepts
		sDefined += kFormat("  __cpp_concepts = {}\n", __cpp_concepts);
#else
		sUndefined += "  __cpp_concepts\n";
#endif
#ifdef __cpp_inheriting_constructors
		sDefined += kFormat("  __cpp_inheriting_constructors = {}\n", __cpp_inheriting_constructors);
#else
		sUndefined += "  __cpp_inheriting_constructors\n";
#endif
#ifdef __cpp_deduction_guides
		sDefined += kFormat("  __cpp_deduction_guides = {}\n", __cpp_deduction_guides);
#else
		sUndefined += "  __cpp_deduction_guides\n";
#endif
#ifdef __cpp_exceptions
		sDefined += kFormat("  __cpp_exceptions = {}\n", __cpp_exceptions);
#else
		sUndefined += "  __cpp_exceptions\n";
#endif
#ifdef __cpp_if_constexpr
		sDefined += kFormat("  __cpp_if_constexpr = {}\n", __cpp_if_constexpr);
#else
		sUndefined += "  __cpp_if_constexpr\n";
#endif
#ifdef __cpp_impl_three_way_comparison
		sDefined += kFormat("  __cpp_impl_three_way_comparison = {}\n", __cpp_impl_three_way_comparison);
#else
		sUndefined += "  __cpp_impl_three_way_comparison\n";
#endif
#ifdef __cpp_unicode_characters
		sDefined += kFormat("  __cpp_unicode_characters = {}\n", __cpp_unicode_characters);
#else
		sUndefined += "  __cpp_unicode_characters\n";
#endif

		// library features
#ifdef __cpp_lib_apply
		sDefined += kFormat("  __cpp_lib_apply = {}\n", __cpp_lib_apply);
#else
		sUndefined += "  __cpp_lib_apply\n";
#endif
#ifdef __cpp_lib_optional
		sDefined += kFormat("  __cpp_lib_optional = {}\n", __cpp_lib_optional);
#else
		sUndefined += "  __cpp_lib_optional\n";
#endif
#ifdef __cpp_lib_bit_cast
		sDefined += kFormat("  __cpp_lib_bit_cast = {}\n", __cpp_lib_bit_cast);
#else
		sUndefined += "  __cpp_lib_bit_cast\n";
#endif
#ifdef __cpp_lib_bitops
		sDefined += kFormat("  __cpp_lib_bitops = {}\n", __cpp_lib_bitops);
#else
		sUndefined += "  __cpp_lib_bitops\n";
#endif
#ifdef __cpp_lib_bool_constant
		sDefined += kFormat("  __cpp_lib_bool_constant = {}\n", __cpp_lib_bool_constant);
#else
		sUndefined += "  __cpp_lib_bool_constant\n";
#endif
#ifdef __cpp_lib_byteswap
		sDefined += kFormat("  __cpp_lib_byteswap = {}\n", __cpp_lib_byteswap);
#else
		sUndefined += "  __cpp_lib_byteswap\n";
#endif
#ifdef __cpp_lib_chrono
		sDefined += kFormat("  __cpp_lib_chrono = {}\n", __cpp_lib_chrono);
#else
		sUndefined += "  __cpp_lib_chrono\n";
#endif
#ifdef __cpp_lib_chrono_udls
		sDefined += kFormat("  __cpp_lib_chrono_udls = {}\n", __cpp_lib_chrono_udls);
#else
		sUndefined += "  __cpp_lib_chrono_udls\n";
#endif
#ifdef __cpp_lib_constexpr_char_traits
		sDefined += kFormat("  __cpp_lib_constexpr_char_traits = {}\n", __cpp_lib_constexpr_char_traits);
#else
		sUndefined += "  __cpp_lib_constexpr_char_traits\n";
#endif
#ifdef __cpp_lib_format
		sDefined += kFormat("  __cpp_lib_format = {}\n", __cpp_lib_format);
#else
		sUndefined += "  __cpp_lib_format\n";
#endif
#ifdef __cpp_lib_incomplete_container_elements
		sDefined += kFormat("  __cpp_lib_incomplete_container_elements = {}\n", __cpp_lib_incomplete_container_elements);
#else
		sUndefined += "  __cpp_lib_incomplete_container_elements\n";
#endif
#ifdef __cpp_lib_int_pow2
		sDefined += kFormat("  __cpp_lib_int_pow2 = {}\n", __cpp_lib_int_pow2);
#else
		sUndefined += "  __cpp_lib_int_pow2\n";
#endif
#ifdef __cpp_lib_ios_noreplace
		sDefined += kFormat("  __cpp_lib_ios_noreplace = {}\n", __cpp_lib_ios_noreplace);
#else
		sUndefined += "  __cpp_lib_ios_noreplace\n";
#endif
#ifdef __cpp_lib_is_constant_evaluated
		sDefined += kFormat("  __cpp_lib_is_constant_evaluated = {}\n", __cpp_lib_is_constant_evaluated);
#else
		sUndefined += "  __cpp_lib_is_constant_evaluated\n";
#endif
#ifdef __cpp_lib_is_layout_compatible
		sDefined += kFormat("  __cpp_lib_is_layout_compatible = {}\n", __cpp_lib_is_layout_compatible);
#else
		sUndefined += "  __cpp_lib_is_layout_compatible\n";
#endif
#ifdef __cpp_lib_logical_traits
		sDefined += kFormat("  __cpp_lib_logical_traits = {}\n", __cpp_lib_logical_traits);
#else
		sUndefined += "  __cpp_lib_logical_traits\n";
#endif
#ifdef __cpp_lib_map_try_emplace
		sDefined += kFormat("  __cpp_lib_map_try_emplace = {}\n", __cpp_lib_map_try_emplace);
#else
		sUndefined += "  __cpp_lib_map_try_emplace\n";
#endif
#ifdef __cpp_lib_ranges
		sDefined += kFormat("  __cpp_lib_ranges = {}\n", __cpp_lib_ranges);
#else
		sUndefined += "  __cpp_lib_ranges\n";
#endif
#ifdef __cpp_lib_shared_mutex
		sDefined += kFormat("  __cpp_lib_shared_mutex = {}\n", __cpp_lib_shared_mutex);
#else
		sUndefined += "  __cpp_lib_shared_mutex\n";
#endif
#ifdef __cpp_lib_source_location
		sDefined += kFormat("  __cpp_lib_source_location = {}\n", __cpp_lib_source_location);
#else
		sUndefined += "  __cpp_lib_source_location\n";
#endif
#ifdef __cpp_lib_string_resize_and_overwrite
		sDefined += kFormat("  __cpp_lib_string_resize_and_overwrite = {}\n", __cpp_lib_string_resize_and_overwrite);
#else
		sUndefined += "  __cpp_lib_string_resize_and_overwrite\n";
#endif
#ifdef __cpp_lib_string_view
		sDefined += kFormat("  __cpp_lib_string_view = {}\n", __cpp_lib_string_view);
#else
		sUndefined += "  __cpp_lib_string_view\n";
#endif
#ifdef __cpp_lib_three_way_comparison
		sDefined += kFormat("  __cpp_lib_three_way_comparison = {}\n", __cpp_lib_three_way_comparison);
#else
		sUndefined += "  __cpp_lib_three_way_comparison\n";
#endif
#ifdef __cpp_lib_to_underlying
		sDefined += kFormat("  __cpp_lib_to_underlying = {}\n", __cpp_lib_to_underlying);
#else
		sUndefined += "  __cpp_lib_to_underlying\n";
#endif
#ifdef __cpp_lib_void_t
		sDefined += kFormat("  __cpp_lib_void_t = {}\n", __cpp_lib_void_t);
#else
		sUndefined += "  __cpp_lib_void_t\n";
#endif
	}
};

} // anonymous namespace

TEST_CASE("Features")
{
	SECTION("C++ standard")
	{
		std::cout << "\n== C++ standard ==\n\n";
		std::cout << "  __cplusplus = " << __cplusplus << "\n";
		std::cout << "  DEKAF2_HAS_CPP_11 = "
#ifdef DEKAF2_HAS_CPP_11
		          << DEKAF2_HAS_CPP_11
#else
		          << "undefined"
#endif
		          << "\n";
		std::cout << "  DEKAF2_HAS_CPP_14 = "
#ifdef DEKAF2_HAS_CPP_14
		          << DEKAF2_HAS_CPP_14
#else
		          << "undefined"
#endif
		          << "\n";
		std::cout << "  DEKAF2_HAS_CPP_17 = "
#ifdef DEKAF2_HAS_CPP_17
		          << DEKAF2_HAS_CPP_17
#else
		          << "undefined"
#endif
		          << "\n";
		std::cout << "  DEKAF2_HAS_CPP_20 = "
#ifdef DEKAF2_HAS_CPP_20
		          << DEKAF2_HAS_CPP_20
#else
		          << "undefined"
#endif
		          << "\n";
		std::cout << "  DEKAF2_HAS_CPP_23 = "
#ifdef DEKAF2_HAS_CPP_23
		          << DEKAF2_HAS_CPP_23
#else
		          << "undefined"
#endif
		          << "\n";
		std::cout << "  DEKAF2_HAS_CPP_26 = "
#ifdef DEKAF2_HAS_CPP_26
		          << DEKAF2_HAS_CPP_26
#else
		          << "undefined"
#endif
		          << "\n";
		std::cout << std::endl;

		CHECK ( true );
	}

	SECTION("feature test macros")
	{
		FeatureReport report;

		std::cout << "\n== C++ feature test macros used by dekaf2 ==\n";

		if (!report.sDefined.empty())
		{
			std::cout << "\ndefined:\n" << report.sDefined;
		}

		if (!report.sUndefined.empty())
		{
			std::cout << "\nnot defined:\n" << report.sUndefined;
		}

		std::cout << std::endl;

		CHECK ( true );
	}
}
