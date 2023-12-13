// adapted for brotli by Ridgeware 2022.
// Based on zstd.h and zlib.h by:
// (C) Copyright Reimar Döffinger 2018.
// (C) Copyright Milan Svoboda 2008.
// (C) Copyright Jonathan Turkanis 2003.
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.)

// See http://www.boost.org/libs/iostreams for documentation.

#ifndef KBROTLI_H_SDHSDFIUGHISDOPIJSHIGUHVXJNDISOHVUGS
#define KBROTLI_H_SDHSDFIUGHISDOPIJSHIGUHVXJNDISOHVUGS

#if defined(_MSC_VER)
# pragma once
#endif

#include <cassert>
#include <iosfwd>            // streamsize.
#include <memory>            // allocator, bad_alloc.
#include <new>
#include <boost/config.hpp>  // MSVC, STATIC_CONSTANT, DEDUCED_TYPENAME, DINKUM.
#include <boost/detail/workaround.hpp>
#include <boost/iostreams/constants.hpp>   // buffer size.
#include <boost/iostreams/detail/config/auto_link.hpp>
#include <boost/iostreams/detail/config/dyn_link.hpp>
#include <boost/iostreams/detail/config/wide_streams.hpp>
#include <boost/iostreams/detail/ios.hpp>  // failure, streamsize.
#include <boost/iostreams/filter/symmetric.hpp>
#include <boost/iostreams/pipeline.hpp>
#include <boost/type_traits/is_same.hpp>

// Must come last.
#ifdef BOOST_MSVC
# pragma warning(push)
# pragma warning(disable:4251 4231 4660)         // Dependencies not exported.
#endif
#include <boost/config/abi_prefix.hpp>

// note we use the literal dekaf2 namespace here and not
// the define from kdefinitions.h
namespace dekaf2 { namespace iostreams {

namespace brotli {

typedef void* (*alloc_func)(void*, size_t, size_t);
typedef void (*free_func)(void*, void*);

                    // Compression levels

BOOST_IOSTREAMS_DECL extern const uint32_t best_speed;
BOOST_IOSTREAMS_DECL extern const uint32_t best_compression;
BOOST_IOSTREAMS_DECL extern const uint32_t default_compression;

                    // Status codes

BOOST_IOSTREAMS_DECL extern const int okay;
BOOST_IOSTREAMS_DECL extern const int stream_end;

                    // Flush codes

BOOST_IOSTREAMS_DECL extern const int finish;
BOOST_IOSTREAMS_DECL extern const int flush;
BOOST_IOSTREAMS_DECL extern const int run;

                    // Code for current OS

                    // Null pointer constant.

const int null                               = 0;

                    // Default values

} // End namespace brotli

//
// Class name: brotli_params.
// Description: Encapsulates the parameters passed to brotlidec_init
//      to customize compression and decompression.
//
struct brotli_params {

    // Non-explicit constructor.
    brotli_params( uint32_t level = brotli::default_compression )
        : level(level)
        { }
    uint32_t level;
};

//
// Class name: brotli_error.
// Description: Subclass of std::ios::failure thrown to indicate
//     brotli errors other than out-of-memory conditions.
//
class BOOST_IOSTREAMS_DECL brotli_error : public BOOST_IOSTREAMS_FAILURE {
public:
	brotli_error();
	explicit brotli_error(int error);
    size_t error() const { return error_; }
    static void check BOOST_PREVENT_MACRO_SUBSTITUTION(int error);
private:
    size_t error_;
};

namespace detail {

template<typename Alloc>
struct brotli_allocator_traits {
#ifndef BOOST_NO_STD_ALLOCATOR
#if defined(BOOST_NO_CXX11_ALLOCATOR)
    typedef typename Alloc::template rebind<char>::other type;
#else
    typedef typename std::allocator_traits<Alloc>::template rebind_alloc<char> type;
#endif
#else
    typedef std::allocator<char> type;
#endif
};

template< typename Alloc,
          typename Base = // VC6 workaround (C2516)
              BOOST_DEDUCED_TYPENAME brotli_allocator_traits<Alloc>::type >
struct brotli_allocator : private Base {
private:
#if defined(BOOST_NO_CXX11_ALLOCATOR) || defined(BOOST_NO_STD_ALLOCATOR)
    typedef typename Base::size_type size_type;
#else
    typedef typename std::allocator_traits<Base>::size_type size_type;
#endif
public:
    BOOST_STATIC_CONSTANT(bool, custom =
        (!std::is_same<std::allocator<char>, Base>::value));
    typedef typename brotli_allocator_traits<Alloc>::type allocator_type;
    static void* allocate(void* self, size_t items, size_t size);
    static void deallocate(void* self, void* address);
};

class BOOST_IOSTREAMS_DECL brotli_base {
public:
    typedef char char_type;
protected:
	brotli_base();
    ~brotli_base();
    template<typename Alloc>
    void init( const brotli_params& p,
               bool compress,
               brotli_allocator<Alloc>& balloc )
        {
            bool custom = brotli_allocator<Alloc>::custom;
            do_init( p, compress,
                     custom ? brotli_allocator<Alloc>::allocate : 0,
                     custom ? brotli_allocator<Alloc>::deallocate : 0,
                     &balloc );
        }
    void before( const char*& src_begin, const char* src_end,
                 char*& dest_begin, char* dest_end );
    void after( const char*& src_begin, char*& dest_begin,
                bool compress );
    int deflate(int action);
    int inflate(int action);
    void reset(bool compress, bool realloc);
	size_t total_out() const { return total_out_; }
private:
    void do_init( const brotli_params& p, bool compress,
                  brotli::alloc_func,
                  brotli::free_func,
                  void* derived );

