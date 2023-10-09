
#include <memory>
#include <iostream>
#include <vector>
#include <cinttypes>
#include <atomic>
#include <thread>
#include <dekaf2/kprof.h>
#include <dekaf2/kstring.h>
#include <dekaf2/ksharedref.h>

using dekaf2::KString;
using dekaf2::KStringView;
using PKString = std::shared_ptr<KString>;
using PKStringView = std::shared_ptr<KStringView>;
using std::shared_ptr;
using std::make_shared;
using std::vector;
using namespace dekaf2;


class MyString2
{
	friend class MyStringView2;
public:

	template<class... Args>
	MyString2(Args... args)
	{
		m_rep = make_shared<KString>(std::forward<Args>(args)...);
	}
	void release()
	{
	}

protected:

private:
	shared_ptr<KString> m_rep;
};

class MyStringView2
{
public:
	MyStringView2() = default;

	MyStringView2(MyString2& sBuf)
	    : m_rep(sBuf)
	    , m_view(&(*m_rep.m_rep.get())[0], m_rep.m_rep.get()->size())
	{
	}
	MyStringView2(MyString2& sBuf, size_t iStart, size_t iEnd)
	    : m_rep(sBuf)
	    , m_view(&(*m_rep.m_rep.get())[iStart], iEnd-iStart)
	{
	}

private:
	MyString2 m_rep;
	KStringView m_view;
};


void shared_ptr_bench()
{
	KString sPage;
	sPage.reserve(60000);
	constexpr KStringView::size_type STRSIZE = 1000;
	for (uint32_t ct = 0; ct < 1000; ++ct)
	{
		sPage += "1234567889012345678901234567890123456789012345678901234567890\r\n";
	}
	KProf profiler("-StringViews");
	{
		for (uint32_t ct = 0; ct < 100000; ++ct)
		{
			KString sBuffer(sPage);
			vector<KStringView> ViewVec;
			ViewVec.reserve(100);

			KProf profiler("views without shared_pointers");

			for (uint32_t xct = 0; xct < 100; ++xct)
			{
				ViewVec.emplace_back(&sBuffer[xct*50], STRSIZE);
			}
		}
	}
	{
		for (uint32_t ct = 0; ct < 100000; ++ct)
		{
			PKString sBuffer = make_shared<KString>(sPage);
			vector<PKStringView> ViewVec;
			ViewVec.reserve(100);

			KProf profiler("views with shared_pointers");

			for (uint32_t xct = 0; xct < 100; ++xct)
			{
				KStringView sv(sBuffer->data()+xct*50, STRSIZE);
				ViewVec.emplace_back(sBuffer, std::move(&sv));
			}
		}
	}
	{
		for (uint32_t ct = 0; ct < 100000; ++ct)
		{
			using MyView = dekaf2::KDependant<KString, KStringView, false>;
			vector<MyView> ViewVec;
			ViewVec.reserve(100);
			MyView::shared_type sBuffer(sPage);

			KProf profiler("views with custom shared_pointers");

			for (uint32_t xct = 0; xct < 100; ++xct)
			{
				ViewVec.emplace_back(sBuffer, KStringView(sBuffer->data() + xct*50, STRSIZE));
			}
		}
	}
	{
		for (uint32_t ct = 0; ct < 100000; ++ct)
		{
			MyString2 sBuffer(sPage);
			vector<MyStringView2> ViewVec;
			ViewVec.reserve(100);

			KProf profiler("views with custom shared_pointer wrapper");

			for (uint32_t xct = 0; xct < 100; ++xct)
			{
				ViewVec.emplace_back(sBuffer, xct*50, STRSIZE);
			}
		}
	}
	{
		for (uint32_t ct = 0; ct < 100000; ++ct)
		{
			KString sBuffer(sPage);
			vector<KString> ViewVec;
			ViewVec.reserve(100);

			KProf profiler("simple strings");

			for (uint32_t xct = 0; xct < 100; ++xct)
			{
				ViewVec.emplace_back(&sBuffer[xct*50], STRSIZE);
			}
		}
	}
}


