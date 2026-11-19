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

/// @file kflushablefilter.h
/// compressing output filters for boost::iostreams that can be flushed

// All headers of boost::iostreams come through kiostreams_filters.h, and through
// nothing else: it switches off the diagnostics that gcc raises falsely for them,
// and it picks the lzma implementation.
#include "kiostreams_filters.h"
#include <memory>
#include <vector>

// The filter of this header handles its buffer like the boost::iostreams::symmetric_filter,
// and gets instantiated with the compressors of boost::iostreams - it needs the same
// diagnostics switched off as the filters in kiostreams_filters.h.
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

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Output filter around the compressor of a boost::iostreams::symmetric_filter.
/// Different to the symmetric_filter it models Flushable: a flush of the filter
/// chain makes the compressor write all the data it got so far, and the
/// compressed stream remains open.
///
/// The Compressor has, additionally to filter() and close(), a type params_type
/// and a method bool sync(char*& dest_begin, char* dest_end) that writes pending
/// output and returns true if it has to be called again.
///
/// There is no variant for bzip2: libbz2 does not fill up the last byte of a block
/// at a flush, the receiver can read the block only after the next one was sent.
template<typename Compressor>
class flushable_compressor
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	typedef char char_type;

	struct category
	: boost::iostreams::multichar_output_filter_tag
	, boost::iostreams::closable_tag
	, boost::iostreams::flushable_tag
	{
	};

	//-----------------------------------------------------------------------------
	explicit flushable_compressor(const typename Compressor::params_type& Params = typename Compressor::params_type(),
	                              std::streamsize iBufferSize = boost::iostreams::default_device_buffer_size)
	//-----------------------------------------------------------------------------
	: m_State(std::make_shared<State>(Params, iBufferSize))
	{
	}

	//-----------------------------------------------------------------------------
	/// Switches on or off that a flush of the filter chain reaches the compressor.
	/// It is off by default: a filter chain also flushes right before it gets
	/// closed, and a compressor that is flushed there writes a few bytes more.
	void enable_flush(bool bYesNo)
	//-----------------------------------------------------------------------------
	{
		m_State->bFlush = bYesNo;
	}

	//-----------------------------------------------------------------------------
	template<typename Sink>
	std::streamsize write(Sink& snk, const char* s, std::streamsize n)
	//-----------------------------------------------------------------------------
	{
		const char* next = s;
		const char* end  = s + n;

		while (next != end)
		{
			if (IsFull() && !WriteBuffer(snk))
			{
				break;
			}

			char* dest = Dest();
			bool bMore = m_State->Codec.filter(next, end, dest, DestEnd(), false);
			SetDest(dest);

			if (!bMore)
			{
				WriteBuffer(snk);
				break;
			}
		}

		return static_cast<std::streamsize>(next - s);
	}

	//-----------------------------------------------------------------------------
	template<typename Sink>
	bool flush(Sink& snk)
	//-----------------------------------------------------------------------------
	{
		if (!m_State->bFlush)
		{
			return true;
		}

		bool bAgain { true };

		while (bAgain)
		{
			if (!IsFull())
			{
				char* dest = Dest();
				bAgain = m_State->Codec.sync(dest, DestEnd());
				SetDest(dest);
			}

			WriteBuffer(snk);
		}

		return true;
	}

	//-----------------------------------------------------------------------------
	template<typename Sink>
	void close(Sink& snk)
	//-----------------------------------------------------------------------------
	{
		try
		{
			// repeatedly invoke filter() with no input
			char        dummy;
			const char* end = &dummy;
			bool bAgain { true };

			while (bAgain)
			{
				if (!IsFull())
				{
					char* dest = Dest();
					bAgain = m_State->Codec.filter(end, end, dest, DestEnd(), true);
					SetDest(dest);
				}

				WriteBuffer(snk);
			}
		}
		catch (...)
		{
			try
			{
				Reset();
			}
			catch (...)
			{
			}

			throw;
		}

		Reset();
	}

//------
private:
//------

	//-----------------------------------------------------------------------------
	char* Dest()
	//-----------------------------------------------------------------------------
	{
		return m_State->Buffer.data() + m_State->iFill;
	}

	//-----------------------------------------------------------------------------
	char* DestEnd()
	//-----------------------------------------------------------------------------
	{
		return m_State->Buffer.data() + m_State->Buffer.size();
	}

	//-----------------------------------------------------------------------------
	void SetDest(char* dest)
	//-----------------------------------------------------------------------------
	{
		m_State->iFill = static_cast<std::size_t>(dest - m_State->Buffer.data());
	}

	//-----------------------------------------------------------------------------
	bool IsFull() const
	//-----------------------------------------------------------------------------
	{
		return m_State->iFill == m_State->Buffer.size();
	}

	//-----------------------------------------------------------------------------
	void Reset()
	//-----------------------------------------------------------------------------
	{
		m_State->iFill = 0;
		m_State->Codec.close();
	}

	//-----------------------------------------------------------------------------
	// Writes the content of the buffer into the sink.
	// @return true if at least one character was written
	template<typename Sink>
	bool WriteBuffer(Sink& snk)
	//-----------------------------------------------------------------------------
	{
		auto& Buffer = m_State->Buffer;
		auto  iFill  = static_cast<std::streamsize>(m_State->iFill);
		auto  iWrote = boost::iostreams::write(snk, Buffer.data(), iFill);

		if (iWrote > 0 && iWrote < iFill)
		{
			std::char_traits<char>::move(Buffer.data(), Buffer.data() + iWrote, static_cast<std::size_t>(iFill - iWrote));
		}

		if (iWrote > 0)
		{
			m_State->iFill = static_cast<std::size_t>(iFill - iWrote);
		}

		return iWrote != 0;
	}

	struct State
	{
		State(const typename Compressor::params_type& Params, std::streamsize iBufferSize)
		: Codec(Params)
		, Buffer(static_cast<std::size_t>(iBufferSize))
		{
		}

		Compressor        Codec;
		std::vector<char> Buffer;
		std::size_t       iFill  { 0 };
		bool              bFlush { false };
	};

	// boost::iostreams copies its filters, the copies share the state
	std::shared_ptr<State> m_State;

}; // flushable_compressor

