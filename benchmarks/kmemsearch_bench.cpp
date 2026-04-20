/*
 * Microbenchmark for dekaf2::memrchr and dekaf2::memmem
 *
 * These two functions are the innermost primitives used by KStringView::rfind(),
 * kRFind(), kReplace(), and every caller of memmem. On macOS / Apple Silicon,
 * dekaf2 supplies its own implementation because:
 *   - Apple libc has no memrchr
 *   - Apple libc memmem is documented to be ~100x slower than glibc's
 *
 * This benchmark isolates the two primitives from higher-level callers so we
 * can directly measure the effect of a NEON-optimized implementation.
 */

#include <cstring>
#include <cinttypes>
#include <string>
#include <dekaf2/time/duration/kprof.h>
#include <dekaf2/core/strings/kstring.h>
#include <dekaf2/core/strings/kstringview.h>
#include <dekaf2/core/strings/bits/kstring_view.h>

using namespace dekaf2;

namespace {

// place a char at every iEvery-th position to stress the first-byte filter in memmem
void SetEvery(std::string& sStr, char ch, std::string::size_type iEvery)
{
	for (std::size_t iPos = iEvery; iPos < sStr.size(); iPos += iEvery)
	{
		sStr[iPos] = ch;
	}
}

} // anon

void kmemsearch_bench()
{
	// =============================================================
	// memrchr benchmarks
	//
	// A reverse memchr-equivalent. Current ARM fallback is a pure
	// scalar byte-by-byte loop. Good target for NEON.
	// =============================================================
	{
		dekaf2::KProf ps("-memrchr");

		// -------------------------------------------------------------
		// rare char, search hits near the start (worst case for reverse scan)
		// -------------------------------------------------------------
		{
			std::string s(200, '-');
			s[3] = 'd';
			dekaf2::KProf prof("memrchr hit@3 (200)");
			prof.SetMultiplier(200000);
			for (int ct = 0; ct < 200000; ++ct)
			{
				KProf::Force(s.data());
				auto* p = DEKAF2_PREFIX memrchr(s.data(), 'd', s.size());
				if (p == nullptr) KProf::Force();
			}
		}
		{
			std::string s(5000, '-');
			s[3] = 'd';
			dekaf2::KProf prof("memrchr hit@3 (5000)");
			prof.SetMultiplier(200000);
			for (int ct = 0; ct < 200000; ++ct)
			{
				KProf::Force(s.data());
				auto* p = DEKAF2_PREFIX memrchr(s.data(), 'd', s.size());
				if (p == nullptr) KProf::Force();
			}
		}
		{
			std::string s(5000000, '-');
			s[3] = 'd';
			dekaf2::KProf prof("memrchr hit@3 (5M)");
			prof.SetMultiplier(200);
			for (int ct = 0; ct < 200; ++ct)
			{
				KProf::Force(s.data());
				auto* p = DEKAF2_PREFIX memrchr(s.data(), 'd', s.size());
				if (p == nullptr) KProf::Force();
			}
		}

		// -------------------------------------------------------------
		// rare char, search hits near the end (best case for reverse scan)
		// -------------------------------------------------------------
		{
			std::string s(200, '-');
			s[s.size() - 4] = 'd';
			dekaf2::KProf prof("memrchr hit@end (200)");
			prof.SetMultiplier(200000);
			for (int ct = 0; ct < 200000; ++ct)
			{
				KProf::Force(s.data());
				auto* p = DEKAF2_PREFIX memrchr(s.data(), 'd', s.size());
				if (p == nullptr) KProf::Force();
			}
		}
		{
			std::string s(5000, '-');
			s[s.size() - 4] = 'd';
			dekaf2::KProf prof("memrchr hit@end (5000)");
			prof.SetMultiplier(200000);
			for (int ct = 0; ct < 200000; ++ct)
			{
				KProf::Force(s.data());
				auto* p = DEKAF2_PREFIX memrchr(s.data(), 'd', s.size());
				if (p == nullptr) KProf::Force();
			}
		}
		{
			std::string s(5000000, '-');
			s[s.size() - 4] = 'd';
			dekaf2::KProf prof("memrchr hit@end (5M)");
			prof.SetMultiplier(200);
			for (int ct = 0; ct < 200; ++ct)
			{
				KProf::Force(s.data());
				auto* p = DEKAF2_PREFIX memrchr(s.data(), 'd', s.size());
				if (p == nullptr) KProf::Force();
			}
		}

		// -------------------------------------------------------------
		// miss (scans the whole buffer)
		// -------------------------------------------------------------
		{
			std::string s(200, '-');
			dekaf2::KProf prof("memrchr miss (200)");
			prof.SetMultiplier(200000);
			for (int ct = 0; ct < 200000; ++ct)
			{
				KProf::Force(s.data());
				auto* p = DEKAF2_PREFIX memrchr(s.data(), 'Z', s.size());
				if (p != nullptr) KProf::Force();
			}
		}
		{
			std::string s(5000, '-');
			dekaf2::KProf prof("memrchr miss (5000)");
			prof.SetMultiplier(200000);
			for (int ct = 0; ct < 200000; ++ct)
			{
				KProf::Force(s.data());
				auto* p = DEKAF2_PREFIX memrchr(s.data(), 'Z', s.size());
				if (p != nullptr) KProf::Force();
			}
		}
		{
			std::string s(5000000, '-');
			dekaf2::KProf prof("memrchr miss (5M)");
			prof.SetMultiplier(200);
			for (int ct = 0; ct < 200; ++ct)
			{
				KProf::Force(s.data());
				auto* p = DEKAF2_PREFIX memrchr(s.data(), 'Z', s.size());
				if (p != nullptr) KProf::Force();
			}
		}
	}

	// =============================================================
	// memmem benchmarks
	//
	// Current approach: std::memchr for first byte + std::memcmp to
	// confirm. Good when first byte is rare, pathological when first
	// byte is common.
	// =============================================================
	{
		dekaf2::KProf ps("-memmem");

		const char* n2  = "de";        // 2-byte needle
		const char* n8  = "abcdefgh";  // 8-byte needle
		const char* n16 = "abcdefghijklmnop"; // 16-byte needle
		const char* n64 = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789AB";

		// -------------------------------------------------------------
		// rare first byte, small needle, hit at end (ideal case)
		// -------------------------------------------------------------
		{
			std::string s(200, '-');
			s.append(n8);
			dekaf2::KProf prof("memmem 8-byte hit@end (200)");
			prof.SetMultiplier(200000);
			for (int ct = 0; ct < 200000; ++ct)
			{
				KProf::Force(s.data());
				auto* p = DEKAF2_PREFIX memmem(s.data(), s.size(), n8, 8);
				if (p == nullptr) KProf::Force();
			}
		}
		{
			std::string s(5000, '-');
			s.append(n8);
			dekaf2::KProf prof("memmem 8-byte hit@end (5000)");
			prof.SetMultiplier(200000);
			for (int ct = 0; ct < 200000; ++ct)
			{
				KProf::Force(s.data());
				auto* p = DEKAF2_PREFIX memmem(s.data(), s.size(), n8, 8);
				if (p == nullptr) KProf::Force();
			}
		}
		{
			std::string s(5000000, '-');
			s.append(n8);
			dekaf2::KProf prof("memmem 8-byte hit@end (5M)");
			prof.SetMultiplier(200);
			for (int ct = 0; ct < 200; ++ct)
			{
				KProf::Force(s.data());
				auto* p = DEKAF2_PREFIX memmem(s.data(), s.size(), n8, 8);
				if (p == nullptr) KProf::Force();
			}
		}

		// -------------------------------------------------------------
		// 2-byte needle
		// -------------------------------------------------------------
		{
			std::string s(5000, '-');
			s.append(n2);
			dekaf2::KProf prof("memmem 2-byte hit@end (5000)");
			prof.SetMultiplier(200000);
			for (int ct = 0; ct < 200000; ++ct)
			{
				KProf::Force(s.data());
				auto* p = DEKAF2_PREFIX memmem(s.data(), s.size(), n2, 2);
				if (p == nullptr) KProf::Force();
			}
		}
		{
			std::string s(5000000, '-');
			s.append(n2);
			dekaf2::KProf prof("memmem 2-byte hit@end (5M)");
			prof.SetMultiplier(200);
			for (int ct = 0; ct < 200; ++ct)
			{
				KProf::Force(s.data());
				auto* p = DEKAF2_PREFIX memmem(s.data(), s.size(), n2, 2);
				if (p == nullptr) KProf::Force();
			}
		}

		// -------------------------------------------------------------
		// 16-byte needle
		// -------------------------------------------------------------
		{
			std::string s(5000, '-');
			s.append(n16);
			dekaf2::KProf prof("memmem 16-byte hit@end (5000)");
			prof.SetMultiplier(200000);
			for (int ct = 0; ct < 200000; ++ct)
			{
				KProf::Force(s.data());
				auto* p = DEKAF2_PREFIX memmem(s.data(), s.size(), n16, 16);
				if (p == nullptr) KProf::Force();
			}
		}
		{
			std::string s(5000000, '-');
			s.append(n16);
			dekaf2::KProf prof("memmem 16-byte hit@end (5M)");
			prof.SetMultiplier(200);
			for (int ct = 0; ct < 200; ++ct)
			{
				KProf::Force(s.data());
				auto* p = DEKAF2_PREFIX memmem(s.data(), s.size(), n16, 16);
				if (p == nullptr) KProf::Force();
			}
		}

		// -------------------------------------------------------------
		// 64-byte needle (beyond 16-byte NEON chunk)
		// -------------------------------------------------------------
		{
			std::string s(5000, '-');
			s.append(n64);
			dekaf2::KProf prof("memmem 64-byte hit@end (5000)");
			prof.SetMultiplier(200000);
			for (int ct = 0; ct < 200000; ++ct)
			{
				KProf::Force(s.data());
				auto* p = DEKAF2_PREFIX memmem(s.data(), s.size(), n64, 64);
				if (p == nullptr) KProf::Force();
			}
		}
		{
			std::string s(5000000, '-');
			s.append(n64);
			dekaf2::KProf prof("memmem 64-byte hit@end (5M)");
			prof.SetMultiplier(200);
			for (int ct = 0; ct < 200; ++ct)
			{
				KProf::Force(s.data());
				auto* p = DEKAF2_PREFIX memmem(s.data(), s.size(), n64, 64);
				if (p == nullptr) KProf::Force();
			}
		}

		// -------------------------------------------------------------
		// miss (scans entire haystack)
		// -------------------------------------------------------------
		{
			std::string s(5000, '-');
			dekaf2::KProf prof("memmem miss (5000)");
			prof.SetMultiplier(200000);
			for (int ct = 0; ct < 200000; ++ct)
			{
				KProf::Force(s.data());
				auto* p = DEKAF2_PREFIX memmem(s.data(), s.size(), n8, 8);
				if (p != nullptr) KProf::Force();
			}
		}
		{
			std::string s(5000000, '-');
			dekaf2::KProf prof("memmem miss (5M)");
			prof.SetMultiplier(200);
			for (int ct = 0; ct < 200; ++ct)
			{
				KProf::Force(s.data());
				auto* p = DEKAF2_PREFIX memmem(s.data(), s.size(), n8, 8);
				if (p != nullptr) KProf::Force();
			}
		}

		// -------------------------------------------------------------
		// worst case: haystack full of needle[0] → memcmp on every position
		// -------------------------------------------------------------
		{
			std::string s(5000, 'a');
			s.append(n8);
			dekaf2::KProf prof("memmem 8-byte worst (5000)");
			prof.SetMultiplier(200000);
			for (int ct = 0; ct < 200000; ++ct)
			{
				KProf::Force(s.data());
				auto* p = DEKAF2_PREFIX memmem(s.data(), s.size(), n8, 8);
				if (p == nullptr) KProf::Force();
			}
		}
		{
			std::string s(5000000, 'a');
			s.append(n8);
			dekaf2::KProf prof("memmem 8-byte worst (5M)");
			prof.SetMultiplier(200);
			for (int ct = 0; ct < 200; ++ct)
			{
				KProf::Force(s.data());
				auto* p = DEKAF2_PREFIX memmem(s.data(), s.size(), n8, 8);
				if (p == nullptr) KProf::Force();
			}
		}

		// -------------------------------------------------------------
		// semi-adversarial: first byte repeats every 50 chars
		// -------------------------------------------------------------
		{
			std::string s(5000, '-');
			SetEvery(s, 'a', 50);
			s.append(n8);
			dekaf2::KProf prof("memmem 8-byte every50 (5000)");
			prof.SetMultiplier(200000);
			for (int ct = 0; ct < 200000; ++ct)
			{
				KProf::Force(s.data());
				auto* p = DEKAF2_PREFIX memmem(s.data(), s.size(), n8, 8);
				if (p == nullptr) KProf::Force();
			}
		}
		{
			std::string s(5000000, '-');
			SetEvery(s, 'a', 50);
			s.append(n8);
			dekaf2::KProf prof("memmem 8-byte every50 (5M)");
			prof.SetMultiplier(200);
			for (int ct = 0; ct < 200; ++ct)
			{
				KProf::Force(s.data());
				auto* p = DEKAF2_PREFIX memmem(s.data(), s.size(), n8, 8);
				if (p == nullptr) KProf::Force();
			}
		}
	}

	// =============================================================
	// 1-char needle variants via KStringView API
	//
	// KStringView::find_first_of(char)    -> kFind(ch)    -> memchr  [libc NEON]
	// KStringView::find_last_of(char)     -> kRFind(ch)   -> memrchr [dekaf2 NEON]
	// KStringView::find_first_not_of(char) -> kFindNot(ch)           [scalar]
	// KStringView::find_last_not_of(char)  -> kRFindNot(ch)          [dekaf2 NEON]
	// =============================================================
	{
		dekaf2::KProf ps("-KStringView 1-char find");

		// Haystack is all '-', with a single 'x' at the very end. For find_*_of
		// this means: miss path walks entire buffer, then hit at end/start.
		// For find_*_not_of this means: every byte except one is a mismatch,
		// so the result is at offset 0 / haystack.size()-1 - very short path.
		// We therefore build dedicated haystacks for each query direction so
		// the full buffer is scanned.
		{
			std::string s(5000000, '-');
			s.back() = 'x';
			KStringView sv(s);
			dekaf2::KProf prof("find_first_of(ch) 5M hit@end");
			prof.SetMultiplier(200);
			for (int ct = 0; ct < 200; ++ct)
			{
				KProf::Force(s.data());
				if (sv.find_first_of('x') == KStringView::npos) KProf::Force();
			}
		}
		{
			std::string s(5000000, '-');
			s.front() = 'x';
			KStringView sv(s);
			dekaf2::KProf prof("find_last_of(ch) 5M hit@start");
			prof.SetMultiplier(200);
			for (int ct = 0; ct < 200; ++ct)
			{
				KProf::Force(s.data());
				if (sv.find_last_of('x') == KStringView::npos) KProf::Force();
			}
		}
		{
			// All bytes are '-', we look for a char that is NOT '-' -> scans
			// the whole buffer and returns npos.
			std::string s(5000000, '-');
			KStringView sv(s);
			dekaf2::KProf prof("find_first_not_of(ch) 5M miss");
			prof.SetMultiplier(200);
			for (int ct = 0; ct < 200; ++ct)
			{
				KProf::Force(s.data());
				if (sv.find_first_not_of('-') != KStringView::npos) KProf::Force();
			}
		}
		{
			std::string s(5000000, '-');
			KStringView sv(s);
			dekaf2::KProf prof("find_last_not_of(ch) 5M miss");
			prof.SetMultiplier(200);
			for (int ct = 0; ct < 200; ++ct)
			{
				KProf::Force(s.data());
				if (sv.find_last_not_of('-') != KStringView::npos) KProf::Force();
			}
		}

		// Short haystacks to see per-call overhead
		{
			std::string s(5000, '-');
			KStringView sv(s);
			dekaf2::KProf prof("find_first_not_of(ch) 5000 miss");
			prof.SetMultiplier(200000);
			for (int ct = 0; ct < 200000; ++ct)
			{
				KProf::Force(s.data());
				if (sv.find_first_not_of('-') != KStringView::npos) KProf::Force();
			}
		}
		{
			std::string s(5000, '-');
			KStringView sv(s);
			dekaf2::KProf prof("find_last_not_of(ch) 5000 miss");
			prof.SetMultiplier(200000);
			for (int ct = 0; ct < 200000; ++ct)
			{
				KProf::Force(s.data());
				if (sv.find_last_not_of('-') != KStringView::npos) KProf::Force();
			}
		}
	}
}
