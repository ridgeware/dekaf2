/*
//-----------------------------------------------------------------------------//
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

#include "kstringview.h"
#include "kwriter.h"
#include <boost/container/vector.hpp>

namespace dekaf2 {

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class KCharSet
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	static constexpr KStringViewZ ANY_ISO8859         = "ISO-8859"; /*-1...*/
	static constexpr KStringViewZ DEFAULT_CHARSET     = "WINDOWS-1252";

}; // KCharSet

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class KMIME
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	//-----------------------------------------------------------------------------
	constexpr
	KMIME()
	//-----------------------------------------------------------------------------
	    : m_mime()
	{}

	//-----------------------------------------------------------------------------
	constexpr
	KMIME(KStringView sv)
	//-----------------------------------------------------------------------------
	    : m_mime(sv)
	{}

	//-----------------------------------------------------------------------------
	constexpr
	operator KStringView() const
	//-----------------------------------------------------------------------------
	{
		return m_mime;
	}

	static constexpr KStringViewZ NONE                   = "";
	static constexpr KStringViewZ TEXT_PLAIN             = "text/plain";
	static constexpr KStringViewZ TEXT_UTF8              = "text/plain; charset=UTF-8";
	static constexpr KStringViewZ JSON_UTF8              = "application/json; charset=UTF-8";
	static constexpr KStringViewZ HTML_UTF8              = "text/html; charset=UTF-8";
	static constexpr KStringViewZ XML_UTF8               = "text/xml; charset=UTF-8";
	static constexpr KStringViewZ SWF                    = "application/x-shockwave-flash";
	static constexpr KStringViewZ WWW_FORM_URLENCODED    = "application/x-www-form-urlencoded";
	static constexpr KStringViewZ MULTIPART_FORM_DATA    = "multipart/form-data";
	static constexpr KStringViewZ MULTIPART_ALTERNATIVE  = "multipart/alternative";
	static constexpr KStringViewZ MULTIPART_MIXED        = "multipart/mixed";
	static constexpr KStringViewZ MULTIPART_RELATED      = "multipart/related";
	static constexpr KStringViewZ APPLICATION_BINARY     = "application/octet-stream";
	static constexpr KStringViewZ APPLICATION_JAVASCRIPT = "application/javascript";
	static constexpr KStringViewZ IMAGE_JPEG             = "image/jpeg";

//------
private:
//------

	KStringView m_mime{TEXT_PLAIN};

}; // KMIME

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class KMIMEPart
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	KMIMEPart(KMIME MIME = KMIME::NONE) : m_MIME(MIME) {}
	KMIMEPart(KStringView sMessage, KMIME MIME) : m_MIME(MIME), m_Data(sMessage) {}
	KMIMEPart& operator=(KStringView sv)  { m_Data = sv;  return *this; }
	KMIMEPart& operator+=(KStringView sv) { m_Data += sv; return *this; }
	/// Add content of file sFileName to MIME part, use sDispName as attachment name
	/// else basename of sFileName
	bool File(KStringView sFilename, KStringView sDispname = KStringView{});
	/// Add content of Stream to MIME part
	bool Stream(KInStream& Stream, KStringView sDispname = "untitled");

	/// Is this a multipart structure?
	bool IsMultiPart() const;
	/// Is this a binary content?
	bool IsBinary() const;

	/// Attach another part to this multipart structure - returns false if this->MIME type is not multipart
	bool Attach(KMIMEPart&& part);
	/// Attach another part to this multipart structure - returns false if this->MIME type is not multipart
	bool Attach(const KMIMEPart& part)
	{
		return Attach(KMIMEPart(part));
	}
	/// Attach another part to this multipart structure - fails if this->MIME type is not multipart
	KMIMEPart& operator+=(KMIMEPart&& part) { Attach(std::move(part)); return *this; }
	/// Attach another part to this multipart structure - fails if this->MIME type is not multipart
	KMIMEPart& operator+=(const KMIMEPart& part) { Attach(part); return *this; }

	bool Serialize(KString& sOut, uint16_t recursion = 0) const;
	bool Serialize(KOutStream& Stream, uint16_t recursion = 0) const;
	KString Serialize(uint16_t recursion = 0) const;

	bool empty() const { return m_Parts.empty(); }
	bool size()  const { return m_Parts.size();  }
	auto begin() { return m_Parts.begin(); }
	auto end() { return m_Parts.end(); }
	KMIMEPart& operator[](size_t pos) { return m_Parts[pos]; }
	const KMIMEPart& operator[](size_t pos) const { return m_Parts[pos]; }

	KMIME MIME() const { return m_MIME; }
	const KString& Data() const { return m_Data; }
	const KString& Name() const { return m_sName; }

//----------
protected:
//----------

	KMIME   m_MIME;
	KString m_Data;
	KString m_sName;

	// boost::container allows use of incomplete types
	boost::container::vector<KMIMEPart> m_Parts;

};

using KMIMEMultiPart = KMIMEPart;

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class KMIMEText : public KMIMEPart
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	KMIMEText(KStringView sMessage = KStringView{}) : KMIMEPart(sMessage, KMIME::TEXT_UTF8) {}

};

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class KMIMEHTML : public KMIMEPart
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	KMIMEHTML(KStringView sMessage = KStringView{}) : KMIMEPart(sMessage, KMIME::HTML_UTF8) {}

};

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class KMIMEFile : public KMIMEPart
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	KMIMEFile(KStringView sFilename, KMIME MIME = KMIME::APPLICATION_BINARY) : KMIMEPart(MIME) { File(sFilename); }

};

} // end of namespace dekaf2
