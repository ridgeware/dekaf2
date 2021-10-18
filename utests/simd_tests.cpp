#include "catch.hpp"

#include <dekaf2/bits/kcppcompat.h>
#include <dekaf2/bits/simd/kfindfirstof.h>
#include <dekaf2/kexception.h>

#ifndef DEKAF2_IS_WINDOWS

#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/mman.h>

using namespace dekaf2;

namespace {

/// provides a pointer to a page that is framed by protected pages to make
/// sure we get a SIGSEGV when tripping page borders
class Protected
{
public:

	Protected()
	: iPageSize(sysconf(_SC_PAGE_SIZE))
	{
		if (iPageSize < 0)
		{
			throw KError { "cannot get page size" };
		}

		if (posix_memalign(&pAligned, iPageSize, 3 * iPageSize) != 0)
		{
			throw KError { "cannot get aligned buffer" };
		}

		pProtected = static_cast<char*>(pAligned);

		if (pProtected == nullptr)
		{
			throw KError { "cannot get buffer" };
		}

		memset(pProtected + iPageSize * 0, '1', iPageSize);
		memset(pProtected + iPageSize * 1, '2', iPageSize);
		memset(pProtected + iPageSize * 2, '3', iPageSize);

		if (mprotect(pProtected + iPageSize * 0, iPageSize, PROT_NONE) == -1)
		{
			throw KError { "cannot protect page" };
		}

		if (mprotect(pProtected + iPageSize * 2, iPageSize, PROT_NONE) == -1)
		{
			throw KError { "cannot protect page" };
		}

		pProtected += iPageSize;
	}

	~Protected()
	{
		pProtected -= iPageSize;
		mprotect(pProtected + iPageSize * 0, iPageSize, PROT_READ | PROT_WRITE | PROT_EXEC);
		mprotect(pProtected + iPageSize * 2, iPageSize, PROT_READ | PROT_WRITE | PROT_EXEC);
		free(pAligned);
	}

	const char* Address() const
	{
		return pProtected;
	}

	std::size_t PageSize() const
	{
		return iPageSize;
	}

private:
	void*       pAligned   = nullptr;
	char*       pProtected = nullptr;
	std::size_t iPageSize  = -1;

}; // Protected

} // end of anonymous namespace

// speed the permutation tests up by an order of magnitude..
#define CCHECK(x) if ((x) == false) CHECK(x)

TEST_CASE("SIMD")
{
	SECTION("find_first_of")
	{
		// instance of our protected page
		Protected Page;

		for (std::size_t iStrLen = 1; iStrLen <= 64; ++iStrLen)
		{
			for (std::size_t iOffset = 0; iOffset < Page.PageSize() - iStrLen; ++iOffset)
			{
				KStringView sString(Page.Address() + iOffset, iStrLen);

				CCHECK ( sString.find(  '3') == KStringView::npos );
				CCHECK ( sString.find("333") == KStringView::npos );
				CCHECK ( sString.find_first_of("13") == KStringView::npos );
				CCHECK ( sString.find_first_of("20") != KStringView::npos );
				CCHECK ( sString.find_first_of("---------------------------13") == KStringView::npos );
				CCHECK ( sString.find_first_of("---------------------------20") != KStringView::npos );

				CCHECK ( sString.rfind(  '1') == KStringView::npos );
				CCHECK ( sString.rfind("111") == KStringView::npos );
				CCHECK ( sString.find_last_of("13") == KStringView::npos );
				CCHECK ( sString.find_last_of("20") != KStringView::npos );
				CCHECK ( sString.find_last_of("---------------------------13") == KStringView::npos );
				CCHECK ( sString.find_last_of("---------------------------20") != KStringView::npos );

				CCHECK ( sString.find_first_not_of("20") == KStringView::npos );
				CCHECK ( sString.find_first_not_of("13") != KStringView::npos );
				CCHECK ( sString.find_first_not_of("---------------------------20") == KStringView::npos );
				CCHECK ( sString.find_first_not_of("---------------------------13") != KStringView::npos );

				CCHECK ( sString.find_last_not_of("20") == KStringView::npos );
				CCHECK ( sString.find_last_not_of("13") != KStringView::npos );
				CCHECK ( sString.find_last_not_of("---------------------------20") == KStringView::npos );
				CCHECK ( sString.find_last_not_of("---------------------------13") != KStringView::npos );
			}
		}
	}
}

#endif // DEKAF2_IS_WINDOWS