namespace detail {

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// the zlib compressor of boost::iostreams with a sync flush - it writes the
/// zlib format, or the gzip format with 16 added to zlib_params::window_bits
class flushable_zlib_compressor_impl : public boost::iostreams::detail::zlib_compressor_impl<>
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	typedef boost::iostreams::zlib_params params_type;

	flushable_zlib_compressor_impl(const params_type& Params)
	: boost::iostreams::detail::zlib_compressor_impl<>(Params)
	{
	}

	//-----------------------------------------------------------------------------
	bool sync(char*& dest_begin, char* dest_end)
	//-----------------------------------------------------------------------------
	{
		char        dummy;
		const char* src = &dummy;

		before(src, src, dest_begin, dest_end);
		int result = xdeflate(boost::iostreams::zlib::sync_flush);
		after(src, dest_begin, true);

		if (result == boost::iostreams::zlib::buf_error)
		{
			// zlib had nothing to flush
			return false;
		}

		boost::iostreams::zlib_error::check BOOST_PREVENT_MACRO_SUBSTITUTION(result);

		// zlib has more if it filled the output
		return dest_begin == dest_end;
	}

}; // flushable_zlib_compressor_impl

#ifdef DEKAF2_HAS_LIBLZMA

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// the lzma compressor (xz format) with a flush
class flushable_lzma_compressor_impl : public lzmaiostreams::detail::lzma_compressor_impl<>
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	typedef lzmaiostreams::lzma_params params_type;

	flushable_lzma_compressor_impl(const params_type& Params)
	: lzmaiostreams::detail::lzma_compressor_impl<>(Params)
	{
	}

	//-----------------------------------------------------------------------------
	bool sync(char*& dest_begin, char* dest_end)
	//-----------------------------------------------------------------------------
	{
		char        dummy;
		const char* src = &dummy;

		before(src, src, dest_begin, dest_end);
		// a full flush, as the multithreaded encoder has no sync flush
		int result = deflate(lzmaiostreams::lzma::full_flush);
		after(src, dest_begin, true);

		if (result == lzmaiostreams::lzma::buf_error)
		{
			// liblzma had nothing to flush
			return false;
		}

		lzmaiostreams::lzma_error::check BOOST_PREVENT_MACRO_SUBSTITUTION(result);

		// liblzma returns stream_end when the flush is complete
		return result != lzmaiostreams::lzma::stream_end;
	}

}; // flushable_lzma_compressor_impl

#endif // DEKAF2_HAS_LIBLZMA

#ifdef DEKAF2_HAS_LIBZSTD

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// the zstd compressor with a flush
class flushable_zstd_compressor_impl : public zstd_compressor_impl<>
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	typedef zstd_params params_type;

	flushable_zstd_compressor_impl(const params_type& Params)
	: zstd_compressor_impl<>(Params)
	{
	}

	//-----------------------------------------------------------------------------
	bool sync(char*& dest_begin, char* dest_end)
	//-----------------------------------------------------------------------------
	{
		char        dummy;
		const char* src = &dummy;

		before(src, src, dest_begin, dest_end);
		int result = deflate(zstd::flush);
		after(src, dest_begin, true);

		return result != zstd::stream_end;
	}

}; // flushable_zstd_compressor_impl

#endif // DEKAF2_HAS_LIBZSTD

#ifdef DEKAF2_HAS_LIBBROTLI

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// the brotli compressor with a flush
class flushable_brotli_compressor_impl : public brotli_compressor_impl<>
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	typedef brotli_params params_type;

	flushable_brotli_compressor_impl(const params_type& Params)
	: brotli_compressor_impl<>(Params)
	{
	}

	//-----------------------------------------------------------------------------
	bool sync(char*& dest_begin, char* dest_end)
	//-----------------------------------------------------------------------------
	{
		char        dummy;
		const char* src = &dummy;

		before(src, src, dest_begin, dest_end);
		deflate(brotli::flush);
		after(src, dest_begin, true);

		// brotli has more if it filled the output
		return dest_begin == dest_end;
	}

}; // flushable_brotli_compressor_impl

#endif // DEKAF2_HAS_LIBBROTLI

} // end of namespace detail

typedef flushable_compressor<detail::flushable_zlib_compressor_impl>   flushable_zlib_compressor;

#ifdef DEKAF2_HAS_LIBLZMA
typedef flushable_compressor<detail::flushable_lzma_compressor_impl>   flushable_lzma_compressor;
#endif

#ifdef DEKAF2_HAS_LIBZSTD
typedef flushable_compressor<detail::flushable_zstd_compressor_impl>   flushable_zstd_compressor;
#endif

#ifdef DEKAF2_HAS_LIBBROTLI
typedef flushable_compressor<detail::flushable_brotli_compressor_impl> flushable_brotli_compressor;
#endif

} } // end of namespace dekaf2::iostreams

#if defined(DEKAF2_USE_PRECOMPILED_HEADERS) && DEKAF2_IS_GCC
#pragma GCC diagnostic pop
#endif
