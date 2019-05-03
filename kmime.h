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

#include <boost/container/vector.hpp>
#include "kstringview.h"
#include "kwriter.h"
#include "kreader.h"
#include "kreplacer.h"

namespace dekaf2 {

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// a MIME type
class KMIME
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	//-----------------------------------------------------------------------------
	/// Construct an empty MIME type (NONE)
	constexpr
	KMIME()
	//-----------------------------------------------------------------------------
	    : m_mime()
	{}

	//-----------------------------------------------------------------------------
	/// Construct a MIME type with an arbitrary type
	constexpr
	KMIME(KStringView sv)
	//-----------------------------------------------------------------------------
	    : m_mime(sv)
	{}

#ifdef _MSC_VER
	//-----------------------------------------------------------------------------
	// MSC has issues with conversions from KStringViewZ to KStringView, therefore
	// we add this constructor
	/// Construct a MIME type with an arbitrary type
	constexpr
	KMIME(KStringViewZ svz)
	//-----------------------------------------------------------------------------
		: m_mime(svz)
	{}
#endif

	//-----------------------------------------------------------------------------
	/// Set MIME type according to the extension of sFilename. Use Default if no
	/// association found.
	bool ByExtension(KStringView sFilename, KMIME Default = NONE);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Generate a KMIME instance with the MIME type set according to the extension
	/// of sFilename. Use Default if no association found.
	static KMIME CreateByExtension(KStringView sFilename, KMIME Default = NONE);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// return the KStringView version of the MIME type
	constexpr
	operator KStringView() const
	//-----------------------------------------------------------------------------
	{
		return m_mime;
	}

	//-----------------------------------------------------------------------------
	constexpr
	bool operator==(const KMIME& other) const
	//-----------------------------------------------------------------------------
	{
		return m_mime == other.m_mime;
	}

	//-----------------------------------------------------------------------------
	constexpr
	bool operator!=(const KMIME& other) const
	//-----------------------------------------------------------------------------
	{
		return m_mime != other.m_mime;
	}

	static constexpr KStringViewZ NONE                   = "";

	static constexpr KStringViewZ AAC                    = "audio/aac";
	static constexpr KStringViewZ MIDI                   = "audio/midi";
	static constexpr KStringViewZ OGA                    = "audio/ogg";
	static constexpr KStringViewZ WAV                    = "audio/wav";
	static constexpr KStringViewZ WEBA                   = "audio/webm";

	static constexpr KStringViewZ BINARY                 = "application/octet-stream";
	static constexpr KStringViewZ JAVASCRIPT             = "application/javascript";
	static constexpr KStringViewZ JSON                   = "application/json";
	static constexpr KStringViewZ XML                    = "application/xml";
	static constexpr KStringViewZ SWF                    = "application/x-shockwave-flash";
	static constexpr KStringViewZ WWW_FORM_URLENCODED    = "application/x-www-form-urlencoded";
	static constexpr KStringViewZ AZV                    = "application/vnd.amazon.ebook";
	static constexpr KStringViewZ BZ2                    = "application/x-bzip2";
	static constexpr KStringViewZ CSH                    = "application/x-csh";
	static constexpr KStringViewZ DOC                    = "application/msword";
	static constexpr KStringViewZ DOCX                   = "application/vnd.openxmlformats-officedocument.wordprocessingml.document";
	static constexpr KStringViewZ EPUB                   = "application/epub+zip";
	static constexpr KStringViewZ JAR                    = "application/java-archive";
	static constexpr KStringViewZ ODP                    = "application/vnd.oasis.opendocument.presentation";
	static constexpr KStringViewZ ODS                    = "application/vnd.oasis.opendocument.spreadsheet";
	static constexpr KStringViewZ ODT                    = "application/vnd.oasis.opendocument.text";
	static constexpr KStringViewZ OGX                    = "application/ogg";
	static constexpr KStringViewZ PDF                    = "application/pdf";
	static constexpr KStringViewZ PPT                    = "application/vnd.ms-powerpoint";
	static constexpr KStringViewZ PPTX                   = "application/vnd.openxmlformats-officedocument.presentationml.presentation";
	static constexpr KStringViewZ RAR                    = "application/x-rar-compressed";
	static constexpr KStringViewZ RTF                    = "application/rtf";
	static constexpr KStringViewZ SH                     = "application/x-sh";
	static constexpr KStringViewZ TAR                    = "application/x-tar";
	static constexpr KStringViewZ TS                     = "application/typescript";
	static constexpr KStringViewZ VSD                    = "application/vnd.visio";
	static constexpr KStringViewZ XHTML                  = "application/xhtml+xml";
	static constexpr KStringViewZ XLS                    = "application/vnd.ms-excel";
	static constexpr KStringViewZ XLSX                   = "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet";
	static constexpr KStringViewZ ZIP                    = "application/zip";
	static constexpr KStringViewZ SEVENZIP               = "application/x-7z-compressed";

