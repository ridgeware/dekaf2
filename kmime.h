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

#include "kstring.h"
#include "kstringview.h"
#include "kwriter.h"
#include "kreader.h"
#include "kreplacer.h"
#include "kerror.h"

#ifndef DEKAF2_HAS_CPP_17
	#include <boost/container/vector.hpp>
#else
	#include <vector>
#endif

DEKAF2_NAMESPACE_BEGIN

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// a MIME type
class DEKAF2_PUBLIC KMIME
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	//-----------------------------------------------------------------------------
	/// Construct an empty MIME type (NONE)
	KMIME() = default;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Construct a MIME type with an arbitrary type
	template<typename T,
			 typename std::enable_if<detail::is_kstringview_assignable<const T&, true>::value == true, int>::type = 0>
	DEKAF2_CONSTEXPR_20
	KMIME(T&& sMIME)
	//-----------------------------------------------------------------------------
		: m_mime(std::forward<T>(sMIME))
	{}

	//-------------------------------------------------------------------------
	template<typename T,
			 typename std::enable_if<detail::is_kstringview_assignable<const T&, true>::value == true, int>::type = 0>
	KMIME& operator=(T&& sv)
	//-------------------------------------------------------------------------
	{
		m_mime = std::forward<T>(sv);
		return *this;
	}

	//-----------------------------------------------------------------------------
	/// Set MIME type according to the extension of sFilename. Use Default if no
	/// association found.
	bool ByExtension(KStringView sFilename, KStringView Default = NONE);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Generate a KMIME instance with the MIME type set according to the extension
	/// of sFilename. Use Default if no association found.
	DEKAF2_NODISCARD
	static KMIME CreateByExtension(KStringView sFilename, KStringView Default = NONE);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Set MIME type according to inspection of sFilename. Use Default if no
	/// association found.
	bool ByInspection(KStringViewZ sFilename, KStringView Default = NONE);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Is this MIME type potentially compressible? (jpg, zip e.g. are not)
	DEKAF2_NODISCARD
	bool IsCompressible();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Generate a KMIME instance with the MIME type set according to inspection
	/// of sFilename. Use Default if no association found.
	DEKAF2_NODISCARD
	static KMIME CreateByInspection(KStringViewZ sFilename, KStringView Default = NONE);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// return the const KString& version of the MIME type
	DEKAF2_NODISCARD DEKAF2_CONSTEXPR_20
	const KString& Serialize() const
	//-----------------------------------------------------------------------------
	{
		return m_mime;
	}

	//-----------------------------------------------------------------------------
	/// return the KStringViewZ version of the MIME type
	DEKAF2_CONSTEXPR_20
	operator KStringViewZ() const
	//-----------------------------------------------------------------------------
	{
		return Serialize();
	}

	//-----------------------------------------------------------------------------
	/// empties the mime type
	void clear()
	//-----------------------------------------------------------------------------
	{
		m_mime.clear();
	}

	//-----------------------------------------------------------------------------
	/// returns true if mime is empty
	bool empty() const
	//-----------------------------------------------------------------------------
	{
		return m_mime.empty();
	}

	//-----------------------------------------------------------------------------
	/// returns the type part of a MIME type: type "/" [tree "."] subtype ["+" suffix]* [";" parameter]
	DEKAF2_NODISCARD
	KStringView Type()      const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// returns the tree part of a MIME type: type "/" [tree "."] subtype ["+" suffix]* [";" parameter]
	DEKAF2_NODISCARD
	KStringView Tree()      const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// returns the subtype part of a MIME type: type "/" [tree "."] subtype ["+" suffix]* [";" parameter]
	DEKAF2_NODISCARD
	KStringView SubType()   const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// returns the suffix part of a MIME type: type "/" [tree "."] subtype ["+" suffix]* [";" parameter]
	DEKAF2_NODISCARD
	KStringView Suffix()    const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// returns the parameter part of a MIME type: type "/" [tree "."] subtype ["+" suffix]* [";" parameter]
	DEKAF2_NODISCARD
	KStringView Parameter() const;
	//-----------------------------------------------------------------------------

	static constexpr KStringViewZ NONE                   = "";

	static constexpr KStringViewZ AAC                    = "audio/aac";
	static constexpr KStringViewZ MIDI                   = "audio/midi";
	static constexpr KStringViewZ OGA                    = "audio/ogg";
	static constexpr KStringViewZ WAV                    = "audio/wav";
	static constexpr KStringViewZ WEBA                   = "audio/webm";
	static constexpr KStringViewZ MP3                    = "audio/mpeg";

	static constexpr KStringViewZ BINARY                 = "application/octet-stream";
	static constexpr KStringViewZ BR                     = "application/brotli";
	static constexpr KStringViewZ JSON                   = "application/json";
	static constexpr KStringViewZ JSON_PATCH             = "application/json-patch+json";
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
	static constexpr KStringViewZ SEVENZIP               = "application/x-7z-compressed";
	static constexpr KStringViewZ SH                     = "application/x-sh";
	static constexpr KStringViewZ TAR                    = "application/x-tar";
	static constexpr KStringViewZ TYPESCRIPT             = "application/typescript";
	static constexpr KStringViewZ VSD                    = "application/vnd.visio";
	static constexpr KStringViewZ WASM                   = "application/wasm";
	static constexpr KStringViewZ XHTML                  = "application/xhtml+xml";
	static constexpr KStringViewZ XLS                    = "application/vnd.ms-excel";
	static constexpr KStringViewZ XLSX                   = "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet";
	static constexpr KStringViewZ ZIP                    = "application/zip";
	static constexpr KStringViewZ ZSTD                   = "application/zstd";

	static constexpr KStringViewZ EOT                    = "font/vnd.ms-fontobject";
	static constexpr KStringViewZ OTF                    = "font/otf";
	static constexpr KStringViewZ TTC                    = "font/collection";
	static constexpr KStringViewZ TTF                    = "font/ttf";
	static constexpr KStringViewZ WOFF                   = "font/woff";
	static constexpr KStringViewZ WOFF2                  = "font/woff2";

	static constexpr KStringViewZ BMP                    = "image/bmp";
	static constexpr KStringViewZ JPEG                   = "image/jpeg";
	static constexpr KStringViewZ JPEG2000               = "image/jp2";
	static constexpr KStringViewZ GIF                    = "image/gif";
	static constexpr KStringViewZ PNG                    = "image/png";
	static constexpr KStringViewZ ICON                   = "image/x-icon";
	static constexpr KStringViewZ SVG                    = "image/svg+xml";
	static constexpr KStringViewZ TIFF                   = "image/tiff";
	static constexpr KStringViewZ WEBP                   = "image/webp";

	static constexpr KStringViewZ MULTIPART_FORM_DATA    = "multipart/form-data";
	static constexpr KStringViewZ MULTIPART_ALTERNATIVE  = "multipart/alternative";
	static constexpr KStringViewZ MULTIPART_MIXED        = "multipart/mixed";
	static constexpr KStringViewZ MULTIPART_MIXED_REPLACE= "multipart/x-mixed-replace";
	static constexpr KStringViewZ MULTIPART_RELATED      = "multipart/related";

	static constexpr KStringViewZ TEXT_PLAIN             = "text/plain";
	static constexpr KStringViewZ TEXT_UTF8              = "text/plain; charset=UTF-8";
	static constexpr KStringViewZ HTML_UTF8              = "text/html; charset=UTF-8";
	static constexpr KStringViewZ H                      = "text/x-h";
	static constexpr KStringViewZ C                      = "text/x-c";
	static constexpr KStringViewZ CPP                    = "text/x-c++";
	static constexpr KStringViewZ CSS                    = "text/css; charset=UTF-8";
	static constexpr KStringViewZ CSV                    = "text/csv";
	static constexpr KStringViewZ TSV                    = "text/tab-separated-values";
	static constexpr KStringViewZ CALENDAR               = "text/calendar";
	static constexpr KStringViewZ PO                     = "text/x-gettext-translation";
	static constexpr KStringViewZ MD                     = "text/markdown";
	static constexpr KStringViewZ JAVASCRIPT             = "text/javascript";
	static constexpr KStringViewZ JAVA                   = "text/x-java";
	static constexpr KStringViewZ CSHARP                 = "text/x-csharp";
	static constexpr KStringViewZ GOLANG                 = "text/x-golang";
	static constexpr KStringViewZ PHP                    = "text/x-php";
	static constexpr KStringViewZ PYTHON                 = "text/x-python";
	static constexpr KStringViewZ RUBY                   = "text/x-ruby";
	static constexpr KStringViewZ TEX                    = "text/x-tex";

	static constexpr KStringViewZ AVI                    = "video/x-msvideo";
	static constexpr KStringViewZ MPEG                   = "video/mpeg";
	static constexpr KStringViewZ MP4                    = "video/mp4";
	static constexpr KStringViewZ OGV                    = "video/ogg";
	static constexpr KStringViewZ WEBM                   = "video/webm";