	struct StreamIO {
		size_t         avail_in;
		const uint8_t* next_in;
		size_t         avail_out;
		uint8_t*       next_out;
		uint8_t*       out_base;

		void        clear()   { memset(this, 0, sizeof(StreamIO)); }
	};

	StreamIO stream_;
	void* DecoderState_;
	void* opaque_;
	void* EncoderState_;
	size_t total_out_;
	uint32_t level;
};

//
// Template name: brotli_compressor_impl
// Description: Model of C-Style Filter implementing compression by
//      delegating to the brotli function deflate.
//
template<typename Alloc = std::allocator<char> >
class brotli_compressor_impl : public brotli_base, public brotli_allocator<Alloc> {
public:
	brotli_compressor_impl(const brotli_params& = brotli::default_compression);
    ~brotli_compressor_impl();
    bool filter( const char*& src_begin, const char* src_end,
                 char*& dest_begin, char* dest_end, bool flush );
    void close();
};

//
// Template name: brotli_compressor_impl
// Description: Model of C-Style Filter implementing decompression by
//      delegating to the brotli function inflate.
//
template<typename Alloc = std::allocator<char> >
class brotli_decompressor_impl : public brotli_base, public brotli_allocator<Alloc> {
public:
    brotli_decompressor_impl(const brotli_params&);
    brotli_decompressor_impl();
    ~brotli_decompressor_impl();
    bool filter( const char*& begin_in, const char* end_in,
                 char*& begin_out, char* end_out, bool flush );
    void close();
	bool eof() const { return eof_; }
private:
	bool eof_;
};

} // End namespace detail.

//
// Template name: brotli_compressor
// Description: Model of InputFilter and OutputFilter implementing
//      compression using brotli.
//
template<typename Alloc = std::allocator<char> >
struct basic_brotli_compressor
    : boost::iostreams::symmetric_filter<detail::brotli_compressor_impl<Alloc>, Alloc>
{
private:
    typedef detail::brotli_compressor_impl<Alloc> impl_type;
    typedef boost::iostreams::symmetric_filter<impl_type, Alloc>  base_type;
public:
    typedef typename base_type::char_type               char_type;
    typedef typename base_type::category                category;
    basic_brotli_compressor( const brotli_params& = brotli::default_compression,
							std::streamsize buffer_size = boost::iostreams::default_device_buffer_size );
};
BOOST_IOSTREAMS_PIPABLE(basic_brotli_compressor, 1)

typedef basic_brotli_compressor<> brotli_compressor;

//
// Template name: brotli_decompressor
// Description: Model of InputFilter and OutputFilter implementing
//      decompression using brotli.
//
template<typename Alloc = std::allocator<char> >
struct basic_brotli_decompressor
    : boost::iostreams::symmetric_filter<detail::brotli_decompressor_impl<Alloc>, Alloc>
{
private:
    typedef detail::brotli_decompressor_impl<Alloc> impl_type;
    typedef boost::iostreams::symmetric_filter<impl_type, Alloc>    base_type;
public:
    typedef typename base_type::char_type               char_type;
    typedef typename base_type::category                category;
	basic_brotli_decompressor( std::streamsize buffer_size = boost::iostreams::default_device_buffer_size );
    basic_brotli_decompressor( const brotli_params& p,
                             std::streamsize buffer_size = boost::iostreams::default_device_buffer_size );
	size_t total_out() const { return this->filter().total_out(); }
};
BOOST_IOSTREAMS_PIPABLE(basic_brotli_decompressor, 1)

typedef basic_brotli_decompressor<> brotli_decompressor;

//----------------------------------------------------------------------------//

//------------------Implementation of brotli_allocator--------------------------//

namespace detail {

template<typename Alloc, typename Base>
void* brotli_allocator<Alloc, Base>::allocate
    (void* self, size_t items, size_t size)
{
    size_type len = items * size;
    char* ptr =
        static_cast<allocator_type*>(self)->allocate
            (len + sizeof(size_type)
            #if BOOST_WORKAROUND(BOOST_DINKUMWARE_STDLIB, == 1)
                , (char*)0
            #endif
            );
    *reinterpret_cast<size_type*>(ptr) = len;
    return ptr + sizeof(size_type);
}

template<typename Alloc, typename Base>
void brotli_allocator<Alloc, Base>::deallocate(void* self, void* address)
{
    char* ptr = reinterpret_cast<char*>(address) - sizeof(size_type);
    size_type len = *reinterpret_cast<size_type*>(ptr) + sizeof(size_type);
    static_cast<allocator_type*>(self)->deallocate(ptr, len);
}

//------------------Implementation of brotli_compressor_impl--------------------//

template<typename Alloc>
brotli_compressor_impl<Alloc>::brotli_compressor_impl(const brotli_params& p)
{ init(p, true, static_cast<brotli_allocator<Alloc>&>(*this)); }

template<typename Alloc>
brotli_compressor_impl<Alloc>::~brotli_compressor_impl()
{ reset(true, false); }

template<typename Alloc>
bool brotli_compressor_impl<Alloc>::filter
    ( const char*& src_begin, const char* src_end,
      char*& dest_begin, char* dest_end, bool flush )
{
    before(src_begin, src_end, dest_begin, dest_end);
    int result = deflate(flush ? brotli::finish : brotli::run);
    after(src_begin, dest_begin, true);
    return result != brotli::stream_end;
}

template<typename Alloc>
void brotli_compressor_impl<Alloc>::close() { reset(true, true); }

//------------------Implementation of brotli_decompressor_impl------------------//

template<typename Alloc>
brotli_decompressor_impl<Alloc>::brotli_decompressor_impl(const brotli_params& p)
: eof_(false)
{ init(p, false, static_cast<brotli_allocator<Alloc>&>(*this)); }

template<typename Alloc>
brotli_decompressor_impl<Alloc>::~brotli_decompressor_impl()
{ reset(false, false); }

template<typename Alloc>
brotli_decompressor_impl<Alloc>::brotli_decompressor_impl()
: eof_(false)
{
	brotli_params p;
    init(p, false, static_cast<brotli_allocator<Alloc>&>(*this));
}

template<typename Alloc>
bool brotli_decompressor_impl<Alloc>::filter
    ( const char*& src_begin, const char* src_end,
      char*& dest_begin, char* dest_end, bool flush )
{
    before(src_begin, src_end, dest_begin, dest_end);
    int result = inflate(flush ? brotli::finish : brotli::run);
    after(src_begin, dest_begin, false);
    return !(eof_ = result == brotli::stream_end);
}

template<typename Alloc>
void brotli_decompressor_impl<Alloc>::close() {
	eof_ = false;
	reset(false, true);
}

} // End namespace detail.

//------------------Implementation of brotli_compressor-----------------------//

template<typename Alloc>
basic_brotli_compressor<Alloc>::basic_brotli_compressor
    (const brotli_params& p, std::streamsize buffer_size)
    : base_type(buffer_size, p) { }

//------------------Implementation of brotli_decompressor-----------------------//

template<typename Alloc>
basic_brotli_decompressor<Alloc>::basic_brotli_decompressor
    (std::streamsize buffer_size)
    : base_type(buffer_size) { }

template<typename Alloc>
basic_brotli_decompressor<Alloc>::basic_brotli_decompressor
    (const brotli_params& p, std::streamsize buffer_size)
    : base_type(buffer_size, p) { }

//----------------------------------------------------------------------------//

} } // End namespaces iostreams, dekaf2.

#include <boost/config/abi_suffix.hpp> // Pops abi_suffix.hpp pragmas.
#ifdef BOOST_MSVC
# pragma warning(pop)
#endif

#endif // KBROTLI_H_SDHSDFIUGHISDOPIJSHIGUHVXJNDISOHVUGS