	static constexpr KStringViewZ EOT                    = "font/vnd.ms-fontobject";
	static constexpr KStringViewZ OTF                    = "font/otf";
	static constexpr KStringViewZ TTC                    = "font/collection";
	static constexpr KStringViewZ TTF                    = "font/ttf";
	static constexpr KStringViewZ WOFF                   = "font/woff";
	static constexpr KStringViewZ WOFF2                  = "font/woff2";

	static constexpr KStringViewZ BMP                    = "image/bmp";
	static constexpr KStringViewZ JPEG                   = "image/jpeg";
	static constexpr KStringViewZ GIF                    = "image/gif";
	static constexpr KStringViewZ PNG                    = "image/png";
	static constexpr KStringViewZ ICON                   = "image/x-icon";
	static constexpr KStringViewZ SVG                    = "image/svg+xml";
	static constexpr KStringViewZ TIFF                   = "image/tiff";
	static constexpr KStringViewZ WEBP                   = "image/webp";

	static constexpr KStringViewZ MULTIPART_FORM_DATA    = "multipart/form-data";
	static constexpr KStringViewZ MULTIPART_ALTERNATIVE  = "multipart/alternative";
	static constexpr KStringViewZ MULTIPART_MIXED        = "multipart/mixed";
	static constexpr KStringViewZ MULTIPART_RELATED      = "multipart/related";

	static constexpr KStringViewZ TEXT_PLAIN             = "text/plain";
	static constexpr KStringViewZ TEXT_UTF8              = "text/plain; charset=UTF-8";
	static constexpr KStringViewZ HTML_UTF8              = "text/html; charset=UTF-8";
	static constexpr KStringViewZ CSS                    = "text/css; charset=UTF-8";
	static constexpr KStringViewZ CSV                    = "text/csv";
	static constexpr KStringViewZ CALENDAR               = "text/calendar";

	static constexpr KStringViewZ AVI                    = "video/x-msvideo";
	static constexpr KStringViewZ MPEG                   = "video/mpeg";
	static constexpr KStringViewZ OGV                    = "video/ogg";
	static constexpr KStringViewZ WEBM                   = "video/webm";

//------
private:
//------

	KStringView m_mime{NONE};

}; // KMIME

//-----------------------------------------------------------------------------
constexpr
inline bool operator==(KStringView left, const KMIME& right)
//-----------------------------------------------------------------------------
{
	return left.operator==(right);
}

//-----------------------------------------------------------------------------
constexpr
inline bool operator==(const KMIME& left, KStringView right)
//-----------------------------------------------------------------------------
{
	return right.operator==(left);
}

//-----------------------------------------------------------------------------
constexpr
inline bool operator!=(KStringView left, const KMIME& right)
//-----------------------------------------------------------------------------
{
	return left.operator!=(right);
}

//-----------------------------------------------------------------------------
constexpr
inline bool operator!=(const KMIME& left, KStringView right)
//-----------------------------------------------------------------------------
{
	return right.operator!=(left);
}



