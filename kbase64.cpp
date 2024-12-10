/*
//
// DEKAF(tm): Lighter, Faster, Smarter(tm)
//
// Copyright (c) 2017, Ridgeware, Inc.
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
*/

#include "kbase64.h"
#include "klog.h"

#include <boost/archive/iterators/base64_from_binary.hpp>
#include <boost/archive/iterators/binary_from_base64.hpp>
#include <boost/archive/iterators/insert_linebreaks.hpp>
#include <boost/archive/iterators/transform_width.hpp>
#include <boost/archive/iterators/remove_whitespace.hpp>

DEKAF2_NAMESPACE_BEGIN

//-----------------------------------------------------------------------------
KString KBase64::Encode(KStringView sInput, bool bWithLinebreaks)
//-----------------------------------------------------------------------------
{
	using namespace boost::archive::iterators;
	using iterator_type = KStringView::const_iterator;
	using base64_enc_lf = insert_linebreaks<base64_from_binary<transform_width<iterator_type, 6, 8> >, 76>;
	using base64_enc    = base64_from_binary<transform_width<iterator_type, 6, 8> >;

	KString out;

	// calculate final size for encoded string
	KString::size_type iSize = sInput.size() * 8 / 6 + sInput.size() % 3;
	// add linefeeds
	iSize += iSize / 76;
	// and reserve buffer to avoid reallocations
	out.reserve(iSize);

	// transform to base64
	if (bWithLinebreaks)
	{
		out.assign(base64_enc_lf(sInput.begin()), base64_enc_lf(sInput.end()));
	}
	else
	{
		out.assign(base64_enc(sInput.begin()), base64_enc(sInput.end()));
	}

	// append the padding
	out.append((3 - sInput.size() % 3) % 3, '=');

	return out;

} // Encode

//-----------------------------------------------------------------------------
KString KBase64::Decode(KStringView sInput)
//-----------------------------------------------------------------------------
{
	using namespace boost::archive::iterators;
	using iterator_type = KStringView::const_iterator;
	using base64_dec    = transform_width<binary_from_base64<remove_whitespace<iterator_type> >, 8, 6>;

	KString out;

	DEKAF2_TRY
	{
		// calculate approximate size for decoded string (input may contain whitespace)
		KString::size_type iSize = sInput.size() * 6 / 8;
		// and reserve buffer to avoid reallocations
		out.reserve(iSize);

		// transform from base64
		out.assign(base64_dec(sInput.begin()), base64_dec(sInput.end()));

		// remove the padding
		KStringView::size_type len = sInput.size();
		if (len > 2 && out.size() > 1)
		{
			// a padded sInput has at least 3 chars
			if (sInput[len-1] == '=')
			{
				if (sInput[len-2] == '=')
				{
					out.erase(out.size()-2, 2);
				}
				else
				{
					out.erase(out.size()-1, 1);
				}
			}
		}
	}
	DEKAF2_CATCH(const std::exception& ex)
	{
#ifdef DEKAF2_IS_WINDOWS
		const char* p = ex.what();
#endif
		kDebug(1, "invalid base64: {}..", sInput.Left(40));
		out.clear();
	}

	return out;

} // Decode


// copied from boost, adapted for URL safe character set
// https://www.boost.org/doc/libs/1_68_0/boost/archive/iterators/base64_from_binary.hpp

namespace detail {

template<class CharType>
struct from_6_bit_url {
    typedef CharType result_type;
    CharType operator()(CharType t) const{
        static const char * lookup_table =
            "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
            "abcdefghijklmnopqrstuvwxyz"
            "0123456789"
            "-_";
        BOOST_ASSERT(t < 64);
        return lookup_table[static_cast<size_t>(t)];
    }
};

template<
    class Base,
    class CharType = typename boost::iterator_value<Base>::type
>
class base64url_from_binary :
	public boost::transform_iterator<
        detail::from_6_bit_url<CharType>,
        Base
    >
{
    friend class boost::iterator_core_access;
	typedef boost::transform_iterator<
        typename detail::from_6_bit_url<CharType>,
        Base
    > super_t;

public:
    template<class T>
    base64url_from_binary(T start) :
        super_t(
            Base(static_cast< T >(start)),
            detail::from_6_bit_url<CharType>()
        )
    {}
    base64url_from_binary(const base64url_from_binary & rhs) :
        super_t(
            Base(rhs.base_reference()),
            detail::from_6_bit_url<CharType>()
        )
    {}
};

} // namespace detail (end of copy from boost)


