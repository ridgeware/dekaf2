#include <cstdlib>
#include <cmath>
#include <vector>

#include <fmt/format.h>

#include "kurl.h"

using dekaf2::KString;
using std::experimental::string_view;
using fmt::format;
using std::vector;

bool unimplemented(const KString& name, const char* __file__, size_t __line__)
{
	static const char* how{"{}[{}]: {} (unimplemented)"};
	puts (format(how, __file__, __line__, name.c_str()).c_str());
	return false;
}

//||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||
// KProto
//||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||

//..............................................................................
dekaf2::KProto::KProto ()
    : m_error{false}, m_mailto{false}, m_proto{""}
//..............................................................................
{
}

//..............................................................................
dekaf2::KProto::KProto (const KString& source)
    : m_error{false}, m_mailto{false}, m_proto{""}
//..............................................................................
{
	// Inefficient CTOR compared with the one having hint as a formal arg.
	size_t hint{0};
	parse(source, hint);
}

//..............................................................................
//..............................................................................
dekaf2::KProto::KProto (const KString& source, size_t& hint)
    : m_error{false}, m_mailto{false}, m_proto{""}
//..............................................................................
{
	parse(source, hint);
}

//..............................................................................
/// @brief RFC3986 3.1: scheme = ALPHA *( ALPHA / DIGIT / "+" / "-" / "." )
/// Implementation: We take all characters until ':'.
bool dekaf2::KProto::parse(const KString& source, size_t& hint)
//..............................................................................
{
	clear();

	size_t index{hint};
	size_t colon{0};
	size_t limit{source.size()};

	char ic{source[index]};

	do
	{
		switch (ic)
		{
			case '\0':
				m_error = true;
				break;
			case ':':
				colon = index;
				break;
			default:
				ic = source[++index];
		}
	} while (!m_error && !colon);
	if (colon)
	{
		m_proto = KString(source, hint, colon - hint);
		if (m_proto == "mailto")
		{
			hint = colon + 1;
			m_mailto = true;
		}
		else
		{
			if (colon >= (limit - 2))
			{
				m_error = true;
			}
			if (source[colon + 1] == '/' && source[colon + 2] == '/')
			{
				hint = colon + 3;
			}
			else
			{
				m_error = true;
			}
		}
	}
	return !m_error;
}

//..............................................................................
bool dekaf2::KProto::serialize (KString& target) const
//..............................................................................
{
	if (!m_error && m_proto.size())
	{
		target += m_proto;
		target += ':';
		if (!m_mailto)
		{
			target += "//";
		}
	}
	return !m_error;
}

//..............................................................................
bool dekaf2::KProto::getProtocol (string_view& target) const
//..............................................................................
{
	//unimplemented("dekaf2::KProto::getProtocol", __FILE__, __LINE__);
	return !m_error;
}

//..............................................................................
void dekaf2::KProto::clear ()
//..............................................................................
{
	m_error = false;
	m_mailto = false;
	m_proto = "";
}


//||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||
// KUserInfo
//||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||

//..............................................................................
dekaf2::KUserInfo::KUserInfo ()
    : m_error{false}, m_user{""}, m_pass{""}
//..............................................................................
{
}

//..............................................................................
dekaf2::KUserInfo::KUserInfo (const KString& source)
    : m_error{false}, m_user{""}, m_pass{""}
//..............................................................................
{
	// Inefficient CTOR compared with the one having hint as a formal arg.
	size_t hint{0};
	parse(source, hint);
}

//..............................................................................
/// @brief RFC3986 3.2: authority   = [ userinfo "@" ] host [ ":" port ]
/// Implementation: We take all characters until '@'.
dekaf2::KUserInfo::KUserInfo (const KString& source, size_t& hint)
    : m_error{false}, m_user{""}, m_pass{""}
//..............................................................................
{
	parse(source, hint);
}

//..............................................................................
bool dekaf2::KUserInfo::parse(const KString& source, size_t& hint)
//..............................................................................
{
	clear();

	size_t index{hint};
	size_t limit{source.size()};
	size_t colon{0};
	size_t atsign{0};
	bool none{false};

	char ic{source[index]};
	while (index < limit && !none && !atsign)
	{
		switch (ic)
		{
			case '@':
				atsign = index;
				break;
			case ':':
				colon = index;
				break;
			case '\0':
			case '/':
			case '?':
			case '#':
				none = true;
				break;
			default:
				break;
		}
		if (atsign)
		{
			if (colon)
			{
				m_user = KString (source, hint, colon - hint);
				m_pass = KString (source, colon+1, atsign - colon - 1);
			}
			else
			{
				m_user = KString (source, hint, atsign - hint);
			}
			hint = atsign + 1;
			break;
		}
		ic = source[++index];
	}
	return !m_error;
}


