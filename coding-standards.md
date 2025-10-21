# DEKAF2 Coding Standards

This document outlines the coding conventions and style rules used in DEKAF2 coding.

## Rationale (K-Preface)

Code written with strict standards is more eaily maintainable by a team.

## IDE Templates

Most IDEs used for DEKAF2 projects have a shared template that bake in these rules. Apply the team template for your editor before starting a large change, and sync with the listed template owner when tweaks are needed.

## File Header (K103)

Every `.cpp` and `.h` file must begin with the established banner. When we vendor third-party sources, preserve their notice and add ours only when licensing allows.

## Newline Convention (K104)

Use Unix (LF) newlines for every text file and ensure each file ends with a trailing newline. Never commit Windows line endings.

Example DEKAF2 project banner:

```cpp
/*
 //
 // DEKAF(tm): Lighter, Faster, Smarter (tm)
 //
 // Copyright (c) 2025, Ridgeware, Inc.
 //
 // +-------------------------------------------------------------------------+
 // | /\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\|
 // |/+---------------------------------------------------------------------+/|
 // |/|                                                                     |/|
 // |\|  ** THIS NOTICE MUST NOT BE REMOVED FROM THE SOURCE CODE MODULE **  |\|
 // |/|                                                                     |/|
 // |\|   OPEN SOURCE LICENSE                                               |\|
 // |/|                                                                     |/|
 // |\|   Permission is hereby granted, free of charge, to any person       |\|
 // |/|   obtaining a copy of this software and associated                  |/|
 // |\|   documentation files (the "Software"), to deal in the              |\|
 // |/|   Software without restriction, including without limitation        |/|
 // |\|   the rights to use, copy, modify, merge, publish,                  |\|
 // |/|   distribute, sublicense, and/or sell copies of the Software,       |/|
 // |\|   and to permit persons to whom the Software is furnished to        |\|
 // |/|   do so, subject to the following conditions:                       |/|
 // |\|                                                                     |\|
 // |/|   The above copyright notice and this permission notice shall       |/|
 // |\|   be included in all copies or substantial portions of the          |\|
 // |/|   Software.                                                         |/|
 // |\|                                                                     |\|
 // |/|   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY         |/|
 // |\|   KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE        |\|
 // |/|   WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR           |/|
 // |\|   PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS        |\|
 // |/|   OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR          |/|
 // |\|   OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR        |\|
 // |/|   OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE         |/|
 // |\|   SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.            |\|
 // |/|                                                                     |/|
 // |/+---------------------------------------------------------------------+/|
 // |\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/ |
 // +-------------------------------------------------------------------------+
 */
```

## Indentation (K100)

- **Use TABS for leading indentation** (not spaces)
- Use spaces for alignment AFTER the initial tab indentation
- Never use tabs after text starts - once you've started writing code/text on a line, only use spaces for alignment
- Litmus test: teammates with a different tab stop still read perfectly aligned code

Example:
```cpp
void Function()
{
	auto pParms = std::make_shared<XDBParms>();
	auto pdb    = GetDB(pParms);  // spaces used to align = after tab indent
	
	if (condition)
	{
		DoSomething();
	}
}
```

The key principle: start each line with tabs for the indentation level, then switch to spaces when aligning elements horizontally (like the `=` signs above).

## Function Declarations and Definitions

### Function Separators (K102)

Use 80 dashes above and below each function definition, and close the scope with a `}// Name` comment.

```cpp
//--------------------------------------------------------------------------------
KStringView KBlockCipher::ToString (Algorithm algorithm)
//--------------------------------------------------------------------------------
{
	// function body

} // ToString
```

Keep triple-slash (`///`) documentation immediately above the signature; place the upper bar above those comments as allowed by K102.

### Function Call Spacing

Default to inserting a space between the callable name and the opening parenthesis. This applies to macro-style helpers such as `kDebug`, free functions, member calls, lambdas, and namespaced helpers.

```cpp
kDebug (1, "...");
ApiLogger (HTTP, pdb);
pdb->SetTranslationKey (sLinkedTKey);
VerifyHashString (sUrlHash);
kjson::Exists (oBlocks, sBlockHash);
```

Omit the space when the call has no arguments, or when it takes a single numeric literal or constant. Also omit the space when using KJSON's read-only call operator (which mirrors the subscript form):

```cpp
pdb->Get();
kjson::Exists (oBlocks, sBlockHash);
```

