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

#include "kmime.h"
#include "kbase64.h"
#include "kquotedprintable.h"
#include "kfile.h"
#include "klog.h"

namespace dekaf2 {

//-----------------------------------------------------------------------------
bool KMIMEPart::IsMultiPart() const
//-----------------------------------------------------------------------------
{
	return KStringView(m_MIME).StartsWith("multipart/");

} // IsMultiPart

//-----------------------------------------------------------------------------
bool KMIMEPart::IsBinary() const
//-----------------------------------------------------------------------------
{
	return KStringView(m_MIME).StartsWith("application/");

} // IsBinary

//-----------------------------------------------------------------------------
bool KMIMEPart::Serialize(KString& sOut, uint16_t recursion) const
//-----------------------------------------------------------------------------
{
	if (!IsMultiPart())
	{
		if (!m_Data.empty())
		{
			sOut += "Content-Type: ";
			sOut += m_MIME;
			sOut += "\r\nContent-Transfer-Encoding: ";
			if (IsBinary())
			{
				sOut += "base64\r\n";
			}
			else
			{
				sOut += "quoted-printable\r\n";
			}

			sOut += "Content-Disposition: ";
			if (!m_sName.empty())
			{
				if (m_MIME == KMIME::MULTIPART_FORM_DATA)
				{
					sOut += "form-data; name=\"";
				}
				else
				{
					sOut += "attachment; filename=\"";
				}
				sOut += m_sName;
				sOut += "\"\r\n";
			}
			else
			{
				sOut += "inline\r\n";
			}

			sOut += "\r\n"; // End of headers

			if (IsBinary())
			{
				sOut += KBase64::Encode(m_Data);
			}
			else
			{
				sOut += KQuotedPrintable::Encode(m_Data, true);
			}

			return true;
		}
	}
	else if (m_Parts.empty())
	{
		return false;
	}
	else if (m_Parts.size() == 1)
	{
		// serialize the single part directly, do not embed it into a multipart structure
		return m_Parts.front().Serialize(sOut, recursion);
	}
	else
	{
		++recursion;

		sOut += "Content-Type: ";
		sOut += m_MIME;
		KString sBoundary;
		sBoundary.Format("----=_KMIME_Part_{}_{}.{}----", recursion, random(), random());
		sOut += "; boundary=\"";
		sOut += sBoundary;
		sOut += "\"\r\n"; // End of headers (see the next \r\n at the begin of the boundaries)

		for (auto& it : m_Parts)
		{
			sOut += "\r\n--";
			sOut += sBoundary;
			sOut += "\r\n";
			it.Serialize(sOut, recursion);
		}

		sOut += "\r\n--";
		sOut += sBoundary;
		sOut += "--\r\n";

		return true;
	}

	return false;

} // Serialize

//-----------------------------------------------------------------------------
bool KMIMEPart::Serialize(KOutStream& Stream, uint16_t recursion) const
//-----------------------------------------------------------------------------
{
	KString sOut;
	if (Serialize(sOut, recursion))
	{
		return Stream.Write(sOut).Good();
	}
	else
	{
		return false;
	}

} // Serialize

//-----------------------------------------------------------------------------
KString KMIMEPart::Serialize(uint16_t recursion) const
//-----------------------------------------------------------------------------
{
	KString sOut;
	Serialize(sOut, recursion);
	return sOut;

} // Serialize

//-----------------------------------------------------------------------------
bool KMIMEPart::Attach(KMIMEPart&& part)
//-----------------------------------------------------------------------------
{
	if (IsMultiPart())
	{
		m_Parts.push_back(std::move(part));
		return true;
	}
	else
	{
		kDebug(1, "cannot attach to non-multipart KMIMEPart");
		return false;
	}

} // Attach

//-----------------------------------------------------------------------------
bool KMIMEPart::Stream(KInStream& Stream, KStringView sDispname)
//-----------------------------------------------------------------------------
{
	if (Stream.ReadAll(m_Data))
	{
		m_sName = sDispname;
		return true;
	}
	else
	{
		return false;
	}

} // Stream

//-----------------------------------------------------------------------------
bool KMIMEPart::File(KStringView sFilename, KStringView sDispname)
//-----------------------------------------------------------------------------
{
	KInFile File(sFilename);
	if (File.is_open())
	{
		if (sDispname.empty())
		{
			sDispname = kBasename(sFilename);
		}
		if (Stream(File, sDispname))
		{
			return true;
		}
	}
	return false;

} // File


#ifdef DEKAF2_REPEAT_CONSTEXPR_VARIABLE

constexpr KStringViewZ KCharSet::ANY_ISO8859;
constexpr KStringViewZ KCharSet::DEFAULT_CHARSET;

constexpr KStringViewZ KMIME::NONE;
constexpr KStringViewZ KMIME::TEXT_PLAIN;
constexpr KStringViewZ KMIME::TEXT_UTF8;
constexpr KStringViewZ KMIME::JSON_UTF8;
constexpr KStringViewZ KMIME::HTML_UTF8;
constexpr KStringViewZ KMIME::XML_UTF8;
constexpr KStringViewZ KMIME::SWF;
constexpr KStringViewZ KMIME::WWW_FORM_URLENCODED;
constexpr KStringViewZ KMIME::MULTIPART_FORM_DATA;
constexpr KStringViewZ KMIME::MULTIPART_ALTERNATIVE;
constexpr KStringViewZ KMIME::MULTIPART_MIXED;
constexpr KStringViewZ KMIME::MULTIPART_RELATED;
constexpr KStringViewZ KMIME::APPLICATION_BINARY;
constexpr KStringViewZ KMIME::APPLICATION_JAVASCRIPT;
constexpr KStringViewZ KMIME::IMAGE_JPEG;

#endif

} // end of namespace dekaf2