//..............................................................................
bool dekaf2::KUserInfo::serialize (KString& target) const
//..............................................................................
{
	if (m_user.size())
	{
		target += m_user;
		if (m_pass.size())
		{
			target += ":";
			target += m_pass;
		}
		target += "@";
	}
	return !m_error;
}

//..............................................................................
void dekaf2::KUserInfo::clear ()
//..............................................................................
{
	m_error = false;
	m_user = "";
	m_pass = "";
}

//||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||
// KDomain
//||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||

//..............................................................................
dekaf2::KDomain::KDomain ()
    : m_error{false}
    , m_iPortNum{0}
    , m_sHostName{""}
    , m_sDomainName{""}
    , m_sBaseDomain{""}
//..............................................................................
{
}

//..............................................................................
dekaf2::KDomain::KDomain (const KString& source)
    : m_error{false}
    , m_iPortNum{0}
    , m_sHostName{""}
    , m_sDomainName{""}
    , m_sBaseDomain{""}
//..............................................................................
{
	// Inefficient CTOR compared with the one having hint as a formal arg.
	size_t hint{0};
	parse(source, hint);
}

//..............................................................................
/// @brief RFC3986 3.2: authority = [ userinfo "@" ] host [ ":" port ]
/// @brief RFC3986 3.2.2: host = IP-literal / IPv4address / reg-name
/// @brief RFC3986 3.2.3: port = *DIGIT
/// Implementation: We take domain from '@'+1 to first of "/:?#\0".
/// Implementation: We take port from ':'+1 to first of "/?#\0".
dekaf2::KDomain::KDomain (const KString& source, size_t& hint)
    : m_error{false}
    , m_iPortNum{0}
    , m_sHostName{""}
    , m_sDomainName{""}
    , m_sBaseDomain{""}
//..............................................................................
{
	parse(source, hint);
}


//..............................................................................
bool dekaf2::KDomain::serialize (KString& target) const
//..............................................................................
{
	bool some = false;
	if (m_sHostName.size())
	{
		some = true;
		target += m_sHostName;
		if (m_sPortName.size())
		{
			target += ':';
			target += m_sPortName;
		}
	}
	return some;
}


//..............................................................................
bool dekaf2::KDomain::parse (const KString& source, size_t& hint)
//..............................................................................
{
	clear();

	size_t index{hint};
	size_t limit{source.size()};
	bool terminate{false};

	char ic{source[index]};
	vector <size_t> dots;

	switch (ic)
	{
		case '/':
		case ':':
		case '?':
		case '#':
		case '\0':
			terminate = true;
			m_error = true;
			return !m_error;
	}

	while (ic && index < limit && !terminate)
	{
		switch (ic)
		{
			case '/':
			case ':':
			case '?':
			case '#':
			case '\0':
				terminate = true;
				break;
			case '.':
				dots.push_back(index);
				ic = source[++index];
				break;
			default:
				ic = source[++index];
				break;
		}
		if (terminate || !ic)
		{
			size_t co = 0;
			m_sHostName = KString(source, hint, index - hint);
			switch (dots.size())
			{
				case 0:
					m_error = true;
					clear();
					break;
				case 1:
					m_sBaseDomain = KString(source, hint, dots[0] - hint).MakeUpper();
					break;
				default:
					co = dots.size() - 1;
					if (3 == (dots[co] - dots[co-1]))
					{
						if (source[dots[co]+1] == 'c' && source[dots[co]+2] == 'o')
						{
							--co;
						}
					}
					m_sBaseDomain = KString(source, dots[co-1], dots[co]).MakeUpper();
					break;

			}
		}
	}
	if (m_error)
	{
		clear();
		return !m_error;
	}
	if (0 == dots.size())
	{
		m_error = true;
		clear();
		return !m_error;
	}
	if (source[index] != ':')
	{
		hint = index;
		return !m_error;
	}

	terminate = false;
	size_t iPortIndex = ++index;
	size_t digits = 0;
	ic = source[index];
	while (ic && index < limit && !terminate)
	{
		switch (ic)
		{
			case '/':
			case '?':
			case '#':
			case '\0':
				terminate = true;
				break;
			case '0':
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
				++digits;
				ic = source[++index];
				break;
			default:
				m_error = true;
				clear();
				return !m_error;
				break;
		}
	}
	if (0 == digits)
	{
		clear();
		m_error = true;
		return !m_error;
	}
	m_sPortName = KString(source, iPortIndex, index);
	hint = index;
	return !m_error;
}