KJSON call operator (no space):
```cpp
auto sConfig = oTranslation(XDB::translation_config_hash);
auto sConfig = oTranslation[XDB::translation_config_hash];
```

Examples of incorrect spacing:

```cpp
kDebug(1, "...");
ApiLogger(HTTP, pdb);
pdb->SetTranslationKey(sLinkedTKey);
```

## Braces

### Placement (K101)

Put the opening brace on its own line for methods and for any control block that spans more than one statement. Always use braces, even for single-statement bodies.

```cpp
if (condition)
{
	DoWork();
}

if (otherCondition)
{
	return;
}
```

Incorrect patterns (brace on same line or missing braces altogether) must be rewritten.

```cpp
if (condition) {
	// wrong: brace shares line
}

if (condition)
	singleStatement(); // wrong: missing braces
```

### Lambda Expressions

Braces follow the same placement rule (opening brace on its own line):

```cpp
auto BuildClause = [&](KStringView sAlias)
{
	if (sTargetKeys)
	{
		return pdb->FormAndClause(...);
	}
	return pdb->FormatSQL(...);
};
```

## Variable Declarations and Alignment

### Align = Signs Vertically

When declaring multiple related variables, align the `=` signs:

```cpp
auto pParms      = std::make_shared<XDBParms>();
auto pdb         = GetDB(pParms);
auto sLinkedTKey = HTTP.GetQueryParmSafe (":translation_key");
auto sUrlHash    = HTTP.GetQueryParmSafe (":content_url_hash");
```

### Align Types and Names in Structured Data

For field definitions and structured arrays:

```cpp
static constexpr XFieldMap Fields[] = {
	// ---------------            ----------------            ---------------  -----------   ---------------   ------
	// JSON FIELD                 DB COL                      INSERT DEFAULT   INSERT REQ    DB FLAGS          MAXLEN
	// ---------------            ----------------            ---------------  -----------   ---------------   ------
	{ "email",                    "email",                    "",              true,         KCOL::NOFLAG,      250 },
	{ "first_name",               "first_name",               "",              false,        KCOL::NOFLAG,      250 },
	{ "last_name",                "last_name",                "",              false,        KCOL::NOFLAG,      250 },
	{ END,                        "",                         "",              false,        KCOL::NOFLAG,        0 }
};
```

### Boolean Variables (K203)

Use `b` prefix for booleans:

```cpp
bool bHasCTXMethod = sTxMethods.contains(TXM_CTX);
bool bHasTMMethod  = sTxMethods.contains(TXM_TM);
```

### Integer Variables (K203)

Use `i` prefix for integers:

```cpp
uint64_t iUrlHash        = sUrlHash.UInt64();
uint64_t iContentUrlHash = HTTP.GetQueryParmSafe ("ctx_url_hash").UInt64();
```

### Floats and Double (K203)

Use `n` prefix for floating point number:

```cpp
double nDollarAmount;
float  nPercent{100.0};
```

### String Variables (K203)

Use `s` prefix for strings and string-views:

```cpp
auto sLinkedTKey = HTTP.GetQueryParmSafe (":translation_key");
auto sUrlHash    = HTTP.GetQueryParmSafe (":content_url_hash");
```

### Pointer Variables (K203)

Use `p` prefix for pointers:

```cpp
auto pParms = std::make_shared<XDBParms>();
auto pdb    = GetDB(pParms);
auto pdbRW  = GetDB(pParms);
```

Avoid adding `const` to the smart pointer when calling helpers such as `GetDB`. The connector cache expects a mutable `std::shared_ptr<XDBParms>&` so it can swap in the configured connection; a `const` qualifier will not compile.

## Naming Conventions (K200-K203)

### Class and Struct Names (K200)

Adopt the project prefixing convention: `K` for DEKAF2 types, etc. Class names start with an uppercase letter; never begin a class with lowercase. `K` in DEKAF2 is used consistently. DEKAF2 standalone functions use `k` like `kWrite` or `kSplit`.

### Method and Function Names (K201)

Methods and free functions use PascalCase (`ApiTranslate`, `GetCookie`). Avoid lowercase-leading names that look like Hungarian variable prefixes.

### Special Scope Prefixes (K202)

Use prefixes to surface scope: `m_` for members, `s_` for local statics, and `g_` for globals (rare).

```cpp
void XClass::SetCount (int iCount)
{
	m_iCount = iCount;
}

int NextCounter ()
{
	static int s_iValue{0};
	return ++s_iValue;
}
```

