#include "catch.hpp"

#include <dekaf2/containers/associative/kmru.h>
#include <dekaf2/core/strings/kstring.h>
#include <dekaf2/core/strings/kstringview.h>
#include <vector>

using namespace dekaf2;

namespace {

//-----------------------------------------------------------------------------
std::size_t& CountedKeyConstructions()
//-----------------------------------------------------------------------------
{
	static std::size_t s_iConstructions { 0 };
	return s_iConstructions;
}

// a key type with transparent std::hash and std::equal_to that counts its copies
struct CountedKey
{
	explicit CountedKey(KStringView sKey) : sKey(sKey) { ++CountedKeyConstructions(); }
	CountedKey(const CountedKey& other) : sKey(other.sKey) { ++CountedKeyConstructions(); }
	CountedKey(CountedKey&&) = default;

	bool operator==(const CountedKey& other) const { return sKey == other.sKey; }

	KString sKey;
};

//-----------------------------------------------------------------------------
KString Join(const std::vector<int>& Keys)
//-----------------------------------------------------------------------------
{
	KString sJoined;

	for (auto iKey : Keys)
	{
		if (!sJoined.empty())
		{
			sJoined += ',';
		}
		sJoined += KString::to_string(iKey);
	}

	return sJoined;
}

//-----------------------------------------------------------------------------
/// returns the keys of a KMRUMap in MRU order, separated by commas
template<class MRU>
KString Order(MRU& mru)
//-----------------------------------------------------------------------------
{
	std::vector<int> Keys;

	for (const auto& it : mru)
	{
		Keys.push_back(it.first);
	}

	return Join(Keys);
}

} // end of anonymous namespace

namespace std {

template<>
struct hash<CountedKey>
{
	using is_transparent = void;

	std::size_t operator()(DEKAF2_PREFIX KStringView sKey) const noexcept { return DEKAF2_PREFIX kHash(sKey.data(), sKey.size()); }
	std::size_t operator()(const CountedKey& Key)          const noexcept { return operator()(Key.sKey.ToView()); }
};

template<>
struct equal_to<CountedKey>
{
	using is_transparent = void;

	bool operator()(const CountedKey& left, const CountedKey& right)          const { return left == right;              }
	bool operator()(DEKAF2_PREFIX KStringView left, const CountedKey& right) const { return left == right.sKey;         }
	bool operator()(const CountedKey& left, DEKAF2_PREFIX KStringView right) const { return left.sKey == right;         }
};

} // end of namespace std

