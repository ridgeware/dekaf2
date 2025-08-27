
# DEKAF CODING STANDARDS

## D001. All code must be indented by TABs, not spaces and not a mix of tabs and spaces.
The exception is when a code line is continued onto the next line: there should be tabs to match the prior line and then there should be spaces to make sure the second line of code lines up correctly with the first line.
The key thing is this: when someone changes their tab-stops from 4 to 8 or even to 2, indentation levels must remain perfect.  If the indentation levels get messed up, then there are spaces being used to indent the code instead of pure tabs.

## D002. All open-curlies must be on their own line.
For example, NOT THIS:

int main(int argc, char* argv[]) {

But THIS:

int main(int argc, char* argv[])
{

## D003. All function and method implementations should be denoted by horizontal bars (77 dashes) and a closing comment that matches the function or method name like this:

//-----------------------------------------------------------------------------
int SomeFunction (int argc, char* argv[])
//-----------------------------------------------------------------------------
{
	code here

} // SomeFunction

## D004. Class names (usually in header files) must also be denoted consistently like this:

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class BBAPIS : public KWebClient
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
//----------
public:
//----------
	public methods here
	
//----------
protected:
//----------
	protected methods here

//----------
private:
//----------
	private methods here

}; // BBAPIS -- XAPIS's interface to BBAPIS

You should minimize the number of sections in a class definition: do not start with "public", switch to "private" then back to "public".  That makes the code difficult to understand.


## D010. All source files must start with a standard copyright notice.
This header will depend on the project.   When you start a new project, this header section needs to be uniquely defined.
Ask the user for this header whenever starting a new project.

## D020. SQL Statements must be formatted very deliberately

We have very deliberate standards for SQL and for SQL statements embedded into code.

* we do not capitalize SQL keywords like SELECT, FROM, etc. as is often done
* we always capitalize TABLE NAMES because they are the most important part of a SQL statement
* we use lower_case and underscores for column names not camel case
* queries (select statements) are formatted with each keyword and each column expression on its own line like this:
		select distinct L.company_domain
		     , if(B.a_rank=0,100000001,ifnull(B.a_rank,100000001)) as alexa_rank
			 , B.num_employees as employees
			 , B.tx_vendor
		     , L.demo_screenshot
		  from XAPIS_LEAD      L
		  left outer
		  join XAPIS_BUILTWITH B on B.root_domain = L.company_domain
		 where L.demo_screenshot is not null
		 order by alexa_rank;

* DEKAF2::KSQL uses python-style placeholders in its SQL statement.  These are expanded at runtime by C++ and the SQL is transmitted to the RDBMS as a flattened string
* when SELECT statements (queries) are embedded into C++ code using KSQL, it is important to force physical newlines into the SQL that gets transmitted to the server.   This decision makes the C++ a tiny bit less readable, but it makes the statement much more loggable and debuggable:

	return (SingleIntQuery (
		"select 1\n"
		"  from {}\n"
		"  join {} on PT.translation_key = S.translation_key and PT.project_key = '{}'\n"
		"  where S.segment_hash = {}\n",
			FormTablename (XDB::XAPIS_SEGMENT,             "S", DEKAF2_FUNCTION_NAME),
			FormTablename (XDB::XAPIS_PROJECT_TRANSLATION, "PT"), GetProjectKey(),
			iHash) == 1);

		ApiExecQuery (
			"select BS.block_hash\n"   // query this tkey's segments for a single block
			"     , BS.segment_order\n"
			"     , BS.segment_hash\n"
			"     , BS.is_ignorable\n"
			"     , B.source        as block_source_jliff\n"
			"     , S.segment_jliff as source_jliff\n"
			"     , S.segment_text  as source_text\n"
			"     , S.approx_word_count\n"
			"     , ST.is_staging\n"
			"     , ST.target_jliff\n"
			"     , ST.target_text\n"
			"  from {}\n"
			"  join {} using (block_hash,   translation_key)\n"
			"  join {} using (segment_hash, translation_key)\n"
			"  left outer\n"
			"  join {}\n"
			"    on ST.segment_hash    = BS.segment_hash\n"
			"   and ST.translation_key = '{}' /*target-tkey*/ \n"
			"   and ST.is_staging      = (\n"
			"       select max(ST2.is_staging)\n"
			"         from {}\n"
			"        where ST2.segment_hash     = BS.segment_hash\n"
			"          and ST2.translation_key  = ST.translation_key\n"
			"          and ST2.is_staging      <= {}\n"
			"       )\n"
			"{}"
			" where BS.block_hash = {}\n"
			"   and BS.translation_key = '{}' /*linked-tkey*/ \n"
			" order by BS.segment_order",
				FormTablename (XDB::XAPIS_BLOCK_SEGMENT,       "BS", DEKAF2_FUNCTION_NAME),
				FormTablename (XDB::XAPIS_BLOCK,               "B"),
				FormTablename (XDB::XAPIS_SEGMENT,             "S"),
				FormTablename (XDB::XAPIS_SEGMENT_TRANSLATION, "ST"),
				GetTranslationKey(),
				FormTablename (XDB::XAPIS_SEGMENT_TRANSLATION, "ST2"),
				iMaxIsStaging,
				iMaxIsStaging ? "" : "   and ST.status_code in ('live', 'no translate')\n",
				iBlockHash,
				GetLinkedTKey());

* in C++ normally INSERTs, UPDATEs and DELETEs are managed by DEKAF2's KROW object like this:

		KROW row (db3.FormTablename (XDB::XAPIS_INTERPRETER_WEBSITE_LOG));
		row.AddCol ("website",             sWebsite,                 KCOL::PKEY,     20);
		row.AddCol (XDB::source_lang_code, sSourceLang,              KCOL::PKEY,     20);
		row.AddCol (XDB::target_lang_code, sTargetLang,              KCOL::PKEY,     20);
		row.AddCol ("num_results",         iNumResults,              KCOL::NUMERIC);
		row.AddCol ("took_seconds",        tTook,                    KCOL::NUMERIC);
		row.AddCol (XDB::status_code,      sStatus,                  KCOL::NOFLAG,   20);
		row.AddCol ("messages",            sMessages,                KCOL::NOFLAG);
		row.AddCol ("created_who",         sEmail,                   KCOL::PKEY,     50);
		row.AddCol (XDB::created_utc,      "{{NOW}}",                KCOL::PKEY | KCOL::EXPRESSION);
		if (!db3.Insert (row, /*bIgnoreDupes=*/true))
		{
			throw ApiError { ApiError::H5xx_ERROR, kFormat("{}\{}", db3.GetLastError(), db3.GetLastSQL()) };
		}