//..............................................................................
void dekaf2::KDomain::clear ()
//..............................................................................
{
	m_error = false;
	m_iPortNum = 0;
	m_sHostName = "";
	m_sDomainName = "";
	m_sBaseDomain = "";
}


//||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||
// KURIPath
//||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||

//..............................................................................
dekaf2::KURIPath::KURIPath ()
//..............................................................................
{
}

//..............................................................................
dekaf2::KURIPath::KURIPath (const KString& source)
//..............................................................................
{
	size_t hint{0};
	parse(source, hint);
}

//..............................................................................
/// @brief RFC3986 3.3: (See RFC)
/// Implementation: All characters after domain from '/' to 1st of "?#\0".
dekaf2::KURIPath::KURIPath (const KString& source, size_t& hint)
//..............................................................................
{
	parse(source, hint);
}

//..............................................................................
bool dekaf2::KURIPath::parse (const KString& source, size_t& hint)
//..............................................................................
{
	clear ();

	size_t index{hint};
	bool terminal{false};

	if (source[hint] != '/')
	{
		return true;
	}
	char ic = source[++index];
	size_t start = index;

	while (ic && !terminal)
	{
		switch (ic)
		{
			case '?': case '#': case '\0':
				terminal = true;
				break;
			default:
				ic = source[++index];
				break;
		}
	}
	m_path = KString (source, start, index - start);
	hint = index;
	return true;
}

//..............................................................................
void dekaf2::KURIPath::clear ()
//..............................................................................
{
	m_error = false;
	m_path = "";
}

//..............................................................................
bool dekaf2::KURIPath::serialize (KString& target) const
//..............................................................................
{
	if (m_path.size())
	{
		target += '/';
		target += m_path;
	}
	return true;
}


//||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||
// KQueryParms
//||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||

//..............................................................................
dekaf2::KQueryParms::KQueryParms ()
    : m_error{false}, m_query{""}
//..............................................................................
{
}

//..............................................................................
dekaf2::KQueryParms::KQueryParms (const KString& source)
    : m_error{false}, m_query{""}
//..............................................................................
{
	size_t hint{0};
	parse(source, hint);
}

//..............................................................................
/// @brief RFC3986 3.3: (See RFC)
/// Implementation: All characters after domain from '/' to 1st of "?#\0".
dekaf2::KQueryParms::KQueryParms (const KString& source, size_t& hint)
    : m_error{false}, m_query{""}
//..............................................................................
{
	parse(source, hint);
}

//..............................................................................
bool dekaf2::KQueryParms::parse (const KString& source, size_t& hint)
//..............................................................................
{
	clear ();

	size_t index{hint};
	bool terminal{false};

	if (source[hint] != '?')
	{
		return true;
	}
	char ic = source[++index];
	size_t start = index;

	while (ic && !terminal)
	{
		switch (ic)
		{
			case '#': case '\0':
				terminal = true;
				break;
			default:
				ic = source[++index];
				break;
		}
	}
	m_query = KString (source, start, index - start);
	hint = index;
	return true;
}

//..............................................................................
void dekaf2::KQueryParms::clear ()
//..............................................................................
{
	m_error = false;
	m_query = "";
}

//..............................................................................
bool dekaf2::KQueryParms::serialize (KString& target) const
//..............................................................................
{
	if (m_query.size())
	{
		target += '?';
		target += m_query;
	}
	return true;
}


//||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||
// KFragment
//||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||

//..............................................................................
dekaf2::KFragment::KFragment ()
    : m_error{false}, m_fragment{""}
//..............................................................................
{
}

//..............................................................................
dekaf2::KFragment::KFragment (const KString& source)
    : m_error{false}, m_fragment{""}
//..............................................................................
{
	size_t hint{0};
	parse(source, hint);
}

//..............................................................................
/// @brief RFC3986 3.3: (See RFC)
/// Implementation: All characters after domain from '/' to 1st of "?#\0".
dekaf2::KFragment::KFragment (const KString& source, size_t& hint)
    : m_error{false}, m_fragment{""}