//------
private:
//------

	KStringView PastType() const;
	KStringView PastTree() const;

	KString m_mime { NONE };

}; // KMIME


//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// one part of a MIME structure
class DEKAF2_PUBLIC KMIMEPart
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

#ifndef DEKAF2_HAS_CPP_17
	// boost::container allows use of incomplete types
	using Storage = boost::container::vector<KMIMEPart>;
#else
	// std::vector beginning with C++17 allows incomplete types in the
	// template instantiation
	using Storage = std::vector<KMIMEPart>;
#endif
	using iterator = Storage::iterator;

	KMIMEPart(KMIME MIME = KMIME::NONE);
	KMIMEPart(KString sMessage, KMIME MIME);
	KMIMEPart(KString sControlName, KString sValue, KMIME MIME);
	KMIMEPart& operator=(KString str)  { m_Data = std::move(str); return *this; }
	KMIMEPart& operator+=(KStringView sv) { m_Data += sv; return *this; }
	/// Load file into MIME part. If MIME type is not already set it will be determined by the file extension.
	/// @param sControlName 'control' name for this MIME part. Can be used to distinguish multiple input file parts
	/// @param sFilename file to load
	/// @param sDispname name used for the attachment. If empty, basename of the filename will be used
	bool File(KStringView sControlName, KStringViewZ sFilename, KStringView sDispname = KStringView{});
	/// Add content of Stream to MIME part
	/// @param sControlName 'control' name for this MIME part. Can be used to distinguish multiple input stream parts
	/// @param Stream stream to read from
	/// @param sDispName name used for the attachment. By default, 'untitled' will be used
	bool Stream(KStringView sControlName, KInStream& Stream, KStringView sDispname = "untitled");

	/// Is this a multipart structure?
	bool IsMultiPart() const;
	/// Is this a binary content?
	bool IsBinary() const;

	/// Attach another part to this multipart structure - returns false if this->MIME type is not multipart
	/// @param part the part to attach
	bool Attach(KMIMEPart part);
	/// Attach another part to this multipart structure - fails if this->MIME type is not multipart
	KMIMEPart& operator+=(KMIMEPart part) { Attach(std::move(part)); return *this; }

	bool Serialize(KStringRef& sOut, bool bForHTTP = false, const KReplacer& Replacer = KReplacer{}, uint16_t recursion = 0, const KMIME& ParentMIME = KMIME::NONE) const;
	bool Serialize(KOutStream& Stream, bool bForHTTP = false, const KReplacer& Replacer = KReplacer{}, uint16_t recursion = 0) const;
	DEKAF2_NODISCARD
	KString Serialize(bool bForHTTP = false, const KReplacer& Replacer = KReplacer{}, uint16_t recursion = 0) const;

	/// is this part empty?
	bool empty() const { return m_Parts.empty(); }
	/// how many (multi) parts does this struct contain
	DEKAF2_NODISCARD
	Storage::size_type size() const { return m_Parts.size(); }
	/// return iterator to the begin of the struct of parts
	DEKAF2_NODISCARD
	iterator begin() { return m_Parts.begin(); }
	/// return iterator to the end of the struct of parts
	DEKAF2_NODISCARD
	iterator end() { return m_Parts.end(); }
	DEKAF2_NODISCARD
	KMIMEPart& operator[](size_t pos) { return m_Parts[pos]; }
	DEKAF2_NODISCARD
	const KMIMEPart& operator[](size_t pos) const { return m_Parts[pos]; }

	/// return the MIME type of this part
	DEKAF2_NODISCARD
	KMIME MIME() const { return m_MIME; }
	/// return a reference on the content of this part
	DEKAF2_NODISCARD
	const KString& Data() const { return m_Data; }
	/// return a reference on the 'control' name of this part
	DEKAF2_NODISCARD
	const KString& ControlName() const { return m_sControlName; }
	/// return a reference on the file name of this part (if any)
	DEKAF2_NODISCARD
	const KString& FileName() const { return m_sFileName; }
	/// return a content type string with boundary if needed
	DEKAF2_NODISCARD
	KMIME ContentType() const;