TEST_CASE("KMRU")
{
	SECTION("KMRUList")
	{
		KMRUList<int> MRU(3);

		MRU.insert(1);
		MRU.insert(2);
		MRU.insert(3);
		CHECK ( MRU.size() == 3 );
		CHECK ( *MRU.begin() == 3 );

		MRU.insert(4);
		CHECK ( MRU.size() == 3 );
		CHECK ( MRU.find(1) == MRU.end() );

		CHECK ( MRU.find(2) != MRU.end() );
		CHECK ( *MRU.begin() == 2 );

		CHECK ( MRU.peek(3) != MRU.end() );
		CHECK ( *MRU.begin() == 2 );
		CHECK ( MRU.peek(1) == MRU.end() );
	}

	SECTION("KMRUMap find and peek")
	{
		KMRUMap<int, KString> MRU(3);

		MRU.insert({ 1, "one"   });
		MRU.insert({ 2, "two"   });
		MRU.insert({ 3, "three" });
		CHECK ( Order(MRU) == "3,2,1" );

		auto it = MRU.peek(1);
		REQUIRE ( it != MRU.end() );
		CHECK ( it->second == "one" );
		CHECK ( Order(MRU) == "3,2,1" );

		CHECK ( MRU.find(1) != MRU.end() );
		CHECK ( Order(MRU) == "1,3,2" );

		const auto& ConstMRU = MRU;
		CHECK ( ConstMRU.peek(2) != ConstMRU.end() );
		CHECK ( ConstMRU.peek(4) == ConstMRU.end() );

		MRU.insert({ 4, "four" });
		CHECK ( Order(MRU) == "4,1,3" );
	}

	SECTION("KString key lookups")
	{
		KMRUMap<KString, int> MRU(10);

		MRU.insert({ "abc", 1 });
		MRU.insert({ "def", 2 });

		KStringView sKey { "abc" };
		auto it = MRU.peek(sKey);
		REQUIRE ( it != MRU.end() );
		CHECK ( it->second == 1 );

		CHECK ( MRU.find("def") != MRU.end() );
		CHECK ( MRU.find(KStringView("def")) != MRU.end() );
		CHECK ( MRU.find(KString("def")) != MRU.end() );
		CHECK ( MRU.find(KStringView("xyz")) == MRU.end() );
		CHECK ( MRU.erase(KStringView("abc")) == true );
		CHECK ( MRU.size() == 1 );
	}

	SECTION("lookups with a transparent key type do not construct a key")
	{
		KMRUMap<CountedKey, int> MRU(10);

		MRU.insert({ CountedKey("abc"), 1 });
		MRU.insert({ CountedKey("def"), 2 });

		auto iConstructions = CountedKeyConstructions();

		CHECK ( MRU.find(KStringView("abc")) != MRU.end() );
		CHECK ( MRU.peek(KStringView("def")) != MRU.end() );
		CHECK ( MRU.find(KStringView("xyz")) == MRU.end() );
		CHECK ( MRU.erase(KStringView("abc")) == true );

		CHECK ( CountedKeyConstructions() == iConstructions );
	}

	SECTION("insert with second chance")
	{
		KMRUMap<int, int> MRU(3);

		MRU.insert({ 1, 1 });
		MRU.insert({ 2, 2 });
		MRU.insert({ 3, 3 });

		std::vector<int> Checked;

		auto it = MRU.insert({ 4, 4 }, [&Checked](const KMRUMap<int, int>::element_type& element)
		{
			Checked.push_back(element.first);
			// keep 1 once more
			return element.first == 1;
		});

		REQUIRE ( it != MRU.end() );
		CHECK ( it->first == 4 );
		CHECK ( Join(Checked) == "1,2" );
		CHECK ( Order(MRU) == "4,1,3" );
	}

	SECTION("insert with second chance for all elements")
	{
		KMRUMap<int, int> MRU(3);

		MRU.insert({ 1, 1 });
		MRU.insert({ 2, 2 });
		MRU.insert({ 3, 3 });

		std::size_t iCalls { 0 };

		// a predicate that never resets its condition still ends after one round
		auto it = MRU.insert({ 4, 4 }, [&iCalls](const KMRUMap<int, int>::element_type&)
		{
			++iCalls;
			return true;
		});

		REQUIRE ( it != MRU.end() );
		CHECK ( it->first == 4 );
		CHECK ( iCalls == 3 );
		CHECK ( MRU.size() == 3 );
		CHECK ( MRU.begin()->first == 4 );
		CHECK ( MRU.peek(4) != MRU.end() );
	}

	SECTION("insert with second chance of a known key")
	{
		KMRUMap<int, int> MRU(3);

		MRU.insert({ 1, 1 });
		MRU.insert({ 2, 2 });
		MRU.insert({ 3, 3 });

		auto it = MRU.insert({ 1, 100 }, [](const KMRUMap<int, int>::element_type&) { return false; });

		REQUIRE ( it != MRU.end() );
		CHECK ( it->first  == 1 );
		CHECK ( it->second == 1 );
		CHECK ( Order(MRU) == "1,3,2" );
	}

	SECTION("insert with second chance and maximum size 1")
	{
		KMRUMap<int, int> MRU(1);

		MRU.insert({ 1, 1 });

		auto it = MRU.insert({ 2, 2 }, [](const KMRUMap<int, int>::element_type&) { return true; });

		REQUIRE ( it != MRU.end() );
		CHECK ( it->first == 2 );
		CHECK ( Order(MRU) == "2" );
	}

	SECTION("insert with second chance and maximum size 0")
	{
		KMRUMap<int, int> MRU(0);

		auto it = MRU.insert({ 1, 1 }, [](const KMRUMap<int, int>::element_type&) { return true; });

		CHECK ( it == MRU.end() );
		CHECK ( MRU.empty() );
	}
}
