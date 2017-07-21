#pragma once

///////////////////////////////////////////////////////////////////////////////
//
// DEKAF(tm): Lighter, Faster, Smarter(tm)
//
// Copyright (c) 2000-2016, Ridgeware, Inc.
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

//_____________________________________________________________________________
// GOAL: rapid disambiguating pass through URL disassembling/reassembling.
//
// scheme:[//][user.name[:pass]@]host.name[:port][/path][?query][#fragment]\0
// A     B CC      D     E     F     G     H      I      J       K         L
//
// RFC 3986 rules.  https://www.ietf.org/rfc/rfc3986.txt
//_____________________________________________________________________________
// Compile and run for test.
// $ g++ -D__KURL_MAIN__ -std=c++14 -Wall -o kurl kurl.cpp
// $ ./kurl
//_____________________________________________________________________________
// Strategy: find all delimiters and identify components to be allocated.
// Allocate unique_ptr copies of char sequences into URL element atomic units.
// Touch each character as few times as necessary: 1, 2, or 3 at max.
// Mimimize branch points and non-inline function calls.
// Use JIT (Just-In-Time) compile to fill delimiter identification table.
// Strategy: find all delimiters and identify components to be allocated.
//_____________________________________________________________________________
// The trickiest code is discriminating [user.name[:pass]@] from host.name.
// Simple LR1 linear passthrough doesn't disambiguate D from G until
// either a '@' identifies it as [user.name[:pass]@]
// or one of ":/?#\0" identifies it as host.name.
// Backtracking on the in-memory string is used to acquire host.name.
//_____________________________________________________________________________
// TODO: parse query and convert %CC codes to bytes (%26 becomes '&').
// TODO: KURL CTOR for string_view
//_____________________________________________________________________________

#include <map>
#include <memory>
#include <string>
#include <cstring>
#include <cstdlib>
#include <cstdint>
#include <cstdio>
#include <vector>

#include <fmt/format.h>

namespace hermes
{

using std::map;
using std::atoi;
using std::puts;
using std::string;
using std::strncmp;
using std::strtoul;
using std::to_string;
using std::vector;

using fmt::format;

bool complaint (const string& message, const char* file, size_t line);

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class KProto
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	//-----
    public:
	//-----
	    friend class KURL;
		KProto();

		// Identical functions with different names
		inline void getScheme (string& sScheme) const { sScheme = m_scheme; }
		inline void getProto  (string& sProto ) const { sProto  = m_scheme; }

		// Identical functions with different names
		inline void setScheme (const string& sScheme) { m_scheme = sScheme; }
		inline void setProto  (const string& sProto ) { m_scheme = sProto ; }

	//------
    private:
	//------
		string m_scheme  ; // KURL : public KProto