### Hungarian Prefix Reference (K203)

Variables carry a succinct prefix to hint at type and intent.

| Prefix | Meaning             | Example             |
| ------ | ------------------- | ------------------- |
| `b`    | boolean             | `bIsActive`         |
| `i`    | integer / index     | `iCount`, `iiIndex` |
| `n`    | floating-point      | `nPrice`            |
| `s`    | string              | `sName`             |
| `o`    | object wrapper      | `oJSON`             |
| `fn`   | function / callable | `fnComplete`        |
| `p`    | pointer / smart ptr | `pdb`, `pParms`     |
| `m_`   | member field        | `m_sUserKey`        |
| `s_`   | local static        | `s_iValue`          |
| `g_`   | global              | `g_Config` (avoid)  |
| `ii`   | looping variable    | `ii`                |
| `jj`   | looping variable    | `jj`                |
| `kk`   | looping variable    | `kk`                |

Keep the existing boolean, integer, string, and pointer examples consistent with these prefixes.

## Spacing

### Around Operators

Space around binary operators:

```cpp
int result = a + b;              // ✓ Correct
uint64_t total = count * size;  // ✓ Correct

int result=a+b;                  // ✗ Incorrect
```

### No Space Before Semicolons

```cpp
DoSomething();                   // ✓ Correct
return value;                    // ✓ Correct

DoSomething() ;                  // ✗ Incorrect
```

### Function Call Arguments

Space after commas:

```cpp
kDebug (1, "...");                                    // ✓ Correct
pdb->FormatSQL("{}.{}", sAlias, XDB::translation_key) // ✓ Correct

kDebug(1,"...");                                      // ✗ Incorrect
```

## Comments

### Section Dividers

Use dashed lines for major section breaks:

```cpp
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
```

### Inline Comments

Use `//` for single-line comments, with space after slashes:

```cpp
auto sTargetKeys = HTTP.GetQueryParmSafe ("target_keys"); // optional
pdb->SetTranslationKey (sLinkedTKey);                     // throws on bad tkey
```

### Block Comments

Use `/* */` for multi-line documentation:

```cpp
/*
 * Sample input:
 *
 * {
 *   "url": "https://example.com/",
 *   "text": [ ... ]
 * }
 */
```

For large temporary blocks, especially JSON fixtures, you can also wrap the region in a pre-processor guard:

```cpp
#if 0 // expected JSON
{
	"translation_source": "..."
	// fields trimmed for brevity
}
#endif
```

## SQL Formatting (K300-K306)

### Keyword Case (K300)

Lowercase SQL keywords (`select`, `from`, `join`) for readability against uppercase table names and indexes.

```cpp
"select distinct\n"
"       BS.block_hash,\n"
"       BS.segment_order,\n"
"       BS.segment_hash\n"
"  from {}\n"
"  join {} on CBA.block_hash = BS.block_hash\n"
" where CBA.linked_translation_key = '{}'\n"
"   and CBA.content_url_hash       = {}\n"
```

### Layout and Vertical Gutter (K301 & K306)

Align commas and operators to form a visible gutter down the statement. Break long lists so each column or clause lives on its own line.

```sql
select  UP.user_key
      , U.email
      , U.first_name
      , U.last_name
      , U.company_name
from    USER_PROJECT UP
join    USER U using (user_key)
where   UP.project_key = '89F0-F9E7-86CF-0AEF'
order by UP.user_key;
```

### Table Aliases (K302)

Alias tables and derived subqueries with uppercase abbreviations that mirror the table name (`UP` for `USER_PROJECT`, `PS` for a `page_segments` CTE, etc.). Coordinate conflicts with the team and stay consistent so column references like `PS.approx_word_count` always match the alias casing.

### Column References (K302b)

Use the `XDB::` column macros for column names in SQL queries, keeping them on one line or one line per table. This keeps column names consistent and reduces the chance of SQL injection bugs.

```cpp
pdb->FormatSQL(
	"select {}, {}, {}\n"
	"  from {}\n"
	" where {} = '{}'\n",
		XDB::user_key, XDB::email, XDB::first_name,
		pdb->FormTablename(XDB::USER, "U"),
		XDB::user_key,
		sUserKey);
```

Or for multiple tables:
```cpp
pdb->FormatSQL(
	"select UP.{}, U.{}\n"
	"  from {}\n"
	"  join {} using ({})\n",
		XDB::project_key, XDB::email,
		pdb->FormTablename(XDB::USER_PROJECT, "UP"),
		pdb->FormTablename(XDB::USER, "U"),
		XDB::user_key);
```

