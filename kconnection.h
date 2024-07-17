/*
//
// DEKAF(tm): Lighter, Faster, Smarter (tm)
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
*/

#pragma once

#include "kdefinitions.h"
#include "kstream.h"
#include "kurl.h"
#include "kstreamoptions.h"

DEKAF2_NAMESPACE_BEGIN

class KTLSIOStream;

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class DEKAF2_PUBLIC KConnection
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	//-----------------------------------------------------------------------------
	KConnection() = default;
	KConnection(const KConnection&) = delete;
	KConnection(KConnection&& other) noexcept;
	KConnection& operator=(const KConnection&) = delete;
	KConnection& operator=(KConnection&& other) noexcept;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	explicit KConnection(KStream& Stream, KTCPEndPoint EndPoint = KTCPEndPoint{}) noexcept
	//-----------------------------------------------------------------------------
	: m_Endpoint(std::move(EndPoint))
	, m_Stream(&Stream)
    , m_bStreamIsNotOwned(true)
	{
	}

	//-----------------------------------------------------------------------------
	explicit KConnection(KStream&& Stream, KTCPEndPoint EndPoint = KTCPEndPoint{}) noexcept
	//-----------------------------------------------------------------------------
	: m_Endpoint(std::move(EndPoint))
	, m_Stream(&Stream)
	, m_bStreamIsNotOwned(false)
	{
	}

	//-----------------------------------------------------------------------------
	virtual ~KConnection()
	//-----------------------------------------------------------------------------
	{
		if (m_Stream)
		{
			Disconnect();
		}
	}

	//-----------------------------------------------------------------------------
	virtual bool Good() const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	operator bool() const
	//-----------------------------------------------------------------------------
	{
		return Good();
	}

	//-----------------------------------------------------------------------------
	KConnection& operator=(KStream& Stream);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	KConnection& operator=(KStream&& Stream);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	KStream& Stream()
	//-----------------------------------------------------------------------------
	{
		return *StreamPtr();
	}

	//-----------------------------------------------------------------------------
	KStream* operator->()
	//-----------------------------------------------------------------------------
	{
		return StreamPtr();
	}

	//-----------------------------------------------------------------------------
	KStream& operator*()
	//-----------------------------------------------------------------------------
	{
		return *StreamPtr();
	}

	//-----------------------------------------------------------------------------
	/// disconnect the connection
	void Disconnect();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// always returns false, has no effect in this class
	virtual bool SetTimeout(KDuration Timeout);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// always returns false, has no effect in this class
	virtual bool IsTLS() const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// always returns false, has no effect in this class
	virtual bool SetManualTLSHandshake(bool bYes = true);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// always returns false, has no effect in this class
	virtual bool StartManualTLSHandshake();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// get the underlying KTLSIOStream if this connection is a TLS connection, otherwise returns nullptr
	virtual KTLSIOStream* GetUnderlyingTLSStream();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	// this interface uses KURL instead of KTCPEndPoint to allow construction like "https://www.abc.de" - otherwise the protocol would be lost..
	static std::unique_ptr<KConnection> Create(const KURL& URL, bool bForceTLS = false, KStreamOptions Options = KStreamOptions::None, KDuration Timeout = chrono::seconds(15));
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// always returns nothing
	virtual KString Error() const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// returns the TCP endpoint
	const KTCPEndPoint& EndPoint() const
	//-----------------------------------------------------------------------------
	{
		return m_Endpoint;
	}

//------
protected:
//------

	//-----------------------------------------------------------------------------
	// returns true if connection is Good()
	bool setConnection(std::unique_ptr<KStream>&& Stream, KTCPEndPoint sEndPoint);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	const KStream* StreamPtr() const
	//-----------------------------------------------------------------------------
	{
		return m_Stream.get();
	}

	//-----------------------------------------------------------------------------
	KStream* StreamPtr()
	//-----------------------------------------------------------------------------
	{
		return m_Stream.get();
	}

	KTCPEndPoint m_Endpoint;

//------
private:
//------

	std::unique_ptr<KStream> m_Stream;
	bool m_bStreamIsNotOwned { false };

}; // KConnection


//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class DEKAF2_PUBLIC KTCPConnection : public KConnection
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	using KConnection::KConnection;

	//-----------------------------------------------------------------------------
	virtual bool Good() const override;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	bool Connect(const KTCPEndPoint& Endpoint, KDuration Timeout = KStreamOptions::GetDefaultTimeout());
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// set IO timeout
	virtual bool SetTimeout(KDuration Timeout) override;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// returns an error string if any
	virtual KString Error() const override;
	//-----------------------------------------------------------------------------

};

#ifdef DEKAF2_HAS_UNIX_SOCKETS

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class DEKAF2_PUBLIC KUnixConnection : public KConnection
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	using KConnection::KConnection;

	//-----------------------------------------------------------------------------
	virtual bool Good() const override;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	bool Connect(KStringViewZ sSocketFile, KDuration Timeout = KStreamOptions::GetDefaultTimeout());
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// set IO timeout in seconds
	virtual bool SetTimeout(KDuration Timeout) override;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// returns an error string if any
	virtual KString Error() const override;
	//-----------------------------------------------------------------------------

};

#endif

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class DEKAF2_PUBLIC KTLSConnection : public KConnection
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	using KConnection::KConnection;

	//-----------------------------------------------------------------------------
	virtual bool Good() const override;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	bool Connect(const KTCPEndPoint& Endpoint, KStreamOptions Options, KDuration Timeout = chrono::seconds(15));
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// set IO timeout
	virtual bool SetTimeout(KDuration Timeout) override;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// is this a TLS connection
	virtual bool IsTLS() const override;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// force manual TLS handshake mode
	virtual bool SetManualTLSHandshake(bool bYes = true) override;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// start the manual TLS handshake
	virtual bool StartManualTLSHandshake() override;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	// note: this header on purpose only forward declares the KTLSIOStream class -
	// to make use of the result, include the ktlsstream.h header at the use place
	/// get the underlying KTLSIOStream if this connection is established, otherwise returns nullptr
	virtual KTLSIOStream* GetUnderlyingTLSStream() override;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// returns an error string if any
	virtual KString Error() const override;
	//-----------------------------------------------------------------------------

};

DEKAF2_NAMESPACE_END
