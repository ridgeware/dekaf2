/*
 //
 // DEKAF(tm): Lighter, Faster, Smarter (tm)
 //
 // Copyright (c) 2026, Ridgeware, Inc.
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

#pragma once

/// @file kimmediateinputfilter.h
/// an input filter for boost::iostreams that hands on the output of another input
/// filter before it waits for more input, and a gzip decompressor to use with it

// All headers of boost::iostreams come through kiostreams_filters.h, and through
// nothing else: it switches off the diagnostics that gcc raises falsely for them,
// and it picks the lzma implementation.
#include "kiostreams_filters.h"
#include <ios>
#include <string>
#include <utility>

// The filter of this header gets instantiated with the decompressors of
// boost::iostreams - it needs the same diagnostics switched off as the filters in
// kiostreams_filters.h.
#if defined(DEKAF2_USE_PRECOMPILED_HEADERS) && DEKAF2_IS_GCC
#pragma GCC diagnostic push
#ifdef DEKAF2_HAS_WARN_ARRAY_BOUNDS
#pragma GCC diagnostic ignored "-Warray-bounds"
#endif
#ifdef DEKAF2_HAS_WARN_STRINGOP_OVERFLOW
#pragma GCC diagnostic ignored "-Wstringop-overflow"
#endif
#ifdef DEKAF2_HAS_WARN_STRINGOP_OVERREAD
#pragma GCC diagnostic ignored "-Wstringop-overread"
#endif
#endif

// note we use the literal dekaf2 namespace here and not
// the define from kdefinitions.h
namespace dekaf2 { namespace iostreams {

//-----------------------------------------------------------------------------
/// Reads from the next stage of a filter chain what it has, and waits only if it
/// has nothing. boost::iostreams::read() waits until the whole request is filled,
/// or until the input ends.
/// @param src the stream buffer of the next stage
/// @param s the buffer to read into
/// @param n the size of the buffer
/// @return the count of bytes read, or -1 at the end of the input
template<typename Source>
std::streamsize read_some(Source& src, char* s, std::streamsize n)
//-----------------------------------------------------------------------------
{
	typedef std::char_traits<char> traits_type;

	if (n <= 0)
	{
		return 0;
	}

	if (src.in_avail() <= 0 && traits_type::eq_int_type(src.sgetc(), traits_type::eof()))
	{
		return -1;
	}

	auto iAvailable = src.in_avail();

	return src.sgetn(s, iAvailable < n ? iAvailable : n);

} // read_some

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Input filter around another input filter of boost::iostreams, typically a
/// decompressor. It hands on what the filter made of the input that has arrived,
/// and does not wait for more input first.
///
/// A boost::iostreams::symmetric_filter, which all decompressors are, reads until
/// the request of its reader is filled, and it reads its own input in pieces of
/// the size of its buffer. On a connection that stays open both wait for input
/// that the other side sends only later, while output is ready: a line of a stream
/// reaches its reader only when the stream ends.
///
/// The wrapped filter gets a source that hands on only what the next stage of the
/// chain has buffered, and else reports that it would have to wait, which makes
/// the filter return its output. Only if the filter has no output without waiting
/// the source waits for input, once. The data that passes is the same as without
/// this filter.
///
/// The wrapped filter has to take that report at every read. The gzip decompressor
/// of boost::iostreams does not, see gzip_members_decompressor.
template<typename Filter>
class immediate_input_filter
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	typedef char char_type;

	struct category
	: boost::iostreams::multichar_input_filter_tag
	, boost::iostreams::closable_tag
	{
	};

	//-----------------------------------------------------------------------------
	/// @param filter the filter to wrap
	explicit immediate_input_filter(Filter filter = Filter())
	//-----------------------------------------------------------------------------
	: m_Filter(std::move(filter))
	{
	}

	//-----------------------------------------------------------------------------
	template<typename Source>
	std::streamsize read(Source& src, char* s, std::streamsize n)
	//-----------------------------------------------------------------------------
	{
		// First without waiting: the filter may hold input of an earlier call, and the
		// next stage may have input buffered.
		feeder<Source> Feeder(src);

		for (;;)
		{
			auto iRead = m_Filter.read(Feeder, s, n);

			if (iRead != 0 || Feeder.may_wait())
			{
				// output, the end of the input, or a filter that did not ask for the
				// input it was allowed to wait for
				return iRead;
			}

			// no output without waiting: wait for the next piece of the input
			Feeder.allow_wait();
		}

	} // read

	//-----------------------------------------------------------------------------
	template<typename Source>
	void close(Source& src)
	//-----------------------------------------------------------------------------
	{
		boost::iostreams::close(m_Filter, src, std::ios_base::in);

	} // close

//------
private:
//------

	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	/// the source that the wrapped filter reads from
	template<typename Source>
	class feeder
	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{

	//------
	public:
	//------

		typedef char char_type;
		typedef boost::iostreams::source_tag category;

		//-----------------------------------------------------------------------------
		explicit feeder(Source& src)
		//-----------------------------------------------------------------------------
		: m_Source(src)
		{
		}

		//-----------------------------------------------------------------------------
		std::streamsize read(char* s, std::streamsize n)
		//-----------------------------------------------------------------------------
		{
			if (!m_bMayWait && m_Source.in_avail() <= 0)
			{
				// input would have to be waited for: 0 tells the filter so, and it
				// returns its output
				return 0;
			}

			auto iRead = read_some(m_Source, s, n);

			if (iRead > 0)
			{
				// the filter has input now, it shall work with that
				m_bMayWait = false;
			}

			return iRead;

		} // read

		//-----------------------------------------------------------------------------
		/// @return true if the next read may wait for input
		bool may_wait() const
		//-----------------------------------------------------------------------------
		{
			return m_bMayWait;
		}

		//-----------------------------------------------------------------------------
		/// let the next read wait for input
		void allow_wait()
		//-----------------------------------------------------------------------------
		{
			m_bMayWait = true;
		}

	//------
	private:
	//------

		Source& m_Source;
		bool    m_bMayWait { false };

	}; // feeder

	Filter m_Filter;

}; // immediate_input_filter

namespace detail {

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// the zlib decompressor of boost::iostreams for the gzip format (with 16 added to
/// zlib_params::window_bits), which goes on with the next member when one ends
class gzip_members_decompressor_impl : public boost::iostreams::detail::zlib_decompressor_impl<>
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	typedef boost::iostreams::detail::zlib_decompressor_impl<> base_type;

	//-----------------------------------------------------------------------------
	gzip_members_decompressor_impl(const boost::iostreams::zlib_params& Params)
	//-----------------------------------------------------------------------------
	: base_type(Params)
	{
	}

	//-----------------------------------------------------------------------------
	/// @return false at the end of the last member
	bool filter(const char*& src_begin, const char* src_end, char*& dest_begin, char* dest_end, bool flush)
	//-----------------------------------------------------------------------------
	{
		// The symmetric_filter fills its input buffer anew after every call, unless the
		// output is full: this may only return with input left if the output is full.
		for (;;)
		{
			if (m_bMemberEnded)
			{
				if (src_begin == src_end)
				{
					// At the end of the input that was the last member. Else further
					// input tells whether another member follows.
					return !flush;
				}

				if (dest_begin == dest_end)
				{
					// the next member has to wait for room in the output
					return true;
				}

				// another member follows
				base_type::close();
				m_bMemberEnded = false;
			}

			if (base_type::filter(src_begin, src_end, dest_begin, dest_end, flush))
			{
				return true;
			}

			m_bMemberEnded = true;
		}

	} // filter

	//-----------------------------------------------------------------------------
	void close()
	//-----------------------------------------------------------------------------
	{
		m_bMemberEnded = false;
		base_type::close();

	} // close

//------
private:
//------

	bool m_bMemberEnded { false };

}; // gzip_members_decompressor_impl

} // end of namespace detail

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Decompressor for the gzip format, with any count of members, as an input filter
/// for boost::iostreams. zlib reads the format, checks the checksum and the size of
/// every member.
///
/// Different to boost::iostreams::gzip_decompressor it can be wrapped into an
/// immediate_input_filter: that one reads the header and the footer of a member
/// byte by byte, and fails at the end of a member if these bytes did not arrive in
/// one piece with the end of the compressed data.
struct gzip_members_decompressor
: boost::iostreams::symmetric_filter<detail::gzip_members_decompressor_impl>
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
private:
//------

	typedef boost::iostreams::symmetric_filter<detail::gzip_members_decompressor_impl> base_type;

	//-----------------------------------------------------------------------------
	static boost::iostreams::zlib_params gzip_params()
	//-----------------------------------------------------------------------------
	{
		boost::iostreams::zlib_params Params;
		// with 16 added zlib reads the gzip format
		Params.window_bits += 16;

		return Params;
	}

//------
public:
//------

	typedef base_type::char_type char_type;
	typedef base_type::category  category;

	//-----------------------------------------------------------------------------
	explicit gzip_members_decompressor(std::streamsize iBufferSize = boost::iostreams::default_device_buffer_size)
	//-----------------------------------------------------------------------------
	: base_type(iBufferSize, gzip_params())
	{
	}

}; // gzip_members_decompressor

} } // end of namespace dekaf2::iostreams

#if defined(DEKAF2_USE_PRECOMPILED_HEADERS) && DEKAF2_IS_GCC
#pragma GCC diagnostic pop
#endif
