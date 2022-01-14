// adapted for brotli by Ridgeware
// Based on zstd.cpp and zlib.cpp by:
// (C) Copyright Reimar Döffinger 2018.
// (C) Copyright Milan Svoboda 2008.
// (C) Copyright Jonathan Turkanis 2003.
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.)

// See http://www.boost.org/libs/iostreams for documentation.

// Define BOOST_IOSTREAMS_SOURCE so that <boost/iostreams/detail/config.hpp>
// knows that we are building the library (possibly exporting code), rather
// than using it (possibly importing code).
#define BOOST_IOSTREAMS_SOURCE

#include <brotli/types.h>
#include <brotli/decode.h>
#include <brotli/encode.h>

#include <boost/throw_exception.hpp>
#include <boost/iostreams/detail/config/dyn_link.hpp>
#include "kbrotli.h"

namespace boost { namespace iostreams {

namespace brotli {
                    // Compression levels

const uint32_t best_speed           = BROTLI_MIN_QUALITY;
const uint32_t best_compression     = BROTLI_MAX_QUALITY;
//const uint32_t default_compression  = BROTLI_DEFAULT_QUALITY;
const uint32_t default_compression  = 5; // 11 (BROTLI_DEFAULT_QUALITY) is too slow

                    // Status codes

const int okay                 = 0;
const int stream_end           = 1;

                    // Flush codes

const int finish               = 0;
const int flush                = 1;
const int run                  = 2;

} // End namespace brotli.

//------------------Implementation of brotli_error------------------------------//

brotli_error::brotli_error(int error)
: BOOST_IOSTREAMS_FAILURE(BrotliDecoderErrorString(static_cast<BrotliDecoderErrorCode>(error))), error_(error)
    { }

void brotli_error::check BOOST_PREVENT_MACRO_SUBSTITUTION(int error)
{
    if (error < 0)
        boost::throw_exception(brotli_error(error));
}

//------------------Implementation of brotli_base-------------------------------//

namespace detail {

brotli_base::brotli_base()
    : DecoderState_(0)
	, EncoderState_(0)
	, total_out_(0)
	, level(brotli::default_compression)
    { }

brotli_base::~brotli_base()
{
	BrotliDecoderDestroyInstance(static_cast<BrotliDecoderState*>(DecoderState_));
	BrotliEncoderDestroyInstance(static_cast<BrotliEncoderState*>(EncoderState_));
}

void brotli_base::before( const char*& src_begin, const char* src_end,
                          char*& dest_begin, char* dest_end )
{
	stream_.next_in   = reinterpret_cast<const uint8_t*>(src_begin );
	stream_.next_out  = reinterpret_cast<      uint8_t*>(dest_begin);
	stream_.out_base  = stream_.next_out; // need this to determine stream_end condition
	stream_.avail_in  = static_cast<size_t>( src_end -  src_begin);
	stream_.avail_out = static_cast<size_t>(dest_end - dest_begin);
}

void brotli_base::after(const char*& src_begin, char*& dest_begin, bool)
{
	src_begin  = reinterpret_cast<const char*>(stream_.next_in );
	dest_begin = reinterpret_cast<      char*>(stream_.next_out);
}

int brotli_base::deflate(int action)
{
	BrotliEncoderOperation op = BROTLI_OPERATION_PROCESS;

	switch (action)
	{
		case brotli::run:
			// process normally, already set
			break;

		case brotli::flush:
			op = BROTLI_OPERATION_FLUSH;
			break;

		case brotli::finish:
			op = BROTLI_OPERATION_FINISH;
			break;
	}

	BROTLI_BOOL result = BrotliEncoderCompressStream(static_cast<BrotliEncoderState*>(EncoderState_), op,
													 &stream_.avail_in, &stream_.next_in,
													 &stream_.avail_out, &stream_.next_out,
													 &total_out_);
	if (result == BROTLI_FALSE)
	{
		// there was an error.. do something
	}

	// check if we are at end
	return (BrotliEncoderIsFinished(static_cast<BrotliEncoderState*>(EncoderState_)) == BROTLI_TRUE) ?
		brotli::stream_end : brotli::okay;
}

int brotli_base::inflate(int action)
{
    // need loop since iostream code cannot handle short reads
    do {

		BrotliDecoderResult result = BrotliDecoderDecompressStream(static_cast<BrotliDecoderState*>(DecoderState_), &stream_.avail_in, &stream_.next_in, &stream_.avail_out, &stream_.next_out, &total_out_);
        brotli_error::check BOOST_PREVENT_MACRO_SUBSTITUTION(result);

    } while (stream_.avail_in && stream_.avail_out);

    return (action == brotli::finish &&
			stream_.avail_in == 0 &&
			stream_.next_out == stream_.out_base)
				? brotli::stream_end : brotli::okay;
}

void brotli_base::reset(bool compress, bool realloc)
{
    if (realloc)
    {
		stream_.clear();
		total_out_ = 0;

		if (compress)
		{
			BrotliEncoderDestroyInstance(static_cast<BrotliEncoderState*>(EncoderState_));
			EncoderState_ = BrotliEncoderCreateInstance(0, 0, opaque_);
			BrotliEncoderSetParameter(static_cast<BrotliEncoderState*>(EncoderState_), BROTLI_PARAM_QUALITY, level);
		}
		else
		{
			BrotliDecoderDestroyInstance(static_cast<BrotliDecoderState*>(DecoderState_));
			DecoderState_ = BrotliDecoderCreateInstance(0, 0, opaque_);
		}
    }
}

void brotli_base::do_init
    ( const brotli_params& p, bool compress,
      brotli::alloc_func, brotli::free_func,
      void* )
{
    level = p.level;
	reset(compress, true);
}

} // End namespace detail.

//----------------------------------------------------------------------------//

} } // End namespaces iostreams, boost.

