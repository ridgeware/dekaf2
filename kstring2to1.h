
#pragma once

#include "bits/kcppcompat.h"

#ifdef DEKAF2_ALLOW_CONVERSIONS_WITH_OLD_KSTRING

#include "kstring.h"
#include </home/onelink/src/dekaf/src/kstring.h>

namespace dekaf2 {

class KString2To1 : public KString
{
public:

	KString2To1(const ::KString& old)
	    : KString(old.c_str(), old.size())
	{
	}

	KString2To1(::KString& old)
	    : KString(old.c_str(), old.size())
	{
	}

#ifndef DEKAF2_USE_FOLLY_STRING_AS_KSTRING
	KString2To1(::KString&& old)
	    : KString(std::move(old))
	{
	}
#endif

	template<class...Args>
	KString2To1(Args&&...args)
	    : KString(std::forward<Args>(args)...)
	{
	}

	/// print arguments with fmt::printf..
	// actually we switch the printing format specifier to a sprintf - like
	// as the previous KString used such
	template<class... Args>
	KString& Format(Args&&... args)
	{
		m_rep = kPrintf(std::forward<Args>(args)...);
		return *this;
	}

#ifdef DEKAF2_ALLOW_CONVERSIONS_WITH_OLD_KSTRING_XXX
 #ifdef DEKAF2_USE_FBSTRING_AS_KSTRING
	KString2To1& operator= (const ::KString str) { m_rep = str.c_str(); return *this; }
 #else
	KString2To1& operator= (const ::KString str) { m_rep = str; return *this; }
 #endif
#endif

#ifdef DEKAF2_ALLOW_CONVERSIONS_WITH_OLD_KSTRING_XXX
	inline operator ::KString() const { return ToStdString(); }
#endif

};

} // end of namespace dekaf2

namespace std
{
	std::istream& getline(std::istream& stream, dekaf2::KString& str);
	std::istream& getline(std::istream& stream, dekaf2::KString& str, dekaf2::KString::value_type delimiter);

	/// provide a std::hash for KString
	template<> struct hash<dekaf2::KString2To1>
	{
		typedef dekaf2::KString2To1 argument_type;
		typedef std::size_t result_type;
		result_type operator()(argument_type const& s) const noexcept
		{
#ifdef DEKAF2_USE_FBSTRING_AS_KSTRING
			return std::hash<dekaf2::KString::string_type>{}(s);
#else
			return std::hash<dekaf2::KString::string_type>{}(s.ToStdString());
#endif
		}
	};

} // end of namespace std

#include <boost/functional/hash.hpp>

namespace boost
{
	/// provide a boost::hash for KString
	template<> struct hash<dekaf2::KString2To1> : public std::unary_function<dekaf2::KString2To1, std::size_t>
	{
		typedef dekaf2::KString2To1 argument_type;
		typedef std::size_t result_type;
		result_type operator()(argument_type const& s) const noexcept
		{
#ifdef DEKAF2_USE_FBSTRING_AS_KSTRING
			// reuse the std::hash, as it knows fbstring already
			return std::hash<dekaf2::KString::string_type>{}(s);
#else
			return std::hash<dekaf2::KString::string_type>{}(s.ToStdString());
#endif
		}
	};

} // end of namespace boost



#endif