### Preferred Join Syntax (K303)

Use explicit `join` clauses to keep conditions out of the `where` block and make switches between inner/outer joins painless. Prefer `using` when the join keys share the same name.

```sql
join PERF_DATA_LATEST PDL using (internal_ip)
join SERVER_NAME      S   using (internal_ip)
```

### Embedding Dynamic SQL (K304)

When building SQL in code, embed newlines and spacing so debug logging prints a readable statement. Substitution parameters should be indented one tab more than the SQL string literals.

```cpp
pdb->FormatSQL(
	"select distinct\n"
	"       BS.block_hash,\n"
	"       BS.segment_hash\n"
	"  from {}\n"
	"  join {} on CBA.block_hash = BS.block_hash\n"
	" where CBA.linked_translation_key = '{}'\n"
	"   and BS.translation_key         = '{}'\n",
		pdb->FormTablename(XDB::BLOCK_SEGMENT, "BS", DEKAF2_FUNCTION_NAME),
		pdb->FormTablename(XDB::CONTENT_BLOCK_ACTIVE, "CBA"),
		sLinkedTKey,
		sLinkedTKey);
```

For subqueries, indent the SQL keywords appropriately within the parent query structure:

```cpp
pdb->FormatSQL(
	"select W.segment_hash\n"
	"  from (\n"
	"    select ST.segment_hash\n"
	"      from {}\n"
	"     where ST.translation_key in (\n"
	"       select {} from {}\n"
	"        where {}  = '{}'\n"
	"          and {} <> '{}'\n"
	"     )\n"
	"  ) W\n",
		pdb->FormTablename(XDB::SEGMENT_TRANSLATION, "ST"),
		XDB::translation_key,
		pdb->FormTablename(XDB::PROJECT_TRANSLATION, "PT", DEKAF2_FUNCTION_NAME),
		XDB::linked_translation_key,
		sLinkedTKey,
		XDB::translation_key,
		sLinkedTKey);
```

When a helper returns a fragment that is spliced into a larger statement (for example, a reusable `where` clause), adjust the leading whitespace so keywords such as `and` line up beneath the parent `where`. If the same fragment is embedded at multiple depths, re-indent it before each use so the debug output stays aligned. This keeps the assembled SQL readable in debug logs as well as in the source.

### DDL and Maintenance Statements (K305 & K306)

DDL, insert, update, and delete statements follow the same tab/space rules (K100) and method separators (K102) when embedded in code. Use the vertical gutter style once the parameter list gets long, and upgrade any debug one-liners as soon as they stop being trivial.

## Throw Statements

### Exception Formatting

Use brace initialization for KHTTPError, with a space before the opening brace:

```cpp
throw KHTTPError { KHTTPError::H4xx_BADREQUEST, "missing required parameters" };
throw KHTTPError { KHTTPError::H5xx_ERROR, db.GetLastError() };

// With kFormat:
throw KHTTPError { KHTTPError::H4xx_BADREQUEST, kFormat ("Invalid content_url_hash={}", sUrlHash) };
```

## Conditional Compilation

### Debug Statements

Use `kDebug` with numeric levels:

```cpp
kDebug (1, "...");                         // Level 1 - function entry and important information commonly needed
kDebug (2, "target[jliff]: {}", jliff);    // Level 2 - enough info to understand what's going on, but not redundant info
kDebug (3, "raw data: {}", rawData);       // Level 3 - any info that might be useful, including raw data dumps
```

Level guidelines:
- **Level 1**: Function entry and important information commonly needed for debugging
- **Level 2**: Enough info to understand what's going on, but avoid redundant information
- **Level 3**: Only used in deperation for crashes and infinite loop detection

## Namespace Usage

### Anonymous Namespaces

For file-local helper functions:

```cpp
namespace
{
void HelperFunction (XDBPointer& pdbRW, KStringView sParam);
}

// Implementation
namespace
{
//--------------------------------------------------------------------------------
void HelperFunction (XDBPointer& pdbRW, KStringView sParam)
//--------------------------------------------------------------------------------
{
	// implementation
}
} // anonymous namespace
```

## Auto Keyword

Use `auto` liberally with clear variable names:

```cpp
auto  sTxMethods     = kjson::GetStringRef (pdb->GetProjectTranslationRecord(), "translation_methods").ToUpper();
const auto sUrlHash  = HTTP.GetQueryParm (":content_url_hash");
auto  iUrlHash       = sUrlHash.UInt64();
auto  iCount         = oRow["count"].UInt16();
```

