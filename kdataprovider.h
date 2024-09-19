/*
 //
 // DEKAF(tm): Lighter, Faster, Smarter(tm)
 //
 // Copyright (c) 2024, Ridgeware, Inc.
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

#pragma once

/// @file kdataprovider.h
/// class hierarchy of providers of data input

#include "kdefinitions.h"
#include "kbuffer.h"
#include "kstringview.h"
#include "kstring.h"
#include <cstdint>
#include <iostream>

DEKAF2_NAMESPACE_BEGIN

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// the interface of a data provider class
class KDataProvider
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	KDataProvider () = default;
	virtual ~KDataProvider ();

	/// if asked to provide size_t bytes
	/// @return how many would really be provided
	virtual std::size_t CalcNextReadSize (std::size_t) const { return 0; }
	/// copy size_t bytes into buffer
	/// @return total copied bytes
	virtual std::size_t Read (void* buffer, std::size_t) = 0;
	/// write size_t bytes from input into std::ostream
	/// @return total written bytes
	virtual std::size_t Read (std::ostream&, std::size_t) { return 0; }
	/// set a string view to point to data to read
	/// @param iMax max size of data to return, default = all
	/// @return false if not available, e.g. because the data is not persistant
	virtual bool Read(KStringView&, std::size_t size = npos) { return false; }

	/// @return true if provider can write data into an std::ostream without copying
	virtual bool NoCopy      () const = 0;
	/// @return true if provider is at EOF
	virtual bool IsEOF       () const = 0;
	/// @return true if provider would be at EOF after reading size_t bytes
	virtual bool IsEOFAfter  (std::size_t) const { return false; }

}; // KDataProvider

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// a data provider that reads from constant memory without own buffering (a view)
class KViewProvider : public KDataProvider
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	KViewProvider () = default;
	KViewProvider (KStringView sView) : m_sView(sView) {}
	KViewProvider (const void* data, std::size_t size) : KViewProvider(KStringView(static_cast<const char*>(data), size)) {}
	KViewProvider (KConstBuffer buffer) : KViewProvider(buffer.VoidData(), buffer.size()) {}

	/// if asked to provide size_t bytes
	/// @return how many would really be provided
	virtual std::size_t CalcNextReadSize (std::size_t size) const override final { return std::min(size, m_sView.size()); }
	/// copy size_t bytes into buffer
	/// @return total copied bytes
	virtual std::size_t Read (void* buffer, std::size_t size) override final;
	/// write size_t bytes from input into std::ostream
	/// @return total written bytes
	virtual std::size_t Read (std::ostream& ostream, std::size_t size) override final;
	/// set a string view to point to data to read
	/// @param iMax max size of data to return, default = all
	/// @return false if not available, e.g. because the data is not persistant
	virtual bool Read(KStringView& sView, std::size_t size = npos) override final;

	/// @return true if provider can send data without copying
	virtual bool NoCopy      ()                 const override final { return true;                   }
	/// @return true if provider is at EOF
	virtual bool IsEOF       ()                 const override final { return m_sView.empty();        }
	/// @return true if provider would be at EOF after reading size_t bytes
	virtual bool IsEOFAfter  (std::size_t size) const override final { return m_sView.size() <= size; }

//----------
private:
//----------

	KStringView m_sView;

}; // KViewProvider

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// a data provider that reads from buffered memory (a string)
class KBufferedProvider : public KViewProvider
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	KBufferedProvider () = default;
	KBufferedProvider (KString sData) : m_sData(std::move(sData)) { KViewProvider::operator=(KStringView(m_sData)); }
	KBufferedProvider (const void* data, std::size_t size) : KBufferedProvider(KString(static_cast<const char*>(data), size)) {}
	KBufferedProvider (KConstBuffer buffer) : KBufferedProvider(buffer.VoidData(), buffer.size()) {}

//----------
private:
//----------

	KString m_sData;

}; // KBufferedProvider

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// a data provider that reads from any std::istream
class KIStreamProvider : public KDataProvider
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	KIStreamProvider(std::istream& istream, std::size_t iMaxRead = npos)
	: m_IStream(istream), m_iMaxRead(iMaxRead) {}

	/// copy size_t bytes into buffer
	/// @return total copied bytes
	virtual std::size_t Read(void* buffer, std::size_t size) override final;
	/// @return true if provider can write data into an std::ostream without copying
	virtual bool NoCopy() const override final { return false;            }
	/// @return true if provider is at EOF
	virtual bool IsEOF () const override final;

//----------
private:
//----------

	std::istream& m_IStream;
	std::size_t   m_iMaxRead { npos };

}; // KIStreamProvider

DEKAF2_NAMESPACE_END
