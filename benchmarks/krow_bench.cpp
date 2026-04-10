#include <dekaf2/time/duration/kprof.h>
#include <dekaf2/core/strings/kstring.h>
#include <dekaf2/data/sql/krow.h>
#include <dekaf2/core/format/kformat.h>
#include <dekaf2/containers/memory/kpersist.h>
#include <vector>

using namespace dekaf2;

// KProf stores const char* — labels must outlive kProfFinalize()
static KPersistStrings<KString, false> s_Labels;

static const char* Label(KString sLabel)
{
	return s_Labels.Persist(std::move(sLabel)).c_str();
}

// simulate the column metadata that KSQL::KColInfo would provide
struct ColDef
{
	KString     sName;
	KString     sValue;
	KCOL::Flags iFlags;
	KCOL::Len   iMaxLen;
};

static std::vector<ColDef> MakeColumnDefs(uint32_t iNumCols)
{
	std::vector<ColDef> Cols;
	Cols.reserve(iNumCols);

	for (uint32_t ii = 0; ii < iNumCols; ++ii)
	{
		ColDef col;
		col.sName = kFormat("column_{}", ii);

		// vary value lengths and types to be realistic:
		// per 10 columns: 3 ints, 2 short strings (SSO), 2 long strings (~200 chars),
		// 1 boolean, 1 int64, 1 medium string
		switch (ii % 10)
		{
			case 0: // integer
			case 3:
			case 7:
				col.sValue  = kFormat("{}", 100000 + ii * 7);
				col.iFlags  = KCOL::NUMERIC;
				col.iMaxLen = 11;
				break;
			case 1: // short string (SSO range, < 22 bytes)
			case 6:
				col.sValue  = kFormat("val_{}", ii);
				col.iFlags  = KCOL::NOFLAG;
				col.iMaxLen = 64;
				break;
			case 2: // long string ~200 chars (varchar(255) - name/description)
			case 8:
				col.sValue  = kFormat("This is a realistic long database value for column {} "
				                      "that simulates a varchar(255) field like a product name, "
				                      "user description, address line, or comment text. "
				                      "Padding to about two hundred chars: {}", ii, ii * 31);
				col.iFlags  = KCOL::NOFLAG;
				col.iMaxLen = 255;
				break;
			case 4: // boolean
				col.sValue  = (ii & 1) ? "1" : "0";
				col.iFlags  = KCOL::BOOLEAN;
				col.iMaxLen = 1;
				break;
			case 5: // int64
				col.sValue  = kFormat("{}", uint64_t(1) << 40 | ii);
				col.iFlags  = static_cast<KCOL::Flags>(KCOL::NUMERIC | KCOL::INT64NUMERIC);
				col.iMaxLen = 20;
				break;
			case 9: // medium string (~50 chars, above SSO)
				col.sValue  = kFormat("medium length value for column number {}", ii);
				col.iFlags  = KCOL::NOFLAG;
				col.iMaxLen = 128;
				break;
		}
		Cols.push_back(std::move(col));
	}

	return Cols;
}

// replicate the cheap fingerprint from ksql.cpp's ComputeRowLayoutHash
static std::size_t ComputeRowLayoutHash(const KROW& Row)
{
	std::size_t iHash = Row.size();

	for (KROW::Index ii = 0; ii < Row.size(); ++ii)
	{
		const auto& elem = Row.at(ii);
		const auto& sName = elem.first;
		iHash += sName.size();
		if (!sName.empty())
		{
			iHash += static_cast<unsigned char>(sName.front()) << 8;
			iHash += static_cast<unsigned char>(sName.back());
		}
		iHash ^= static_cast<std::size_t>(elem.second.GetFlags()) << (ii & 15);
	}

	return iHash;
}

// simulate slow path: clear + AddCol for all columns (what old NextRow did)
static void SlowPath(KROW& Row, const std::vector<ColDef>& Cols)
{
	Row.clear();

	for (const auto& col : Cols)
	{
		Row.AddCol(col.sName, col.sValue, col.iFlags, col.iMaxLen);
	}
}

// simulate fast path: just overwrite sValue in place (what new NextRow does)
static void FastPath(KROW& Row, const std::vector<ColDef>& Cols)
{
	for (KROW::Index ii = 0; ii < Row.size(); ++ii)
	{
		Row.at(ii).second.sValue = Cols[ii].sValue;
	}
}

static void RunBench(uint32_t iNumCols, uint32_t iNumRows)
{
	auto Cols = MakeColumnDefs(iNumCols);

	KProf pGroup(Label(kFormat("-NextRow {} cols x {} rows", iNumCols, iNumRows)));

	// prepare a second set of values for alternating (to avoid same-value optimization)
	auto Cols2 = Cols;
	for (auto& col : Cols2)
	{
		col.sValue += "_v2";
	}

	// --- slow path benchmark ---
	{
		KROW Row;

		KProf pp(Label(kFormat("slow path ({} cols)", iNumCols)));
		pp.SetMultiplier(iNumRows);
		for (uint32_t row = 0; row < iNumRows; ++row)
		{
			KProf::Force(&Row);
			SlowPath(Row, (row & 1) ? Cols2 : Cols);
			KProf::Force(&Row);
		}
	}

	// --- fast path benchmark (with hash verification) ---
	{
		KROW Row;

		// prime the KROW with initial structure (first row is always slow path)
		SlowPath(Row, Cols);
		std::size_t iHash = ComputeRowLayoutHash(Row);

		KProf pp(Label(kFormat("fast path + hash ({} cols)", iNumCols)));
		pp.SetMultiplier(iNumRows);
		for (uint32_t row = 0; row < iNumRows; ++row)
		{
			KProf::Force(&Row);
			auto& CurrentCols = (row & 1) ? Cols2 : Cols;

			// verify hash (same as the real NextRow fast path check)
			std::size_t iCurrentHash = ComputeRowLayoutHash(Row);
			if (iCurrentHash == iHash && Row.size() == CurrentCols.size())
			{
				FastPath(Row, CurrentCols);
			}
			else
			{
				SlowPath(Row, CurrentCols);
				iHash = ComputeRowLayoutHash(Row);
			}
			KProf::Force(&Row);
		}
	}

	// --- fast path without hash (theoretical minimum) ---
	{
		KROW Row;

		// prime the KROW
		SlowPath(Row, Cols);

		KProf pp(Label(kFormat("fast path no hash ({} cols)", iNumCols)));
		pp.SetMultiplier(iNumRows);
		for (uint32_t row = 0; row < iNumRows; ++row)
		{
			KProf::Force(&Row);
			FastPath(Row, (row & 1) ? Cols2 : Cols);
			KProf::Force(&Row);
		}
	}

	// --- hash computation cost alone ---
	{
		KROW Row;
		SlowPath(Row, Cols);

		KProf pp(Label(kFormat("hash only ({} cols)", iNumCols)));
		pp.SetMultiplier(iNumRows);
		for (uint32_t row = 0; row < iNumRows; ++row)
		{
			KProf::Force(&Row);
			auto iHash2 = ComputeRowLayoutHash(Row);
			KProf::Force(&iHash2);
		}
	}
}

void krow_bench()
{
	// benchmark with different column counts
	RunBench( 5, 100000);
	RunBench(10, 100000);
	RunBench(20, 100000);
	RunBench(50,  50000);
}
