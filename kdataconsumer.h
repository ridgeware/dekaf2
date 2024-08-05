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

/// @file kdataconsumer.h
/// class hierarchy of receivers of data output

#include "kdefinitions.h"
#include "kstringview.h"
#include "kstring.h"
#include "kerror.h"
#include "kbuffer.h"

#include <cstdint>
#include <iostream>

DEKAF2_NAMESPACE_BEGIN

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// the interface of a data consumer class, taking incoming bytes
class KDataConsumer : public KErrorBase
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	KDataConsumer() = default;
	virtual ~KDataConsumer() {}

	/// write size_t bytes into buffer
	virtual std::size_t Write (const void* buffer , std::size_t size) = 0;
	/// marking consumer as completed / finished (all data received)
	void                SetFinished ();
	/// returns true if consumer is finished
	bool                IsFinished  () const { return m_bFinished; }

//----------
protected:
//----------

	virtual void        CallFinishedCallback() = 0;

//----------
private:
//----------

	bool  m_bFinished { false };

}; // KDataConsumer

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// data consumer class writing into a view (external buffer)
class KViewConsumer : public KDataConsumer
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	/// construct around data and capacity
	KViewConsumer(void* data, std::size_t capacity) : m_Buffer(data, capacity) {}
	/// Construct around data and capacity, and a callback function that is called when all bytes for the request
	/// have been received. The callback should return without blocking!
	KViewConsumer(void* data, std::size_t capacity, std::function<void(KViewConsumer&)> callback) : m_Callback(std::move(callback)), m_Buffer(data, capacity) {}
	/// write size_t bytes into view
	virtual std::size_t Write (const void* buffer , std::size_t size) override final;
	/// get total size written into view
	std::size_t size() const { return m_Buffer.size(); }

//----------
protected:
//----------

	virtual void        CallFinishedCallback() override final;

//----------
private:
//----------
	
	std::function<void(KViewConsumer&)> m_Callback { nullptr };
	KBuffer m_Buffer;

}; // KViewConsumer

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// data consumer class writing into a KString reference
class KStringConsumer : public KDataConsumer
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	/// Construct around a string buffer
	KStringConsumer(KString& sBuffer) : m_sBuffer(sBuffer) {}
	/// Construct around a string buffer, and a callback function that is called when all bytes for the request
	/// have been received. The callback should return without blocking!
	KStringConsumer(KString& sBuffer, std::function<void(KStringConsumer&)> callback) : m_Callback(std::move(callback)), m_sBuffer(sBuffer) {}
	/// write size_t bytes into string
	virtual std::size_t Write (const void* buffer , std::size_t) override final;
	/// return reference for the string buffer
	KString& GetData() { return m_sBuffer; }

//----------
protected:
//----------

	virtual void        CallFinishedCallback() override;

//----------
private:
//----------

	std::function<void(KStringConsumer&)> m_Callback { nullptr };
	KString& m_sBuffer;

}; // KStringConsumer

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// data consumer class writing into an internal buffer (a string)
class KBufferedConsumer : public KStringConsumer
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	/// Construct around an internal string buffer
	KBufferedConsumer() : KStringConsumer(m_sBuffer) {}
	/// Construct around an internal string buffer, and a callback function that is called when all bytes for the request
	/// have been received. The callback should return without blocking!
	KBufferedConsumer(std::function<void(KBufferedConsumer&)> callback) : KStringConsumer(m_sBuffer), m_Callback(std::move(callback)) {}

//----------
protected:
//----------

	virtual void        CallFinishedCallback() override final;

//----------
private:
//----------

	std::function<void(KBufferedConsumer&)> m_Callback { nullptr };
	KString m_sBuffer;

}; // KBufferedConsumer

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// data consumer class writing into a std::ostream
class KOStreamConsumer : public KDataConsumer
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	/// Construct around a std::ostream
	KOStreamConsumer(std::ostream& ostream) : m_OStream(ostream) {}
	/// Construct around a std::ostream, and a callback function that is called when all bytes for the request
	/// have been received. The callback should return without blocking!
	KOStreamConsumer(std::ostream& ostream, std::function<void(KOStreamConsumer&)> callback) : m_Callback(std::move(callback)), m_OStream(ostream) {}
	/// write size_t bytes into ostream
	virtual std::size_t Write (const void* buffer , std::size_t) override final;

//----------
protected:
//----------

	std::function<void(KOStreamConsumer&)> m_Callback { nullptr };
	virtual void        CallFinishedCallback() override final;

//----------
private:
//----------

	std::ostream& m_OStream;

}; // KOstreamConsumer

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// data consumer class writing through a generic callback
class KCallbackConsumer : public KDataConsumer
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	/// Construct around a write callback, the callback should return without blocking!
	/// If size is 0 end of file is reached (and the void ptr then points to this class instance)
	KCallbackConsumer(std::function<std::size_t(const void*, std::size_t)> callback) : m_WriteCallback(callback) {}
	/// write size_t bytes into callback
	virtual std::size_t Write (const void* buffer , std::size_t) override final;

//----------
protected:
//----------

	std::function<void(KCallbackConsumer&)> m_Callback { nullptr };
	virtual void        CallFinishedCallback() override final;

//----------
private:
//----------

	std::function<std::size_t(const void*, std::size_t)> m_WriteCallback;

}; // KCallbackConsumer

DEKAF2_NAMESPACE_END
