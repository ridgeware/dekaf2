
/// @file kwebobjects_bench.cpp
/// Allocation + throughput benchmarks for KWebObjects HTML builders. Provides
/// the baseline numbers for the planned KHTMLDOM arena migration (see
/// notes/khtmldom-arena-design.md). Two scenarios:
///
/// 1. Large data table (1000 rows x 5 cells x 3 attributes) — bulk-build path.
/// 2. Form-heavy admin page — realistic UI shape with mixed element types.

#include <iostream>
#include <cinttypes>
#include <dekaf2/time/duration/kprof.h>
#include <dekaf2/core/strings/kstring.h>
#include <dekaf2/core/strings/kstringview.h>
#include <dekaf2/web/html/khtmldom.h>
#include <dekaf2/web/objects/kwebobjects.h>
#include "allocation_counter.h"

using namespace dekaf2;
using dekaf2bench::AllocationSnapshot;

// ----------------------------------------------------------------------------
// Scenario 1: large data table built via KWebObjects
// ----------------------------------------------------------------------------

void kwebobjects_big_table()
{
	{
		AllocationSnapshot a("KWebObjects table 1000x5x3 build x100");
		dekaf2::KProf ps("KWebObjects table 1000x5x3 build");
		ps.SetMultiplier(100);

		for (size_t count = 0; count < 100; ++count)
		{
			html::Table table;
			table.SetID("tbl1");
			table.SetClass(html::Class("data", ""));

			for (std::size_t row = 0; row < 1000; ++row)
			{
				auto& tr = table.Add(html::TableRow());
				tr.SetAttribute("data-row", KString::to_string(row));

				for (std::size_t col = 0; col < 5; ++col)
				{
					auto& td = tr.Add(html::TableData());
					td.SetAttribute("class",    "cell");
					td.SetAttribute("data-col", KString::to_string(col));
					td.SetAttribute("scope",    "col");
					td.AddText("cell content");
				}
			}

			KProf::Force(&table);
		}
	}

	{
		AllocationSnapshot a("KWebObjects table 1000x5x3 build+serialize x100");
		dekaf2::KProf ps("KWebObjects table build + serialize");
		ps.SetMultiplier(100);

		for (size_t count = 0; count < 100; ++count)
		{
			html::Table table;
			table.SetID("tbl1");

			for (std::size_t row = 0; row < 1000; ++row)
			{
				auto& tr = table.Add(html::TableRow());
				tr.SetAttribute("data-row", KString::to_string(row));

				for (std::size_t col = 0; col < 5; ++col)
				{
					auto& td = tr.Add(html::TableData());
					td.SetAttribute("class",    "cell");
					td.SetAttribute("data-col", KString::to_string(col));
					td.AddText("cell content");
				}
			}

			KString sOut = table.Print();
			KProf::Force(&sOut);
		}
	}
}

// ----------------------------------------------------------------------------
// Scenario 2: realistic admin-style page (Page + Form + various inputs)
// Modelled after utests/kwebobjects_tests.cpp's larger sample.
// ----------------------------------------------------------------------------

void kwebobjects_admin_page()
{
	{
		AllocationSnapshot a("KWebObjects admin page x500");
		dekaf2::KProf ps("KWebObjects admin page build");
		ps.SetMultiplier(500);

		for (size_t count = 0; count < 500; ++count)
		{
			html::Page page("Admin Console", "en");

			html::Class BodyClass("body", "font-family: sans-serif;");
			page.AddClass(BodyClass);

			auto& body = page.Body();

			// header
			{
				auto& div = body.Add(html::Div("HeaderDiv"));
				div.AddText("Welcome to the admin page");
				div.Add(html::LineBreak());
			}

			// nav links
			{
				auto& div = body.Add(html::Div("NavDiv"));
				for (int i = 0; i < 8; ++i)
				{
					auto& link = div.Add(html::Link(KString::to_string(i)));
					link.SetTitle("nav");
					link.AddText("link ");
					link.AddText(KString::to_string(i));
				}
			}

			// data table (10 rows)
			{
				auto& table = body.Add(html::Table());
				table.SetID("data");
				for (int row = 0; row < 10; ++row)
				{
					auto& tr = table.Add(html::TableRow());
					for (int col = 0; col < 4; ++col)
					{
						auto& td = tr.Add(html::TableData());
						td.SetAttribute("data-r", KString::to_string(row));
						td.SetAttribute("data-c", KString::to_string(col));
						td.AddText("v");
					}
				}
			}

			KProf::Force(&page);
		}
	}

	{
		AllocationSnapshot a("KWebObjects admin page+serialize x500");
		dekaf2::KProf ps("KWebObjects admin page build + serialize");
		ps.SetMultiplier(500);

		for (size_t count = 0; count < 500; ++count)
		{
			html::Page page("Admin Console", "en");

			auto& body = page.Body();

			{
				auto& div = body.Add(html::Div("HeaderDiv"));
				div.AddText("Welcome");
			}

			{
				auto& div = body.Add(html::Div("NavDiv"));
				for (int i = 0; i < 8; ++i)
				{
					auto& link = div.Add(html::Link(KString::to_string(i)));
					link.AddText("link ");
					link.AddText(KString::to_string(i));
				}
			}

			{
				auto& table = body.Add(html::Table());
				for (int row = 0; row < 10; ++row)
				{
					auto& tr = table.Add(html::TableRow());
					for (int col = 0; col < 4; ++col)
					{
						auto& td = tr.Add(html::TableData());
						td.AddText("v");
					}
				}
			}

			KString sOut = page.Print();
			KProf::Force(&sOut);
		}
	}
}

// ----------------------------------------------------------------------------
// Scenario 3: tiny but frequent — many short-lived single-element constructions
// (tests detached-element overhead, relevant for the lazy-mini-document idea
// in the design doc).
// ----------------------------------------------------------------------------

void kwebobjects_short_lived()
{
	{
		AllocationSnapshot a("KWebObjects detached small x100000");
		dekaf2::KProf ps("KWebObjects single-element churn");
		ps.SetMultiplier(100000);

		for (size_t count = 0; count < 100000; ++count)
		{
			html::Div div;
			div.SetID("d1");
			div.AddText("hello");
			KProf::Force(&div);
		}
	}
}

void kwebobjects_bench()
{
	kwebobjects_big_table();
	kwebobjects_admin_page();
	kwebobjects_short_lived();
}