//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// one part of a MIME structure
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

	bool Serialize(KString& sOut, const KReplacer& Replacer = KReplacer{}, uint16_t recursion = 0, bool bIsMultipartRelated = false) const;
	bool Serialize(KOutStream& Stream, const KReplacer& Replacer = KReplacer{}, uint16_t recursion = 0) const;
	KString Serialize(const KReplacer& Replacer = KReplacer{}, uint16_t recursion = 0) const;

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

	void MIME(KMIME _MIME) { m_MIME = _MIME; }

	KMIME   m_MIME;
	KString m_Data;
	KString m_sName;

	// boost::container allows use of incomplete types
	boost::container::vector<KMIMEPart> m_Parts;

}; // KMIMEPart

using KMIMEMultiPart = KMIMEPart;

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class KMIMEMultiPartMixed : public KMIMEMultiPart
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	KMIMEMultiPartMixed() : KMIMEPart(KMIME::MULTIPART_MIXED) {}

}; // KMIMEMultiPartMixed

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class KMIMEMultiPartRelated : public KMIMEMultiPart
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	KMIMEMultiPartRelated() : KMIMEPart(KMIME::MULTIPART_RELATED) {}

}; // KMIMEMultiPartRelated

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class KMIMEMultiPartAlternative : public KMIMEMultiPart
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	KMIMEMultiPartAlternative() : KMIMEPart(KMIME::MULTIPART_ALTERNATIVE) {}

}; // KMIMEMultiPartAlternative

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// a MIME part holding UTF8 plain text
class KMIMEText : public KMIMEPart
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	/// sMessage sets the initial plain text (UTF8) message for this part
	KMIMEText(KStringView sMessage = KStringView{}) : KMIMEPart(sMessage, KMIME::TEXT_UTF8) {}

}; // KMIMEText

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// a MIME part holding a UTF8 HTML message
class KMIMEHTML : public KMIMEPart
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	/// sMessage sets the initial HTML (UTF8) message for this part
	KMIMEHTML(KStringView sMessage = KStringView{}) : KMIMEPart(sMessage, KMIME::HTML_UTF8) {}

}; // KMIMEHTML

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// A MIME part holding a file
class KMIMEFile : public KMIMEPart
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	/// sFilename is loaded as data for this part. MIME type is automatically detected,
	/// or can be set explicitly through the MIME parameter
	KMIMEFile(KStringView sFilename, KMIME MIME = KMIME::NONE) : KMIMEPart(MIME) { File(sFilename); }

}; // KMIMEFile

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// A MIME part holding a file that will be displayed inline
class KMIMEFileInline : public KMIMEPart
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

	//----------
public:
	//----------

	/// sFilename is loaded as data for this part. MIME type is automatically detected,
	/// or can be set explicitly through the MIME parameter
	KMIMEFileInline(KStringView sFilename, KMIME MIME = KMIME::NONE) : KMIMEPart(MIME) { File(sFilename); m_sName.erase(); }
	/// the open stream is loaded as data for this part. MIME type has to be set manually.
	KMIMEFileInline(KInStream& stream, KMIME MIME) : KMIMEPart(MIME) { Stream(stream, KStringView{}); }

}; // KMIMEFile

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// KMIMEDirectory wraps all files in a directory into a multipart message. If
/// index.html and or index.txt exist, those will become the body part. All
/// additional files become part of a multipart/related structure, typically
/// used for a html message with inline images.
class KMIMEDirectory : public KMIMEMultiPart
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	/// KMIMEDirectory wraps all files in a directory into a multipart message. If
	/// index.html and or index.txt exist, those will become the body part. All
	/// additional files become part of a multipart/related structure, typically
	/// used for a html message with inline images.
	KMIMEDirectory(KStringViewZ sPathname);

}; // KMIMEDirectory

} // end of namespace dekaf2
