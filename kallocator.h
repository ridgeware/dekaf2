/*
// DEKAF(tm): Lighter, Faster, Smarter (tm)
//
// Copyright (c) 2018, Ridgeware, Inc.
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

/// @file kallocator.h
/// provides std::allocator that reserves a certain size on the stack, and
/// only if more storage is needed switches to dynamic allocation

#include <cstddef>
#include <cassert>

namespace dekaf2 {

// This source is largely taken from https://howardhinnant.github.io/short_alloc.h
// and has the following original license note

// The MIT License (MIT)
//
// Copyright (c) 2015 Howard Hinnant
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
template <std::size_t N, std::size_t alignment = alignof(std::max_align_t)>
class KStackArena
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
private:
//------

	alignas(alignment) char buf_[N];
	char* ptr_;

//------
public:
//------

	~KStackArena() { ptr_ = nullptr; }
	KStackArena() noexcept : ptr_(buf_) {}
	KStackArena(const KStackArena&) = delete;
	KStackArena& operator=(const KStackArena&) = delete;

	//-----------------------------------------------------------------------------
	template <std::size_t ReqAlign>
	char* allocate(std::size_t n)
	//-----------------------------------------------------------------------------
	{
		static_assert(ReqAlign <= alignment, "alignment is too small for this arena");
		assert(pointer_in_buffer(ptr_) && "short_alloc has outlived arena");
		auto const aligned_n = align_up(n);
		if (static_cast<decltype(aligned_n)>(buf_ + N - ptr_) >= aligned_n)
		{
			char* r = ptr_;
			ptr_ += aligned_n;
			return r;
		}

		static_assert(alignment <= alignof(std::max_align_t), "you've chosen an "
					  "alignment that is larger than alignof(std::max_align_t), and "
					  "cannot be guaranteed by normal operator new");
		return static_cast<char*>(::operator new(n));
	}

	//-----------------------------------------------------------------------------
	void deallocate(char* p, std::size_t n) noexcept
	//-----------------------------------------------------------------------------
	{
		assert(pointer_in_buffer(ptr_) && "short_alloc has outlived arena");
		if (pointer_in_buffer(p))
		{
			n = align_up(n);
			if (p + n == ptr_)
			{
				ptr_ = p;
			}
		}
		else
		{
			::operator delete(p);
		}
	}

	//-----------------------------------------------------------------------------
	static constexpr std::size_t size() noexcept
	//-----------------------------------------------------------------------------
	{
		return N;
	}

	//-----------------------------------------------------------------------------
	std::size_t used() const noexcept
	//-----------------------------------------------------------------------------
	{
		return static_cast<std::size_t>(ptr_ - buf_);
	}

	//-----------------------------------------------------------------------------
	void reset() noexcept
	//-----------------------------------------------------------------------------
	{
		ptr_ = buf_;
	}

//------
private:
//------

	//-----------------------------------------------------------------------------
	static std::size_t align_up(std::size_t n) noexcept
	//-----------------------------------------------------------------------------
	{
		return (n + (alignment-1)) & ~(alignment-1);
	}

	//-----------------------------------------------------------------------------
	bool pointer_in_buffer(char* p) noexcept
	//-----------------------------------------------------------------------------
	{
		return buf_ <= p && p <= buf_ + N;
	}

};

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
template <class T, std::size_t N, std::size_t Align = alignof(std::max_align_t)>
class KStackAlloc
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	using value_type = T;
	static auto constexpr alignment = Align;
	static auto constexpr size = N;
	using arena_type = KStackArena<size, alignment>;

//------
private:
//------

	arena_type& a_;

//------
public:
//------

	KStackAlloc(const KStackAlloc&) = default;
	KStackAlloc& operator=(const KStackAlloc&) = delete;

	//-----------------------------------------------------------------------------
	KStackAlloc(arena_type& a) noexcept : a_(a)
	//-----------------------------------------------------------------------------
	{
		static_assert(size % alignment == 0, "size N needs to be a multiple of alignment Align");
	}

	//-----------------------------------------------------------------------------
	template <class U>
	KStackAlloc(const KStackAlloc<U, N, alignment>& a) noexcept
	//-----------------------------------------------------------------------------
	: a_(a.a_) {}

	//-----------------------------------------------------------------------------
	template <class _Up>
	struct rebind
	//-----------------------------------------------------------------------------
	{
		using other = KStackAlloc<_Up, N, alignment>;
	};

	//-----------------------------------------------------------------------------
	T* allocate(std::size_t n)
	//-----------------------------------------------------------------------------
	{
		return reinterpret_cast<T*>(a_.template allocate<alignof(T)>(n*sizeof(T)));
	}

	//-----------------------------------------------------------------------------
	void deallocate(T* p, std::size_t n) noexcept
	//-----------------------------------------------------------------------------
	{
		a_.deallocate(reinterpret_cast<char*>(p), n*sizeof(T));
	}

	template <class T1, std::size_t N1, std::size_t A1, class U, std::size_t M, std::size_t A2>
	friend
	bool operator==(const KStackAlloc<T1, N1, A1>& x, const KStackAlloc<U, M, A2>& y) noexcept;

	template <class U, std::size_t M, std::size_t A> friend class KStackAlloc;

};

//-----------------------------------------------------------------------------
template <class T, std::size_t N, std::size_t A1, class U, std::size_t M, std::size_t A2>
inline
bool operator==(const KStackAlloc<T, N, A1>& x, const KStackAlloc<U, M, A2>& y) noexcept
//-----------------------------------------------------------------------------
{
	return N == M && A1 == A2 && &x.a_ == &y.a_;
}

//-----------------------------------------------------------------------------
template <class T, std::size_t N, std::size_t A1, class U, std::size_t M, std::size_t A2>
inline
bool operator!=(const KStackAlloc<T, N, A1>& x, const KStackAlloc<U, M, A2>& y) noexcept
//-----------------------------------------------------------------------------
{
	return !(x == y);
}

} // of namespace dekaf2
