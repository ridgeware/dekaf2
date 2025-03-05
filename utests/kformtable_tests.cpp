#include "catch.hpp"
#include <dekaf2/kformtable.h>

using namespace dekaf2;

TEST_CASE("KFormTable")
{
	SECTION("ASCII")
	{
		static constexpr KStringView sExpected =
R"(
+------------+------------+----------------------+-------+----+
| Col1       |    Col2    |         Col3         |  Col4 | Co>|
+------------+------------+----------------------+-------+----+
|     123.45 |    344     |          12          |     0 |    |
+------------+------------+----------------------+-------+----+
)";
		KString sOut;
		sOut += '\n'; // compensate for the leading newline in the sExpected string

		KFormTable Table(sOut,
		{
			10,
			{ 10, KFormTable::Center },
			{ 20, KFormTable::Center | KFormTable::Wrap },
			{  5, KFormTable::Right  },
			2
		});

		CHECK ( Table.GetStyle() == KFormTable::Style::Box );
		CHECK ( Table.ColCount() == 5 );

		Table.PrintRow( { "Col1", "Col2", "Col3", "Col4", "Col5" } );
		Table.PrintSeparator();
		Table.PrintColumn(123.45);
		Table.PrintColumn(344);
		Table.PrintColumn(chrono::seconds(12));
		Table.PrintColumn(false);
		Table.Close();

		CHECK ( sOut == sExpected );
	}

	SECTION("Hidden")
	{
		static constexpr KStringView sExpected =
R"(
 Col1           Col2             Col3           Col4  Co>

     123.45     344               12               0     
)";
		KString sOut;
		sOut += '\n'; // compensate for the leading newline in the sExpected string

		KFormTable Table(sOut,
		{
			10,
			{ 10, KFormTable::Center },
			{ 20, KFormTable::Center | KFormTable::Wrap },
			{  5, KFormTable::Right  },
			2
		});

		Table.SetStyle(KFormTable::Style::Hidden);
		CHECK ( Table.ColCount() == 5 );

		Table.PrintRow( { "Col1", "Col2", "Col3", "Col4", "Col5" } );
		Table.PrintSeparator();
		Table.PrintColumn(123.45);
		Table.PrintColumn(344);
		Table.PrintColumn(chrono::seconds(12));
		Table.PrintColumn(false);
		Table.Close();

		CHECK ( sOut == sExpected );
	}

	SECTION("Block")
	{
		static constexpr KStringView sExpected =
R"(
╭────────────┬────────────┬──────────────────────┬───────┬────╮
│ Col1       │    Col2    │         Col3         │  Col4 │ Co>│
├────────────┼────────────┼──────────────────────┼───────┼────┤
│     123.45 │    344     │          12          │     0 │    │
╰────────────┴────────────┴──────────────────────┴───────┴────╯
)";
		KString sOut;
		sOut += '\n'; // compensate for the leading newline in the sExpected string

		KFormTable Table(sOut,
		{
			10,
			{ 10, KFormTable::Center },
			{ 20, KFormTable::Center | KFormTable::Wrap },
			{  5, KFormTable::Right  },
			2
		});

		Table.SetBoxStyle(KFormTable::BoxStyle::Rounded);
		CHECK ( Table.ColCount() == 5 );

		Table.PrintRow( { "Col1", "Col2", "Col3", "Col4", "Col5" } );
		Table.PrintSeparator();
		Table.PrintColumn(123.45);
		Table.PrintColumn(344);
		Table.PrintColumn(chrono::seconds(12));
		Table.PrintColumn(false);
		Table.Close();

		CHECK ( sOut == sExpected );
	}

	SECTION("HTML")
	{
		static constexpr KStringView sExpected =
R"(
<table>
	<tr>
		<td>Col1</td>
		<td align=center>Col2</td>
		<td align=center>Col3</td>
		<td align=right>Col4</td>
		<td>Co</td>
	</tr>
	<tr>
		<td align=right>123.45</td>
		<td align=center>344</td>
		<td align=center>12</td>
		<td align=right>0</td>
		<td></td>
	</tr>
</table>
)";
		KString sOut;
		sOut += '\n'; // compensate for the leading newline in the sExpected string

		KFormTable Table(sOut,
		{
			10,
			{ 10, KFormTable::Center },
			{ 20, KFormTable::Center | KFormTable::Wrap },
			{  5, KFormTable::Right  },
			2
		});

		Table.SetStyle(KFormTable::Style::HTML);
		CHECK ( Table.ColCount() == 5 );

		Table.PrintRow( { "Col1", "Col2", "Col3", "Col4", "Col5" } );
		Table.PrintSeparator();
		Table.PrintColumn(123.45);
		Table.PrintColumn(344);
		Table.PrintColumn(chrono::seconds(12));
		Table.PrintColumn(false);
		Table.Close();

		CHECK ( sOut == sExpected );
	}

	SECTION("DryMode")
	{
		std::vector<std::vector<KStringView>> Data
		{
			{ "one"  , "two", "three"               },
			{ "one"  , "two", "three"  , "four"     },
			{ "once" , "two", "threely"             },
			{ "oncer", "two", "threely", "fourteen" },
		};

		static constexpr KStringView sExpected =
R"(
+-------+-----+---------+----------+
| one   | two | three   |          |
| one   | two | three   | four     |
| once  | two | threely |          |
| oncer | two | threely | fourteen |
+-------+-----+---------+----------+
)";

		KString sOut;
		sOut += '\n'; // compensate for the leading newline in the sExpected string

		KFormTable Table(sOut);

		// learn column sizes
		Table.DryMode(true);

		for (auto& row : Data)
		{
			Table.PrintRow(row);
		}

		// switch to output mode
		Table.DryMode(false);

		for (auto& row : Data)
		{
			Table.PrintRow(row);
		}

		Table.Close();

		CHECK ( sOut == sExpected );
	}

	SECTION("JSON two-dimensional array")
	{
		KJSON Data
		{
			{ "one"  , "two", "three"               },
			{ "one"  , "two", "three"  , "four"     },
			{ "once" , "two", "threely"             },
			{ "oncer", "two", "threely", "fourteen" },
		};

		static constexpr KStringView sExpected =
R"(
+-------+-----+---------+----------+
| one   | two | three   |          |
| one   | two | three   | four     |
| once  | two | threely |          |
| oncer | two | threely | fourteen |
+-------+-----+---------+----------+
)";

		KString sOut;
		sOut += '\n'; // compensate for the leading newline in the sExpected string

		KFormTable Table(sOut);

		// learn column sizes
		Table.DryMode(true);
		Table.PrintJSON(Data);
		// switch to output mode
		Table.DryMode(false);
		Table.PrintJSON(Data);

		Table.Close();

		CHECK ( sOut == sExpected );
	}

	SECTION("JSON array of objects")
	{
		KJSON Data
		{
			{
				{ "Name3", "three" },
				{ "Name2", "two"   },
				{ "Name1", "one"   }
			},
			{
				{ "Name1", "one"   },
				{ "Name2", "two"   },
				{ "Name3", "three" },
				{ "Name4", "four"  },
				{ "Name5", 123     }
			},
			{
				{ "Name1", "once"    },
				{ "Name2", "two"     },
				{ "Name3", "threely" }
			},
			{
				{ "Name1", "oncer"    },
				{ "Name2", "two"      },
				{ "Name3", "threely"  },
				{ "Name4", "fourteen" },
				{ "Name6", 12345.678  }
			}
		};

		static constexpr KStringView sExpected =
R"(
+-------+--------+---------+----------+-------+-----------+
| First | Middle | Last    | Name4    | Name5 |     Name6 |
+-------+--------+---------+----------+-------+-----------+
| one   | two    | three   |          |       |           |
| one   | two    | three   | four     |   123 |           |
| once  | two    | threely |          |       |           |
| oncer | two    | threely | fourteen |       | 12345.678 |
+-------+--------+---------+----------+-------+-----------+
)";

		KString sOut;
		sOut += '\n'; // compensate for the leading newline in the sExpected string

		KFormTable Table(sOut);
		Table.SetColNameAs("Name1", "First");
		Table.SetColNameAs("Name2", "Middle");
		Table.SetColNameAs("Name3", "Last");
		Table.PrintJSON(Data);

		Table.Close();

		CHECK ( sOut == sExpected );
	}

	SECTION("JSON limited array of objects")
	{
		KJSON Data
		{
			{
				{ "Name3", "three" },
				{ "Name2", "two"   },
				{ "Name1", "one"   }
			},
			{
				{ "Name1", "one"   },
				{ "Name2", "two"   },
				{ "Name3", "three" },
				{ "Name4", "four"  },
				{ "Name5", 123     }
			},
			{
				{ "Name1", "once"    },
				{ "Name2", "two"     },
				{ "Name3", "threely" }
			},
			{
				{ "Name1", "oncer"    },
				{ "Name2", "two"      },
				{ "Name3", "threely"  },
				{ "Name4", "fourteen" },
				{ "Name6", 12345.678  }
			}
		};

		static constexpr KStringView sExpected =
R"(
+-------+-------+---------+---------+
| Name1 | Name2 | Name3   | Name4   |
+-------+-------+---------+---------+
| one   | two   | three   |         |
| one   | two   | three   | four    |
| once  | two   | threely |         |
| oncer | two   | threely | fourtee>|
+-------+-------+---------+---------+
)";

		KString sOut;
		sOut += '\n'; // compensate for the leading newline in the sExpected string

		KFormTable Table(sOut);

		// learn column sizes
		Table.DryMode(true);
		Table.PrintJSON(Data);
		// switch to output mode
		Table.DryMode(false);
		// limit after creation of coldefs
		Table.SetMaxColCount(4);
		Table.SetMaxColWidth(7);
		// print
		Table.PrintJSON(Data);

		Table.Close();

		CHECK ( sOut == sExpected );
	}

	SECTION("JSON array of objects to HTML")
	{
		KJSON Data
		{
			{
				{ "Name1", "one"   },
				{ "Name2", "two"   },
				{ "Name3", "three" }
			},
			{
				{ "Name1", "one\""  },
				{ "Name2", "two>"   },
				{ "Name3", "&three" },
				{ "Name4", "<four"  }
			},
			{
				{ "Name1", "once"    },
				{ "Name2", "two"     },
				{ "Name3", "threely" }
			},
			{
				{ "Name1", "oncer"    },
				{ "Name2", "two"      },
				{ "Name3", "threely"  },
				{ "Name4", "fourteen" },
				{ "Name5", 12345.678  }
			}
		};

		static constexpr KStringView sExpected =
R"(
<table>
	<tr>
		<th>Name1</th>
		<th>Name2</th>
		<th>Name3</th>
		<th>Name4</th>
		<th align=right>Name5</th>
	</tr>
	<tr>
		<td>one</td>
		<td>two</td>
		<td>three</td>
		<td></td>
		<td align=right></td>
	</tr>
	<tr>
		<td>one&quot;</td>
		<td>two&gt;</td>
		<td>&amp;three</td>
		<td>&lt;four</td>
		<td align=right></td>
	</tr>
	<tr>
		<td>once</td>
		<td>two</td>
		<td>threely</td>
		<td></td>
		<td align=right></td>
	</tr>
	<tr>
		<td>oncer</td>
		<td>two</td>
		<td>threely</td>
		<td>fourteen</td>
		<td align=right>12345.678</td>
	</tr>
</table>
)";

		KString sOut;
		sOut += '\n'; // compensate for the leading newline in the sExpected string

		KFormTable Table(sOut);
		Table.SetStyle(KFormTable::Style::HTML);

		// learn column sizes
		Table.DryMode(true);
		Table.PrintJSON(Data);
		// switch to output mode
		Table.DryMode(false);
		Table.PrintJSON(Data);

		Table.Close();

		CHECK ( sOut == sExpected );
	}

	SECTION("two-dimensional array into CSV")
	{
		KJSON Data
		{
			{ "one"  , "two", "three"                },
			{ "one"  , "two", "three"   , "four"     },
			{ "once" , "two", "threely,"             },
			{ "oncer", "two", "threely" , "fourteen" },
		};

		static constexpr KStringView sExpected =
R"(
first,second,third,fourth
one,two,three,
one,two,three,four
once,two,"threely,",
oncer,two,threely,fourteen
)";

		KString sOut;
		sOut += '\n'; // compensate for the leading newline in the sExpected string

		KFormTable Table(sOut);
		Table.SetStyle(KFormTable::Style::CSV);
		Table.SetColName(0, "first");
		Table.SetColName(1, "second");
		Table.SetColName(2, "third");
		Table.SetColName(3, "fourth");
		Table.PrintJSON(Data);
		Table.Close();

		// CSV outputs with canonical lf, reduce it to a simple lf
		sOut.Replace("\r\n", "\n");
		CHECK ( sOut == sExpected );
	}

	SECTION("array of objects into CSV")
	{
		KJSON Data
		{
			{
				{ "Name1", "one"   },
				{ "Name2", "two"   },
				{ "Name3", "three" }
			},
			{
				{ "Name1", "one"    },
				{ "Name2", "two"    },
				{ "Name3", "thr,ee" },
				{ "Name4", "four"   }
			},
			{
				{ "Name1", "once"    },
				{ "Name2", "two"     },
				{ "Name3", "threely" }
			},
			{
				{ "Name1", "oncer"    },
				{ "Name2", "two"      },
				{ "Name3", "threely"  },
				{ "Name4", "fourteen" },
				{ "Name5", 12345.678  }
			}
		};

		static constexpr KStringView sExpected =
R"(
Name1,Name2,Name3,Name4,Name5
one,two,three,,
one,two,"thr,ee",four,
once,two,threely,,
oncer,two,threely,fourteen,12345.678
)";

		KString sOut;
		sOut += '\n'; // compensate for the leading newline in the sExpected string

		KFormTable Table(sOut);
		Table.SetStyle(KFormTable::Style::CSV);
		Table.PrintJSON(Data);
		Table.Close();

		// CSV outputs with canonical lf, reduce it to a simple lf
		sOut.Replace("\r\n", "\n");
		CHECK ( sOut == sExpected );
	}

	SECTION("two-dimensional array into JSON")
	{
		std::vector<std::vector<KStringView>> Data
		{
			{ "one"  , "two", "three"               },
			{ "one"  , "two", "three"  , "four"     },
			{ "once" , "two", "threely"             },
			{ "oncer", "two", "threely", "fourteen" },
		};

		static constexpr KStringView sExpected =
R"(
[["one","two","three"],["one","two","three","four"],["once","two","threely"],["oncer","two","threely","fourteen"]]
)";

		KString sOut;
		sOut += '\n'; // compensate for the leading newline in the sExpected string

		KFormTable Table(sOut);
		Table.SetStyle(KFormTable::Style::JSON);

		for (auto& row : Data)
		{
			Table.PrintRow(row);
		}

		Table.Close();

		sOut += '\n'; // compensate for the trailing newline in the sExpected string
		CHECK ( sOut == sExpected );
	}

	SECTION("array of objects into JSON")
	{
		KJSON Data
		{
			{
				{ "Name1", "one"   },
				{ "Name2", "two"   },
				{ "Name3", "three" }
			},
			{
				{ "Name1", "one"    },
				{ "Name2", "two"    },
				{ "Name3", "three"  },
				{ "Name4", "four"   }
			},
			{
				{ "Name1", "once"    },
				{ "Name2", "two"     },
				{ "Name3", "threely" }
			},
			{
				{ "Name1", "oncer"    },
				{ "Name2", "two"      },
				{ "Name3", "threely"  },
				{ "Name4", "fourteen" },
				{ "Name5", 12345.678  }
			}
		};

		static constexpr KStringView sExpected =
R"(
[
{"Name1":"one","Name2":"two","Name3":"three"},
{"Name1":"one","Name2":"two","Name3":"three","Name4":"four"},
{"Name1":"once","Name2":"two","Name3":"threely"},
{"Name1":"oncer","Name2":"two","Name3":"threely","Name4":"fourteen","Name5":"12345.678"}
]
)";

		KString sOut;

		KFormTable Table(sOut);
		Table.SetStyle(KFormTable::Style::JSON);
		Table.PrintJSON(Data);
		Table.Close();

		KString sExp = sExpected;
		sExp.Replace("\n", "");
		CHECK ( sOut == sExp );
	}

	SECTION("array of objects into JSON object")
	{
		KJSON Data
		{
			{
				{ "Name1", "one"   },
				{ "Name2", "two"   },
				{ "Name3", "three" }
			},
			{
				{ "Name1", "one"    },
				{ "Name2", "two"    },
				{ "Name3", "three"  },
				{ "Name4", "four"   }
			},
			{
				{ "Name1", "once"    },
				{ "Name2", "two"     },
				{ "Name3", "threely" }
			},
			{
				{ "Name1", "oncer"    },
				{ "Name2", "two"      },
				{ "Name3", "threely"  },
				{ "Name4", "fourteen" },
				{ "Name5", 12345.678  }
			}
		};

		static constexpr KStringView sExpected =
R"(
[
{"Name1":"one","Name2":"two","Name3":"three"},
{"Name1":"one","Name2":"two","Name3":"three","Name4":"four"},
{"Name1":"once","Name2":"two","Name3":"threely"},
{"Name1":"oncer","Name2":"two","Name3":"threely","Name4":"fourteen","Name5":"12345.678"}
]
)";

		KJSON jOut;
		KFormTable Table(jOut);
		Table.SetStyle(KFormTable::Style::JSON);
		Table.PrintJSON(Data);
		Table.Close();

		KString sOut = jOut.dump();
		KString sExp = sExpected;
		sExp.Replace("\n", "");
		CHECK ( sOut == sExp );
	}
}
