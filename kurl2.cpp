///////////////////////////////////////////////////////////////////////////////
//
// DEKAF(tm): Lighter, Faster, Smarter(tm)
//
// Copyright (c) 2000-2017, Ridgeware, Inc.
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
//
///////////////////////////////////////////////////////////////////////////////

#include "kurl2.h"

namespace hermes
{

string KURL::s_mailto("mailto");

//-----------------------------------------------------------------------------
// Service function
bool complaint (const string& message, const char* file, size_t line)
//-----------------------------------------------------------------------------
{
	puts(format("{}[{}]: {}", file, line, message).c_str());
	return false;
}

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
//-----------------------------------------------------------------------------
KProto::KProto ()
    : m_scheme ("")
//-----------------------------------------------------------------------------
{
}

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
//-----------------------------------------------------------------------------
KDomain::KDomain ()
    : m_user     (""),
      m_pword    (""),
      m_host     (""),
      m_domain   ("")
//-----------------------------------------------------------------------------
{
}

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
//-----------------------------------------------------------------------------
KPort::KPort ()
    : m_port ("")
//-----------------------------------------------------------------------------
{
}

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
// kurl2 delimiter statics

//-----------------------------------------------------------------------------
// CTOR
KURL::KURL (const string& URL) :
    m_path     (""),
    m_query    (""),
    m_frag     ("")
//-----------------------------------------------------------------------------
{
	    parse (URL);
}

//-----------------------------------------------------------------------------
// DTOR
KURL::~KURL ()
//-----------------------------------------------------------------------------
{
	    // Allocation uses unique_ptr for auto-free
}

//-----------------------------------------------------------------------------
// TOP-LEVEL ELEMENT DISPLAY (unparsed query)
void KURL::report () const
//-----------------------------------------------------------------------------
{
	const string as ("\t   \"{}\"\t: \"{}\"\n");
	puts (format (as, "scheme", m_scheme.size () ? m_scheme : "?").c_str ());
	puts (format (as, "user"  , m_user  .size() ? m_user   : "?").c_str());
	puts (format (as, "pass"  , m_pword .size() ? m_pword  : "?").c_str());
	puts (format (as, "host"  , m_host  .size() ? m_host   : "?").c_str());
	puts (format (as, "domain", m_domain.size() ? m_domain : "?").c_str());
	puts (format (as, "port"  , m_port  .size() ? m_port   : "?").c_str());
	puts (format (as, "path"  , m_path  .size() ? m_path   : "?").c_str());
	puts (format (as, "query" , m_query .size() ? m_query  : "?").c_str());
	puts (format (as, "frag"  , m_frag  .size() ? m_frag   : "?").c_str());
}

//-----------------------------------------------------------------------------
// REASSEMBLE URL FROM ATOMIC ELEMENTS
bool KURL::serialize (string& URL) const
//-----------------------------------------------------------------------------
{
	URL.clear ();

	URL += m_scheme.size() ?      m_scheme : ""   ;
	URL += m_isMailto      ?           ":" : "://";
	URL += m_user  .size() ?        m_user : ""   ;
	URL += m_pword .size() ? ":" + m_pword : ""   ;
	URL += m_user  .size() ?           "@" : ""   ;
	URL += m_host  .size() ?        m_host : ""   ;
	URL += m_port  .size() ? ":" + m_port  : ""   ;
	URL += m_path  .size() ? "/" + m_path  : ""   ;
	URL += m_query .size() ? "?" + m_query : ""   ;
	URL += m_frag  .size() ? "#" + m_frag  : ""   ;
	return true;
}

//-----------------------------------------------------------------------------
bool KURL::parse (const string& URL)
//-----------------------------------------------------------------------------
{
	// TODO:JDL@JS Distribute parsing into parent classes?
	// This function has more lines of code than the standard permits.
	// TODO break this up into a series of inline functions
	// TODO distribute them into parent classes and call from here.
	bool result = false;

	if (URL.size() == 0)
	{
		return complaint ("empty URL", __FILE__, __LINE__);
	}

	// cursors is a vector of pointers to delimiters in a URL.
	// The delimiter sequence is the key analysis structure.
	// It is used throughout processing to disambiguate fields.
	vector<size_t> cursors;
	cut(URL, cursors);
	size_t delimiters = cursors.size();
	size_t index = 1;            // Setup for disassembling URL

	if (!parseScheme(m_scheme, URL, cursors, index))
	{
		return false;
	}
	m_isMailto = (m_scheme == s_mailto);

	parseUserPass(m_user, m_pword, URL, cursors, index);

	if (!parseHostPortDomain(m_host, m_port, m_domain, URL, cursors, index))
	{
		return false;	// At least host is required
	}

	if (!parsePath(m_path, URL, cursors, index))
	{
		// Ignore absence
	}

	size_t afterQuery = delimiters - 1;
	if (!parseFragment(m_path, URL, cursors, afterQuery))
	{
		// Ignore absence
	}

	if (!parseQuery(m_query, URL, cursors, index, afterQuery))
	{
		// Ignore absence
	}

	return result;
}