//..............................................................................
{
	parse(source, hint);
}

//..............................................................................
bool dekaf2::KFragment::parse (const KString& source, size_t& hint)
//..............................................................................
{
	clear ();

	size_t index{hint};
	bool terminal{false};

	if (source[hint] != '#')
	{
		return true;
	}
	char ic = source[++index];
	size_t start = index;
	while (ic && !terminal)
	{
		switch (ic)
		{
			case '\0':
				terminal = false;
				break;
			default:
				ic = source[++index];
				break;
		}
	}
	m_fragment = KString (source, start, index - start);
	hint = index;
	return true;
}

//..............................................................................
void dekaf2::KFragment::clear ()
//..............................................................................
{
	m_error = false;
	m_fragment = "";
}

//..............................................................................
bool dekaf2::KFragment::serialize (KString& target) const
//..............................................................................
{
	if (m_fragment.size())
	{
		target += '#';
		target += m_fragment;
	}
	return true;
}



//||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||
// KURI
//||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||

//..............................................................................
dekaf2::KURI::KURI()
    : KURIPath(), KQueryParms()
//..............................................................................
{
}

//..............................................................................
dekaf2::KURI::KURI(const KString& source)
    : KURIPath(), KQueryParms()
//..............................................................................
{
	size_t hint{0};
	parse(source, hint);
}

//..............................................................................
/// @brief RFC3986 3.3: (See RFC)
/// Implementation: All characters after domain from '/' to 1st of "?#\0".
dekaf2::KURI::KURI(const KString& source, size_t& hint)
    : KURIPath(), KQueryParms()
//..............................................................................
{
	parse(source, hint);
}

//..............................................................................
bool dekaf2::KURI::parse (const KString& source, size_t& hint)
//..............................................................................
{
	clear ();

	size_t index{hint};
	bool result{true};

	result = KURIPath::parse(source, index);
	result = result && KQueryParms::parse(source, index);

	if (result)
	{
		hint = index;
	}
	else
	{
		clear();
	}
	return result;
}

//..............................................................................
void dekaf2::KURI::clear ()
//..............................................................................
{
	KURIPath	::clear();
	KQueryParms	::clear();
}

//..............................................................................
bool dekaf2::KURI::serialize (KString& target) const
//..............................................................................
{
	bool result;
	result = KURIPath::serialize(target);
	result = result && KQueryParms::serialize(target);
	return result;
}


//||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||
// KURL
//||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||

//..............................................................................
dekaf2::KURL::KURL ()
    : KProto(), KUserInfo(), KDomain(), KURI(), KFragment()
//..............................................................................
{
}

//..............................................................................
dekaf2::KURL::KURL (const KString& source)
    : KProto(), KUserInfo(), KDomain(), KURI(), KFragment()
//..............................................................................
{
	size_t hint{0};
	parse(source, hint);
}

//..............................................................................
/// @brief RFC3986 3.3: (See RFC)
/// Implementation: All characters after domain from '/' to 1st of "?#\0".
dekaf2::KURL::KURL (const KString& source, size_t& hint)
    : KProto(), KUserInfo(), KDomain(), KURI(), KFragment()
//..............................................................................
{
	parse(source, hint);
}

//..............................................................................
bool dekaf2::KURL::parse (const KString& source, size_t& hint)
//..............................................................................
{
	size_t index{hint};
	bool result = true;

	result &= KProto	::parse (source, index);
	result &= KUserInfo	::parse (source, index);
	result &= KDomain	::parse (source, index);
	result &= KURI		::parse (source, index);
	result &= KFragment	::parse (source, index);

	m_error = !result;

	if (m_error)
	{
		clear();
	}
	else
	{
		hint = index;
	}

	return !m_error;
}

//..............................................................................
bool dekaf2::KURL::serialize (KString& target) const
//..............................................................................
{
	bool result = true;

	result &= KProto	::serialize(target);
	result &= KUserInfo	::serialize(target);
	result &= KDomain	::serialize(target);
	result &= KURI		::serialize(target);
	result &= KFragment	::serialize(target);

	return result;
}

//..............................................................................
void dekaf2::KURL::clear ()
//..............................................................................
{
	KProto		::clear();
	KUserInfo	::clear();
	KDomain		::clear();
	KURI		::clear();
	KFragment	::clear();
}