		// Fetch scheme
		// scheme:[//][user.name[:pass]@]host[:port][/path][?query][#fragment]\0
		// scheme:
		// Pass '/' if any after scheme
		// scheme:[//][user.name[:pass]@]host[:port][/path][?query][#fragment]\0
		//       :[//]
		/// @fn parseScheme
		/// @brief fill first arg with legal scheme or leave it empty
		static inline bool parseScheme (
		    string& m_scheme,
		    const string& URL,
		    const vector<size_t>& cursors,
		    size_t& index)
		{
			size_t colon = index;
			size_t delimiters = cursors.size();
			m_scheme.clear();
			if (delimiters < 2)
			{
				return complaint ("missing delimiters", __FILE__, __LINE__);
			}
			if (URL[cursors[index]] != ':')
			{
				return complaint ("missing scheme", __FILE__, __LINE__);
			}
			m_scheme = URL.substr (0, cursors[index]);

			while (index < delimiters && URL[cursors[index]] == '/')
			{
				++index;
			}
			size_t slashes = index - colon - 1;
			if ((slashes == 0 && (m_scheme == "mailto")) || (slashes == 2))
			{
				return true;
			}
			return complaint("bad scheme slashes", __FILE__, __LINE__);
		}

};

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class KDomain
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	//-----
    public:
	//-----
	    friend class KURL;
		KDomain();

		inline void getUser   (string& sUser  ) const { sUser   = m_user    ; }
		inline void getPass   (string& sPass  ) const { sPass   = m_pword   ; }
		inline void getHost   (string& sHost  ) const { sHost   = m_host    ; }
		inline void getDomain (string& sDomain) const { sDomain = m_domain  ; }

		inline void setUser   (const string& sUser  ) { m_user     = sUser  ; }
		inline void setPass   (const string& sPass  ) { m_pword    = sPass  ; }
		inline void setHost   (const string& sHost  ) { m_host     = sHost  ; }
		inline void setDomain (const string& sDomain) { m_domain   = sDomain; }

	//------
    private:
	//------
		string m_user;
		string m_pword;
		string m_host;         // www.acme.com
		string m_domain;       //     ACME
};

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class KPort
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	//-----
    public:
	//-----
	    friend class KURL;
		KPort ();

		inline uint16_t getPort () const {
			if (m_port.size())
			{
				const char* start = m_port.c_str();
				char* end;
				return static_cast<uint16_t>(strtoul (start, &end, 10));
			}
			return 0U;
		}

		inline void getPort (string&   sPort) const { sPort = m_port            ; }
		inline void getPort (uint16_t& iPort) const { iPort = getPort()         ; }

		inline void setPort (const string&   sPort) { m_port = sPort            ; }
		inline void setPort (const uint16_t& iPort) { m_port = to_string(iPort) ; }

	//------
    private:
	//------
		string m_port;
};