//-----------------------------------------------------------------------------
KString KBase64Url::Encode(KStringView sInput)
//-----------------------------------------------------------------------------
{
	using namespace boost::archive::iterators;
	using iterator_type = KStringView::const_iterator;
	using base64_enc    = detail::base64url_from_binary<transform_width<iterator_type, 6, 8> >;

	KString out;

	// calculate final size for encoded string
	KString::size_type iSize = sInput.size() * 8 / 6;
	// and reserve buffer to avoid reallocations
	out.reserve(iSize);

	// transform to base64
	out.assign(base64_enc(sInput.begin()), base64_enc(sInput.end()));

	return out;

} // Encode

// copied from boost, adapted for URL safe character set (works with
// both character sets actually)
// https://www.boost.org/doc/libs/1_68_0/boost/archive/iterators/binary_from_base64.hpp
	
namespace detail {

template<class CharType>
struct to_6_bit_url {
    typedef CharType result_type;
    CharType operator()(CharType t) const{
        static const signed char lookup_table[] = {
            -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
            -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
            -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,62,-1,62,-1,63,
            52,53,54,55,56,57,58,59,60,61,-1,-1,-1, 0,-1,-1, // render '=' as 0
            -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,
            15,16,17,18,19,20,21,22,23,24,25,-1,-1,-1,-1,63,
            -1,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,
            41,42,43,44,45,46,47,48,49,50,51,-1,-1,-1,-1,-1
        };
        signed char value = -1;
        if((unsigned)t <= 127)
            value = lookup_table[(unsigned)t];
        if(-1 == value)
            boost::serialization::throw_exception(
				boost::archive::iterators::dataflow_exception(boost::archive::iterators::dataflow_exception::invalid_base64_character)
            );
        return value;
    }
};

template<
    class Base,
    class CharType = typename boost::iterator_value<Base>::type
>
class binary_from_base64url : public
	boost::transform_iterator<
        detail::to_6_bit_url<CharType>,
        Base
    >
{
    friend class boost::iterator_core_access;
	typedef boost::transform_iterator<
        detail::to_6_bit_url<CharType>,
        Base
    > super_t;
public:
    template<class T>
    binary_from_base64url(T  start) :
        super_t(
            Base(static_cast< T >(start)),
            detail::to_6_bit_url<CharType>()
        )
    {}
    binary_from_base64url(const binary_from_base64url & rhs) :
        super_t(
            Base(rhs.base_reference()),
            detail::to_6_bit_url<CharType>()
        )
    {}
};

} // namespace detail (end of copy from boost)

//-----------------------------------------------------------------------------
KString KBase64Url::Decode(KStringView sInput)
//-----------------------------------------------------------------------------
{
	using namespace boost::archive::iterators;
	using iterator_type = KStringView::const_iterator;
	using base64_dec    = transform_width<detail::binary_from_base64url<remove_whitespace<iterator_type> >, 8, 6>;

	KString out;

	DEKAF2_TRY
	{
		// calculate approximate size for decoded string (input may contain whitespace)
		KString::size_type iSize = sInput.size() * 6 / 8;
		// and reserve buffer to avoid reallocations
		out.reserve(iSize);

		// transform from base64
		out.assign(base64_dec(sInput.begin()), base64_dec(sInput.end()));

		// remove the padding if any
		KStringView::size_type len = sInput.size();
		if (len > 2 && out.size() > 1)
		{
			// a padded sInput has at least 3 chars
			if (sInput[len-1] == '=')
			{
				if (sInput[len-2] == '=')
				{
					out.erase(out.size()-2, 2);
				}
				else
				{
					out.erase(out.size()-1, 1);
				}
			}
		}
	}
	DEKAF2_CATCH(const std::exception& ex)
	{
#ifdef DEKAF2_IS_WINDOWS
		const char* p = ex.what();
#endif
		kDebug(1, "invalid base64: {}..", sInput.Left(40));
		out.clear();
	}

	return out;

} // Decode

DEKAF2_NAMESPACE_END