**Exception for static values**: We cannot use `auto` for scalar integers because the compiler might pick too small of a precision.  Here are examples of misuse of auto:

```cpp
auto iCount {0};        // <-- no precision information
auto iNumItems {100};   // <-- no precision information
```

## JSON Handling

### KJSON Object Construction

Use initializer lists:

```cpp
KJSON seg = {
	{XDB::segment_hash, row[XDB::segment_hash]},
	{XDB::approx_word_count, row[XDB::approx_word_count].UInt64()},
	{"is_leveraged", row["is_leveraged_any"].UInt16()}
};
```

### JSON Field Access

```cpp
HTTP.json.tx["linked_translation_key"]       = sLinkedTKey;
HTTP.json.tx["content_url_hash"]             = iUrlHash;
HTTP.json.tx["totals"]["blocks"]             = blocks_count;
HTTP.json.tx["totals"]["words"]["total"]     = words_total;
```

### KJSON API Patterns (K400-K403)

**K400: Use kjson::Parse() helper instead of KJSON::Parse() template**

```cpp
// ✓ Correct - use lowercase helper
KJSON json = kjson::Parse(sContent);

// ✗ Incorrect - uppercase is template, more complex
KJSON json = KJSON::Parse(sContent);
```

**K401: Use Select() method for field access instead of get<T>()**

```cpp
// ✓ Correct - Select() returns KString directly
KString sName = json.Select("collection_name");

// ✗ Incorrect - get<T>() is private in KJSON2
KString sName = json["collection_name"].get<KString>();
```

**K402: Use typed accessors for numeric values**

```cpp
// ✓ Correct - use typed accessors
std::int64_t iSize = json.Select("size_bytes").Int64();
bool bActive = json.Select("is_active").Bool();
std::size_t iLength = json.Select("max_length").UInt64();

// ✗ Incorrect - get<T>() is private
std::int64_t iSize = json["size_bytes"].get<std::int64_t>();
bool bActive = json["is_active"].get<bool>();
```

**K403: Use proper iteration pattern for KJSON objects**

```cpp
// ✓ Correct - use iterator with key()/value() methods
for (const auto& it : json["table_schemas"].items())
{
	auto sTableName = it.key();
	auto oTableData = it.value();
	
	// Process sTableName and oTableData...
}

// ✗ Incorrect - structured bindings fail with private members
for (const auto& [tableName, tableData] : json["table_schemas"].items())
{
	// This will not compile due to private member access
}
```

## Error Handling

### Validation Pattern

Check parameters early and throw immediately:

```cpp
if (sLinkedTKey.empty() || sUrlHash.empty())
{
	throw KHTTPError { KHTTPError::H4xx_BADREQUEST, "missing required parameters" };
}
if (!VerifyHashString(sUrlHash))
{
	throw KHTTPError { KHTTPError::H4xx_BADREQUEST, kFormat ("Invalid content_url_hash={}", sUrlHash) };
}
```

## Line Length

- Prefer keeping lines under 120 characters
- Break long SQL statements into multiple lines
- Break long function calls with many arguments into multiple lines

## Include Order

1. Local headers (`"kstring.h"`)
2. External library headers (`<openssl/...>`, `<uuid/...>`)
3. Standard library headers (if any)

```cpp
#include "kstring.h"
#include <openssl/evp.h>
```

## Summary Checklist

- [ ] Use TABS for indentation and spaces for alignment (K100)
- [ ] Space after macro helpers (`kDebug (...)`) and align `=` in related declarations
- [ ] Apply Hungarian prefixes (`b`, `i`, `s`, `p`, plus `m_`, `s_`, `g_`)
- [ ] Place braces on their own line and wrap functions with 80-dash bars + `} // Name` (K101/K102)
- [ ] Keep ASCII art headers and Unix newlines with a trailing LF (K103/K104)
- [ ] Enforce PascalCase on classes and methods (K200/K201)
- [ ] Format SQL with lowercase keywords, uppercase aliases, and the vertical gutter (K300-K306)
- [ ] Space around binary operators and after commas
- [ ] Use `auto` with descriptive names
- [ ] Throw KHTTPError with brace initialization (space before `{`)
- [ ] Use proper KJSON API patterns: `kjson::Parse()`, `Select()`, typed accessors, iterator pattern (K400-K403)