//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class KURL : public KProto, KDomain, KPort
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	//-----
    public:
	//-----
	    KURL           (const string& URL);
		~KURL          (                 );

		bool parse     (const string& URL);
		bool serialize (string& URL    ) const;
		void report    (               ) const;

	//------
    private:
	//------

		// TODO:JDL@Joe. www.acme.com TLD industry std .com or basedomain ACME
		// KDomain::getTLD(); e.g. ACME (check if Joe still wants this getter)

		//ups  m_user    ; // KDomain::getUser
		//ups  m_password; // KDomain::getPass
		//ups  m_host    ; // KDomain::getDomain(); e.g. sub.acme.com
		//ups  m_domain  ; // KDomain::getBaseDomain(); e.g. ACME
		//ups  m_port    ; // KDomain::getPort
		string  m_path    ; // KURI : public KPath
		string  m_query   ; // KURI : public KQueryParms
		string  m_frag    ; // KURI : public KFragment

		bool m_isMailto;

		map<string,string> m_KeyVal;  // Convert to KProp


		/// @fn cut
		/// @brief fill cursors with offsets to delimiter characters in URL.
		/// @param URL any string
		/// @param cursors offsets to delimiters
		/// @returns nothing
		static inline void cut (const string& URL, vector<size_t>& cursors)
		{
			cursors.clear();
			cursors.push_back (0);  // Beginning of line is delimiter
			size_t index;
			for (index = 0; index < URL.size(); ++index)
			{
				switch (URL[index])
				{
					case '#': case '.': case '/': case ':': case '?': case '@':
						cursors.push_back (index);
				}
			}
			cursors.push_back (index);  // NUL at end is also a delimiter
		}

		inline bool decode (char*& sTarget, const char*sSource)
		{
			while (*sSource)
			{
				if (*sSource == '%')
				{
					unsigned iByte = 0;
					// Violate DRY (unroll loop of 2 passes)
					switch (*++sSource)
					{
						case '0':           iByte =  0; break;
						case '1':           iByte =  1; break;
						case '2':           iByte =  2; break;
						case '3':           iByte =  3; break;
						case '4':           iByte =  4; break;
						case '5':           iByte =  5; break;
						case '6':           iByte =  6; break;
						case '7':           iByte =  7; break;
						case '8':           iByte =  8; break;
						case '9':           iByte =  9; break;
						case 'A': case 'a': iByte = 10; break;
						case 'B': case 'b': iByte = 11; break;
						case 'C': case 'c': iByte = 12; break;
						case 'D': case 'd': iByte = 13; break;
						case 'E': case 'e': iByte = 14; break;
						case 'F': case 'f': iByte = 15; break;
						default:
							puts (format("{}[{}]: illegal hex character '{}'",
							             __FILE__, __LINE__, *sSource).c_str());
							return false;
					}
					iByte <<= 4;
					switch (*++sSource)
					{
						case '0':           iByte +=  0; break;
						case '1':           iByte +=  1; break;
						case '2':           iByte +=  2; break;
						case '3':           iByte +=  3; break;
						case '4':           iByte +=  4; break;
						case '5':           iByte +=  5; break;
						case '6':           iByte +=  6; break;
						case '7':           iByte +=  7; break;
						case '8':           iByte +=  8; break;
						case '9':           iByte +=  9; break;
						case 'A': case 'a': iByte += 10; break;
						case 'B': case 'b': iByte += 11; break;
						case 'C': case 'c': iByte += 12; break;
						case 'D': case 'd': iByte += 13; break;
						case 'E': case 'e': iByte += 14; break;
						case 'F': case 'f': iByte += 15; break;
						default:
							puts (format("{}[{}]: illegal hex character '{}'",
							             __FILE__, __LINE__, *sSource).c_str());
							return false;
					}

					*sTarget++ = static_cast<char>(iByte);
					++sSource;
				}
				else
				{
					*sTarget++ = *sSource++;
				}
			}
			return true;
		}

		// Fetch host, port, and domain
		// scheme:[//][user.name[:pass]@]host[:port][/path][?query][#fragment]\0
		//                               host[:port]
		// domain for subdomain.domain.co.jp is DOMAIN.
		static inline bool parseHostPortDomain(
		    string& m_host, string& m_port, string& m_domain,
		    const string& URL,
		    const vector<size_t>& cursors,
		    size_t& index)
		{
			size_t anchor = index;
			size_t delimiters = cursors.size();
			size_t user = index - 1;
			size_t dots = 0;
			m_host.clear();
			m_port.clear();
			m_domain.clear();
			while (index < delimiters && URL[cursors[index]] == '.') {
				++index; // leave index on any of ":/?#"
				++dots;
			}
			size_t host = cursors[user] + 1;
			m_host = string(host, URL[cursors[index]]);

			// Fetch and uppercase DOMAIN
			size_t domain_start = 0;
			size_t domain_end   = 0;
			if (!dots)
			{
				index = anchor;
				return complaint("no domain sub to TLD", __FILE__, __LINE__);
			}
			domain_start = cursors[index-2];
			domain_end   = cursors[index-1];
			if (
			    URL[domain_start+0] == '.' &&
			    URL[domain_start+1] == 'c' &&
			    URL[domain_start+2] == 'o' &&
			    URL[domain_start+3] == '.'
			    )
			{
				domain_start = cursors[index-3];
				domain_end   = cursors[index-2];
			}
			++domain_start;
			m_domain = URL.substr(domain_start, domain_end);
			for (size_t ii = 0; ii < m_domain.size (); ++ii)
			{
				m_domain.at (ii) = static_cast<char>(toupper (m_domain.at (ii)));
			}

			// Fetch port if any
			// scheme:[//][user.name[:pass]@]host[:port][/path][?query][#fragment]\0
			//                                   [:port]
			if (index < delimiters && URL[cursors[index] == ':'])
			{
				size_t port_start = cursors[index++]+1;
				size_t port_end   = cursors[index  ]-1; // leave index on "/?#"
				m_port = URL.substr(port_start, port_end);
			}

			return true;
		}

		// Fetch user:pass if present (deprecated in RFC 3986)
		// scheme:[//][user.name[:pass]@]host[:port][/path][?query][#fragment]\0
		//            [user.name[:pass]@]
		//            usernames may have periods but none of  ":/@?#";
		//            passwords may have         but none of ".:/@?#";
		//            TODO should code be written to lift restrictions?

		// index is not on first delimiter in user.
		// Subtle.  Variables 'user' and 'dots' keep track of ambiguous
		// user and host fields.  A username may have dots.  So must a host.
		// We keep track of the user index as an anchor for the host if needed.
		// We count dots also, and hosts are required to have them.
		// A '@' disambiguates as user[:password].
		// Any of "/?#\0" disambiguates as host.
		// Optional [:port] after host is ambiguous with optional [:password].
		static inline bool parseUserPass(
		    string& m_user, string& m_pword,
		    const string& URL,
		    const vector<size_t>& cursors,
		    size_t& index)
		{
			size_t anchor = index;
			size_t delimiters = cursors.size();
			m_user.clear();
			m_pword.clear();
			// index past '.' if any in user
			while (index < delimiters && URL[cursors[index]] == '.')
			{
				++index;
			}

			if (URL[cursors[index]] == '@')
			{
				// No password
				m_user = string(cursors[anchor -1]+1, cursors[index]);
				m_pword = "";
				index += 1;
				// Disambiguation to user complete
				return true;
			}
			else if (URL[cursors[index]] == ':' && URL[cursors[index+1]] == '@')
			{
				// Password
				m_user  = URL.substr(cursors[anchor -1]+1, cursors[index  ]);
				m_pword = URL.substr(cursors[index   ]+1, cursors[index+1]);
				index += 2;
				// Disambiguation to user:password complete
				return true;
			}
			else
			{
				index = anchor;
			}

			return false;
		}

		// Fetch path if any
		// scheme:[//][user.name[:pass]@]host[:port][/path][?query][#fragment]\0
		//                                          [/path]
		// host cursors with '.' have already been stepped past.
		// The while loop will not see a '.' until after the first '/'.
		static inline bool parsePath(
		    string& m_path,
		    const string& URL,
		    const vector<size_t>& cursors,
		    size_t& index)
		{
			size_t anchor = index;
			size_t delimiters = cursors.size();
			m_path.clear();
			if (URL[cursors[index++]] != '/')
			{
				return false;
			}
			++index;
			while (index < delimiters &&
			       (URL[cursors[index]] == '.' || URL[cursors[index]] == '/'))
			{
				++index; // leave index on "?#"
			}
			if (index != anchor)
			{
				size_t path_start = cursors[anchor] + 1;
				size_t path_end   = cursors[index ] - 1;
				m_path = URL.substr(path_start, path_end);
				return true;
			}
			return false;
		}

		// Fetch fragment if any
		// scheme:[//][user.name[:pass]@]host[:port][/path][?query][#fragment]\0
		//                                                         [#fragment]\0
		static inline bool parseFragment(
		    string& m_frag,
		    const string& URL,
		    const vector<size_t>& cursors,
		    size_t& index)
		{
			m_frag.clear();
			if (URL[cursors[index-1]] == '#')
			{
				m_frag = URL.substr(cursors[index - 1] + 1, cursors[index]);
				--index;
				return true;
			}

			return false;
		}

		// Fetch query if any
		// scheme:[//][user.name[:pass]@]host[:port][/path][?query][#fragment]\0
		//                                                 [?query]
		// http://www.ietf.org/rfc/rfc3986.txt ENCODE/DECODE
		static inline bool parseQuery(
		    string& m_query,
		    const string& URL,
		    const vector<size_t>& cursors,
		    size_t begin, size_t end)
		{
			size_t delimiters = cursors.size();
			m_query.clear();
			if (end < delimiters && URL[cursors[begin]] == '?')
			{
				size_t query_start = cursors[begin]  + 1;
				size_t query_end   = cursors[end];
				m_query = URL.substr(query_start, query_end);
			}

			return false;
		}

		static string  s_mailto  ; // Manifest constant for reassemble decision //

}; // class KURL
}  // namespace hermes