        /*
	// Lex the query string into key:val pairs
	if (m_query.size())
	{
			 * Delimiter '&' separates encoded key=value pairs.
			 * Divide a source string into "<encodedKey>=<encodedVal>" fields.
			 * Divide a field into {"<encodedKey>", "<encodedVal>"} pairs.
			 * For each <encoded> member of the pair run decode (inline).
			 * This avoids problems if decode is run before delimiter splitting.
			 * Store key=value pairs using unique_ptrs.
			   vector<const char*> qursors;
			   size_t iKey = 0, iVal = 0;
			   for (size_t ii = 0; ii < m_query.size (); ++ii)
			   {
			   if (m_query[ii] == '=')
			   {
			   iVal = ii;
			   qursors.push_back (m_query + ii);
			   }
			   else if (m_query[ii] == '&')
			   {
			   qursors.push_back (m_query + ii);
			   ups sKey
			   allocate (sKey, iKey, iVal);
			   allocate (m_KeyVal[sKey], iVal + ii);
			   }
			   }
	}
			   */

} // namespace hermes

#ifdef __KURL_MAIN__
//-----------------------------------------------------------------------------
void test (const char* candidate)
//-----------------------------------------------------------------------------
{
	static const char* passfail[2] = { "[PASS]", "[FAIL]" };
	printf ("       original: \"%s\"\n", candidate ? candidate : "null ptr");

	hermes::KURL kurl (candidate);
	std::string target;
	kurl.serialize (target);
	const char* restoring = target.c_str();

	printf ("%s restored: \"%s\"\n", passfail[candidate == restoring], restoring);

	kurl.report ();
}

//-----------------------------------------------------------------------------
int main (int argc, char** argv)
//-----------------------------------------------------------------------------
{
	    const char* empty = nullptr;
		test (empty);
		test ("");
		test ("garbage");
		test ("3.141592");
		test ("It was a dark and stormy night.");
		test ("google.com");
		test ("jonathan.lettvin.com");
		test ("mailto:jlettvin@google.com");
		test ("ftp://google.com");
		test ("http://google.com");
		test ("https://google.com");
		test ("http://go@subsub.sub.DOMAIN.co.uk");
		test ("http://go:fish@subsub.sub.DOMAIN.co.uk");
		test ("http://go:fish@subsub.sub.DOMAIN.co.uk?hello=world");
		test ("http://go:fish@subsub.sub.DOMAIN.co.uk?hello=world&hola=mundo");
		test ("http://go:fish@subsub.sub.domain.co.uk?hello=world&hola=mundo#piece");
		test ("http://go.and:fish@subsub.sub.DOMAIN.co.uk#piece");

		return 0;
}
#endif//__KURL_MAIN__
