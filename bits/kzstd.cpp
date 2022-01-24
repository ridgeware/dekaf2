// adapted for multithreading by Ridgeware 2022.
// (C) Copyright Reimar Döffinger 2018.
// Based on zstd.cpp by:
// (C) Copyright Milan Svoboda 2008.
// (C) Copyright Jonathan Turkanis 2003.
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.)

// See http://www.boost.org/libs/iostreams for documentation.

// Define BOOST_IOSTREAMS_SOURCE so that <boost/iostreams/detail/config.hpp>
// knows that we are building the library (possibly exporting code), rather
// than using it (possibly importing code).
#define BOOST_IOSTREAMS_SOURCE

#include "kzstd.h"

#include <zstd.h>

#include <boost/throw_exception.hpp>
#include <boost/iostreams/detail/config/dyn_link.hpp>

namespace dekaf2 { namespace iostreams {

namespace zstd {
                    // Compression levels

const uint16_t best_speed           = 1;
const uint16_t best_compression     = 19;
const uint16_t default_compression  = 3;

                    // Status codes

const int okay                 = 0;
const int stream_end           = 1;

                    // Flush codes

const int finish               = ZSTD_e_end;
const int flush                = ZSTD_e_flush;
const int run                  = ZSTD_e_continue;
} // End namespace zstd.

//------------------Implementation of zstd_error------------------------------//

zstd_error::zstd_error(size_t error)
    : BOOST_IOSTREAMS_FAILURE(ZSTD_getErrorName(error)), error_(error)
    { }

void zstd_error::check BOOST_PREVENT_MACRO_SUBSTITUTION(size_t error)
{
    if (ZSTD_isError(error))
        boost::throw_exception(zstd_error(error));
}

//------------------Implementation of zstd_base-------------------------------//

namespace detail {

zstd_base::zstd_base()
    : cstream_(nullptr)
	, dstream_(nullptr)
	, in_(new ZSTD_inBuffer)
	, out_(new ZSTD_outBuffer)
	, eof_(0)
    { }

zstd_base::~zstd_base()
{
    ZSTD_freeCStream(static_cast<ZSTD_CStream *>(cstream_));
    ZSTD_freeDStream(static_cast<ZSTD_DStream *>(dstream_));
    delete static_cast<ZSTD_inBuffer*>(in_);
    delete static_cast<ZSTD_outBuffer*>(out_);
}

void zstd_base::before( const char*& src_begin, const char* src_end,
                        char*& dest_begin, char* dest_end )
{
    ZSTD_inBuffer *in = static_cast<ZSTD_inBuffer *>(in_);
    ZSTD_outBuffer *out = static_cast<ZSTD_outBuffer *>(out_);
    in->src = src_begin;
    in->size = static_cast<size_t>(src_end - src_begin);
    in->pos = 0;
    out->dst = dest_begin;
    out->size = static_cast<size_t>(dest_end - dest_begin);
    out->pos = 0;
}

void zstd_base::after(const char*& src_begin, char*& dest_begin, bool)
{
    ZSTD_inBuffer *in = static_cast<ZSTD_inBuffer *>(in_);
    ZSTD_outBuffer *out = static_cast<ZSTD_outBuffer *>(out_);
    src_begin = reinterpret_cast<const char*>(in->src) + in->pos;
    dest_begin = reinterpret_cast<char*>(out->dst) + out->pos;
}

int zstd_base::deflate(int action)
{
    ZSTD_CStream *s = static_cast<ZSTD_CStream *>(cstream_);
    ZSTD_inBuffer *in = static_cast<ZSTD_inBuffer *>(in_);
    ZSTD_outBuffer *out = static_cast<ZSTD_outBuffer *>(out_);
    // Ignore spurious extra calls.
    // Note size > 0 will trigger an error in this case.
    if (eof_ && in->size == 0) return zstd::stream_end;
	size_t result = ZSTD_compressStream2(s, out, in, static_cast<ZSTD_EndDirective>(action));
    zstd_error::check BOOST_PREVENT_MACRO_SUBSTITUTION(result);
    if (action != zstd::run)
    {
        eof_ = action == zstd::finish && result == 0;
        return result == 0 ? zstd::stream_end : zstd::okay;
    }
    return zstd::okay;
}

int zstd_base::inflate(int action)
{
    ZSTD_DStream *s = static_cast<ZSTD_DStream *>(dstream_);
    ZSTD_inBuffer *in = static_cast<ZSTD_inBuffer *>(in_);
    ZSTD_outBuffer *out = static_cast<ZSTD_outBuffer *>(out_);
    // need loop since iostream code cannot handle short reads
    do {
        size_t result = ZSTD_decompressStream(s, out, in);
        zstd_error::check BOOST_PREVENT_MACRO_SUBSTITUTION(result);
    } while (in->pos < in->size && out->pos < out->size);
    return action == zstd::finish && in->size == 0 && out->pos == 0 ? zstd::stream_end : zstd::okay;
}

void zstd_base::reset(bool compress, bool realloc)
{
    ZSTD_inBuffer *in = static_cast<ZSTD_inBuffer *>(in_);
    ZSTD_outBuffer *out = static_cast<ZSTD_outBuffer *>(out_);
    if (realloc)
    {
        memset(in, 0, sizeof(*in));
        memset(out, 0, sizeof(*out));
        eof_ = 0;

		if (compress)
		{
			zstd_error::check BOOST_PREVENT_MACRO_SUBSTITUTION(
				ZSTD_initCStream(static_cast<ZSTD_CStream *>(cstream_), level)
			);

			zstd_error::check BOOST_PREVENT_MACRO_SUBSTITUTION(
				ZSTD_CCtx_setParameter(static_cast<ZSTD_CStream *>(cstream_), ZSTD_c_nbWorkers, iMultiThreading)
			);
		}
		else
		{
			zstd_error::check BOOST_PREVENT_MACRO_SUBSTITUTION(
				ZSTD_initDStream(static_cast<ZSTD_DStream *>(dstream_))
			);
		}
    }
}

void zstd_base::do_init
    ( const zstd_params& p, bool compress,
      zstd::alloc_func, zstd::free_func,
      void* )
{
    ZSTD_inBuffer *in = static_cast<ZSTD_inBuffer *>(in_);
    ZSTD_outBuffer *out = static_cast<ZSTD_outBuffer *>(out_);

    memset(in, 0, sizeof(*in));
    memset(out, 0, sizeof(*out));
    eof_ = 0;

	if (compress)
	{
		level = p.level;
		iMultiThreading = p.iMultiThreading;

		if (!cstream_)
		{
			cstream_ = ZSTD_createCStream();
		}

		zstd_error::check BOOST_PREVENT_MACRO_SUBSTITUTION(
			ZSTD_initCStream(static_cast<ZSTD_CStream *>(cstream_), level)
		);
		
		zstd_error::check BOOST_PREVENT_MACRO_SUBSTITUTION(
			ZSTD_CCtx_setParameter(static_cast<ZSTD_CStream *>(cstream_), ZSTD_c_nbWorkers, iMultiThreading)
		);
	}
	else
	{
		if (!dstream_)
		{
			dstream_ = ZSTD_createDStream();
		}

		zstd_error::check BOOST_PREVENT_MACRO_SUBSTITUTION(
			ZSTD_initDStream(static_cast<ZSTD_DStream *>(dstream_))
		);
	}
}

} // End namespace detail.

//----------------------------------------------------------------------------//

} } // End namespaces iostreams, boost.