//----------
protected:
//----------

	void SetMIME(KMIME _MIME) { m_MIME = std::move(_MIME); }

	KMIME   m_MIME;
	KString m_Data;
	KString m_sControlName;
	KString m_sFileName;

//----------
private:
//----------

	/// creates a multipart boundary string for recursion level iLevel of a multipart
	/// structure using the m_sRandom generated by CreateBoundaryRandom()
	/// earlier
	/// @param iLevel recursion level in the multipart structure. Start with 1 and
	/// increase for nested multipart structures
	/// @returns a multipart boundary string
	KString GetMultiPartBoundary(std::size_t iLevel) const;
	/// creates a strong random string, typically a UUID, and stores it in m_sRandom
	const KString& GetBoundaryRandom() const;

	mutable
	KString m_sRandom;
	Storage m_Parts;

}; // KMIMEPart

using KMIMEMultiPart = KMIMEPart;

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class DEKAF2_PUBLIC KMIMEMultiPartFormData : public KMIMEMultiPart
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	KMIMEMultiPartFormData() : KMIMEMultiPart(KMIME::MULTIPART_FORM_DATA) {}

}; // KMIMEMultiPartFormData

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class DEKAF2_PUBLIC KMIMEMultiPartMixed : public KMIMEMultiPart
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	KMIMEMultiPartMixed() : KMIMEMultiPart(KMIME::MULTIPART_MIXED) {}

}; // KMIMEMultiPartMixed

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class DEKAF2_PUBLIC KMIMEMultiPartRelated : public KMIMEMultiPart
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	KMIMEMultiPartRelated() : KMIMEMultiPart(KMIME::MULTIPART_RELATED) {}

}; // KMIMEMultiPartRelated

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class DEKAF2_PUBLIC KMIMEMultiPartAlternative : public KMIMEMultiPart
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	KMIMEMultiPartAlternative() : KMIMEMultiPart(KMIME::MULTIPART_ALTERNATIVE) {}

}; // KMIMEMultiPartAlternative

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// a MIME part holding UTF8 plain text
class DEKAF2_PUBLIC KMIMEText : public KMIMEPart
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	/// @param sMessage sets the initial plain text (UTF8) message for this part
	KMIMEText(KStringView sMessage = KStringView{}) : KMIMEPart(sMessage, KMIME::TEXT_UTF8) {}
	/// Create a name / value pair
	/// @param sControlName sets the name of this control when used in a multipart message
	/// @param sMessage sets the initial plain text (UTF8) message for this part
	KMIMEText(KStringView sControlName, KStringView sMessage) : KMIMEPart(sControlName, sMessage, KMIME::TEXT_UTF8) {}

}; // KMIMEText

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// a MIME part holding a UTF8 HTML message
class DEKAF2_PUBLIC KMIMEHTML : public KMIMEPart
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	/// sMessage sets the initial HTML (UTF8) message for this part
	KMIMEHTML(KStringView sMessage = KStringView{}) : KMIMEPart(sMessage, KMIME::HTML_UTF8) {}
	/// Create a name / value pair
	/// @param sControlName sets the name of this control when used in a multipart message
	/// @param sMessage sets the initial plain text (UTF8) message for this part
	KMIMEHTML(KStringView sControlName, KStringView sMessage) : KMIMEPart(sControlName, sMessage, KMIME::HTML_UTF8) {}

}; // KMIMEHTML

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// A MIME part holding a file
class DEKAF2_PUBLIC KMIMEFile : public KMIMEPart
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	/// sFilename is loaded as data for this part. MIME type is automatically detected,
	/// or can be set explicitly through the MIME parameter
	template <typename KMime = KMIME, typename std::enable_if<std::is_same<KMIME, KMime>::value, int>::type = 0>
	KMIMEFile(KStringView sControlName, KStringViewZ sFilename, KMime MIME = KMIME::NONE) : KMIMEPart(std::move(MIME)) { File(sControlName, sFilename); }
	/// set a KMIMEFile from sData, with sDispname and MIME MIME type
	KMIMEFile(KStringView sControlName, KStringView sData, KStringView sDispname, KMIME MIME = KMIME::NONE);

}; // KMIMEFile

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// A MIME part holding a file that will be displayed inline
class DEKAF2_PUBLIC KMIMEFileInline : public KMIMEPart
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	/// sFilename is loaded as data for this part. MIME type is automatically detected,
	/// or can be set explicitly through the MIME parameter
	KMIMEFileInline(KStringViewZ sFilename, KMIME MIME = KMIME::NONE) : KMIMEPart(std::move(MIME)) { File("", sFilename); m_sFileName.clear(); }
	/// the open stream is loaded as data for this part. MIME type has to be set manually.
	KMIMEFileInline(KInStream& stream, KMIME MIME) : KMIMEPart(std::move(MIME)) { Stream("", stream, KStringView{}); }

}; // KMIMEFile

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// KMIMEDirectory wraps all files in a directory into a multipart message. If
/// index.html and or index.txt exist, those will become the body part. All
/// additional files become part of a multipart/related structure, typically
/// used for a html message with inline images.
class DEKAF2_PUBLIC KMIMEDirectory : public KMIMEMultiPart
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

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// the opposite to KMIMEDirectory: receive a data stream, split it into files that are stored in a given directory
class DEKAF2_PUBLIC KMIMEReceiveMultiPartFormData : public KErrorBase
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	class File
	//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{
		friend class KMIMEReceiveMultiPartFormData;

	//----------
	public:
	//----------

		File(KStringView sFilename, KMIME Mime);

		const KString& GetFilename     () const { return m_sFilename;     }
		const KString& GetOrigFilename () const ;
		KMIME          GetMIME         () const { return m_Mime;          }
		/// has this file been received successfully?
		bool           GetCompleted    () const { return m_bCompleted;    }
		/// returned size will be npos for inexisting file, 0 for empty file
		std::size_t    GetReceivedSize () const { return m_iReceivedSize; }

	//----------
	protected:
	//----------

		void           SetCompleted    ()                  { m_bCompleted    = true;  }
		void           SetReceivedSize (std::size_t iSize) { m_iReceivedSize = iSize; }

	//----------
	private:
	//----------

		KString     m_sFilename;
		KString     m_sOrigFilename;
		std::size_t m_iReceivedSize  { 0 };
		KMIME       m_Mime { KMIME::NONE };
		bool        m_bCompleted { false };
	};

	/// set output directory and boundary string
	KMIMEReceiveMultiPartFormData(KStringView sOutputDirectory, KStringView sFormBoundary = KStringView{});
	/// receive from InStream
	bool ReadFromStream(KInStream& InStream);
	/// returns received files
	const std::vector<File>& GetFiles() const { return m_Files; }

//----------
private:
//----------

	bool WriteToFile(KOutFile& OutFile, KStringView sData, File& file, KStringViewZ sOutFile);

	KStringView       m_sOutputDirectory;
	KString           m_sFormBoundary;
	std::vector<File> m_Files;

}; // KReceiveFormData

DEKAF2_NAMESPACE_END

namespace DEKAF2_FORMAT_NAMESPACE
{

template <>
struct formatter<DEKAF2_PREFIX KMIME> : formatter<string_view>
{
	template <typename FormatContext>
	auto format(const DEKAF2_PREFIX KMIME& MIME, FormatContext& ctx) const
	{
		return formatter<string_view>::format(MIME.Serialize(), ctx);
	}
};

} // end of namespace DEKAF2_FORMAT_NAMESPACE
