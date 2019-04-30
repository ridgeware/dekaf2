/*
 //
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

/// @file kcgistream.h
/// provides an implementation of a std::istream for CGI

#include "kstring.h"
#include "kstreambuf.h"
#include <vector>

namespace dekaf2
{


//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// std::istream CGI implementation
class KCGIInStream : public std::istream
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	using base_type = std::istream;

//----------
public:
//----------

	//-----------------------------------------------------------------------------
	/// Construcs a CGI stream from an istream
	KCGIInStream(std::istream& stream);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	bool Good() const
	//-----------------------------------------------------------------------------
	{
		return m_Stream.istream->good();
	}

	//-----------------------------------------------------------------------------
	/// Add custom translations from a CGI var to a HTTP header. Has to be set before
	/// any read from the stream.
	void AddCGIVar(KString sCGIVar, KString sHTTPHeader)
	//-----------------------------------------------------------------------------
	{
		m_Stream.AddCGIVar(std::move(sCGIVar), std::move(sHTTPHeader));
	}

	//-----------------------------------------------------------------------------
	/// Converts a HTTP header name into a CGI var name, like "Accept-Encoding" ->
	/// "HTTP_ACCEPT_ENCODING"
	static KString ConvertHTTPHeaderNameToCGIVar(KStringView sHeadername);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Converts a CGI var name into a HTTP header name, like "HTTP_ACCEPT_ENCODING"
	/// -> "Accept-Encoding"
	static KString ConvertCGIVarToHTTPHeaderName(KStringView sCGIVar);
	//-----------------------------------------------------------------------------

	static constexpr KStringViewZ AUTH_PASSWORD           = "AUTH_PASSWORD";
	static constexpr KStringViewZ AUTH_TYPE               = "AUTH_TYPE";
	static constexpr KStringViewZ AUTH_USER               = "AUTH_USER";
	static constexpr KStringViewZ CERT_COOKIE             = "CERT_COOKIE";
	static constexpr KStringViewZ CERT_FLAGS              = "CERT_FLAGS";
	static constexpr KStringViewZ CERT_ISSUER             = "CERT_ISSUER";
	static constexpr KStringViewZ CERT_KEYSIZE            = "CERT_KEYSIZE";
	static constexpr KStringViewZ CERT_SECRETKEYSIZE      = "CERT_SECRETKEYSIZE";
	static constexpr KStringViewZ CERT_SERIALNUMBER       = "CERT_SERIALNUMBER";
	static constexpr KStringViewZ CERT_SERVER_ISSUER      = "CERT_SERVER_ISSUER";
	static constexpr KStringViewZ CERT_SERVER_SUBJECT     = "CERT_SERVER_SUBJECT";
	static constexpr KStringViewZ CERT_SUBJECT            = "CERT_SUBJECT";
	static constexpr KStringViewZ CF_TEMPLATE_PATH        = "CF_TEMPLATE_PATH";
	static constexpr KStringViewZ CONTENT_LENGTH          = "CONTENT_LENGTH";
	static constexpr KStringViewZ CONTENT_TYPE            = "CONTENT_TYPE";
	static constexpr KStringViewZ CONTEXT_PATH            = "CONTEXT_PATH";
	static constexpr KStringViewZ DOCUMENT_ROOT           = "DOCUMENT_ROOT";
	static constexpr KStringViewZ GATEWAY_INTERFACE       = "GATEWAY_INTERFACE";
	static constexpr KStringViewZ HTTPS                   = "HTTPS";
	static constexpr KStringViewZ HTTPS_KEYSIZE           = "HTTPS_KEYSIZE";
	static constexpr KStringViewZ HTTPS_SECRETKEYSIZE     = "HTTPS_SECRETKEYSIZE";
	static constexpr KStringViewZ HTTPS_SERVER_ISSUER     = "HTTPS_SERVER_ISSUER";
	static constexpr KStringViewZ HTTPS_SERVER_SUBJECT    = "HTTPS_SERVER_SUBJECT";
	static constexpr KStringViewZ HTTP_ACCEPT             = "HTTP_ACCEPT";
	static constexpr KStringViewZ HTTP_ACCEPT_ENCODING    = "HTTP_ACCEPT_ENCODING";
	static constexpr KStringViewZ HTTP_ACCEPT_LANGUAGE    = "HTTP_ACCEPT_LANGUAGE";
	static constexpr KStringViewZ HTTP_AUTHORIZATION      = "HTTP_AUTHORIZATION";
	static constexpr KStringViewZ HTTP_CONNECTION         = "HTTP_CONNECTION";
	static constexpr KStringViewZ HTTP_COOKIE             = "HTTP_COOKIE";
	static constexpr KStringViewZ HTTP_HOST               = "HTTP_HOST";
	static constexpr KStringViewZ HTTP_REFERER            = "HTTP_REFERER";
	static constexpr KStringViewZ HTTP_USER_AGENT         = "HTTP_USER_AGENT";
	static constexpr KStringViewZ PATH                    = "PATH";
	static constexpr KStringViewZ QUERY_STRING            = "QUERY_STRING";
	static constexpr KStringViewZ REMOTE_ADDR             = "REMOTE_ADDR";
	static constexpr KStringViewZ REMOTE_HOST             = "REMOTE_HOST";
	static constexpr KStringViewZ REMOTE_PORT             = "REMOTE_PORT";
	static constexpr KStringViewZ REMOTE_USER             = "REMOTE_USER";
	static constexpr KStringViewZ REQUEST_METHOD          = "REQUEST_METHOD";
	static constexpr KStringViewZ REQUEST_URI             = "REQUEST_URI";
	static constexpr KStringViewZ SCRIPT_NAME             = "SCRIPT_NAME";
	static constexpr KStringViewZ SERVER_NAME             = "SERVER_NAME";
	static constexpr KStringViewZ SERVER_PORT             = "SERVER_PORT";
	static constexpr KStringViewZ SERVER_PORT_SECURE      = "SERVER_PORT_SECURE";
	static constexpr KStringViewZ SERVER_PROTOCOL         = "SERVER_PROTOCOL";
	static constexpr KStringViewZ SERVER_SOFTWARE         = "SERVER_SOFTWARE";
	static constexpr KStringViewZ WEB_SERVER_API          = "WEB_SERVER_API";

//----------
private:
//----------

	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	class Stream
	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{
	//----------
	public:
	//----------

		Stream(std::istream* _istream)
		: istream(_istream)
		{}

		void CreateHeader();
		void AddCGIVar(KString sCGIVar, KString sHTTPHeader);

		std::istream* istream;
		KStringView   sHeader;
		char          chCommentDelimiter { 0 };
		bool          bAtStartOfLine { true }; // was the last character of the last buffer read an EOL?
		bool          bIsComment { false };
		bool          bIsCreated { false };

	//----------
	private:
	//----------

		std::vector<std::pair<KString, KString> > m_AdditionalCGIVars;
		KString m_sHeader;

	}; // Stream

	Stream m_Stream;

	KInStreamBuf m_StreamBuf{&StreamReader, &m_Stream};

	//-----------------------------------------------------------------------------
	/// this is the custom streambuf reader
	static std::streamsize StreamReader(void* sBuffer, std::streamsize iCount, void* stream);
	//-----------------------------------------------------------------------------

}; // KCGIInStream

} // namespace dekaf2

