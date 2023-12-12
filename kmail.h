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

#include "kassociative.h"
#include "kstring.h"
#include "kstringview.h"
#include "kurl.h"
#include "kmime.h"
#include "dekaf2.h"

/// @file kmail.h
/// Adds the KMail class to represent an email to be sent

DEKAF2_NAMESPACE_BEGIN

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// This class takes all information for an email message. It can then be used
/// as an argument for the KSMTP class, or sent via the convenience Send()
/// method of KMail, which internally calls KSMTP.
class DEKAF2_PUBLIC KMail
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	using map_t = std::map<KString, KString>;
	using self  = KMail;

	/// Add one recipient to the To list, first arg is the email,
	/// second arg is the full name or nothing
	self& To(KString sTo, KString sPretty = KString{});

	/// Add one recipient to the Cc list, first arg is the email,
	/// second arg is the full name or nothing
	self& Cc(KString sCc, KString sPretty = KString{});

	/// Add one recipient to the Bcc list, first arg is the email,
	/// second arg is the full name or nothing
	self& Bcc(KString sBcc, KString sPretty = KString{});

	/// Set the sender for the From field, first arg is the email,
	/// second arg is the full name or nothing
	self& From(KString sFrom, KString sPretty = KString{});

	/// Set the subject
	self& Subject(KString sSubject);

	/// Set the MIME type for the main content part to HTML/UTF-8. Returns false
	/// if content was added before.
	bool AsHTML();

	/// Set the text message (UTF-8, or HTML/UTF-8 if AsHTML() was called before)
	self& Message(KString sMessage);

	/// Returns true if this mail has all elements needed for expedition
	bool Good() const;

	/// Send the mail via MTA at URL. URL may contain a user's name and pass, or
	/// they can be set explicitly with sUsername / sPassword, which will override
	/// anything in the URL.
	bool Send(const KURL& URL, KStringView sUsername = KStringView{}, KStringView sPassword = KStringView{});

	/// Set the text message (UTF-8, or HTML/UTF-8 if AsHTML() was called before)
	self& operator=(KString sMessage)
	{
		Message(std::move(sMessage));
		return *this;
	}

	/// Append to text message (UTF-8, or HTML/UTF-8 if AsHTML() was called before)
	self& operator+=(KStringView sMessage);

	/// Set the mail body to a multipart structure (or to a single part). This voids
	/// any previously set content.
	self& Body(KMIMEMultiPart part);

	/// Set the mail body to a multipart structure (or to a single part). This voids
	/// any previously set content.
	self& operator=(KMIMEMultiPart part)
	{
		return Body(std::move(part));
	}

	/// Create mail body by reading the files in a directory or from a single file,
	/// creating either a multipart/related mail with inline images, or a
	/// multipart/mixed mail with attachments, or in case of a single file a mime
	/// type deduced from the file extension. This voids any previously set content.
	self& LoadBodyFrom(KStringViewZ sPath);

	/// Read a manifest.ini file or try to load the manifest from an index.html/.txt
	/// in the folder sPath, and set or add key/values to the Replacer. If sPath is a
	/// regular file, try to read the key/values from the head of it
	self& LoadManifestFrom(KStringViewZ sPath);

	/// Read manifest and body in one call
	self& LoadManifestAndBodyFrom(KStringViewZ sPath)
	{
		return LoadManifestFrom(sPath).LoadBodyFrom(sPath);
	}

	/// Read manifest and body in one call, compose path from sBasePath and sName
	self& LoadManifestAndBodyFrom(KStringView sBasePath, KStringView sName)
	{
		KString sPath { sBasePath };
		sPath += '/';
		sPath += sName;
		return LoadManifestAndBodyFrom(sPath);
	}

	/// Add a KReplacer to substitute text in all text/* parts of the mail
	self& VariableReplacer(std::shared_ptr<KReplacer> Replacer)
	{
		m_Replacer = std::move(Replacer);
		return *this;
	}

	/// Add a KReplacer to substitute text in all text/* parts of the mail
	self& VariableReplacer(KReplacer Replacer)
	{
		m_Replacer = std::make_shared<KReplacer>(std::move(Replacer));
		return *this;
	}

	/// Add a key/value pair to substitute text in all text/* parts of the mail
	self& AddReplaceVar(KStringView sKey, KStringView sValue);

	/// Attach a file, automatically creating a multipart structure if not yet
	/// set
	bool Attach(KStringViewZ sFilename, const KMIME& MIME = KMIME::BINARY);

	/// Attach KMIMEParts, automatically creating a multipart structure if not yet
	/// set
	self& Attach(KMIMEPart part);

	/// Attach KMIMEParts, automatically creating a multipart structure if not yet
	/// set
	self& operator+=(KMIMEPart part)
	{
		return Attach(std::move(part));
	}

	/// Returns the To recipients
	const map_t& To() const;

	/// Returns the Cc recipients
	const map_t& Cc() const;

	/// Returns the Bcc recipients
	const map_t& Bcc() const;

	/// Returns the sender (only first entry in map is valid)
	const map_t& From() const;

	/// Returns the subject
	const KString& Subject() const;

	/// Returns the message
	KString Serialize() const;

	/// Returns creation time
	KUnixTime Time() const;

	/// Returns last error
	const KString& Error() const;

	/// Returns last error
	const KString& GetLastError() const { return Error(); }

//----------
private:
//----------

	DEKAF2_PRIVATE self& Add(KStringView sWhich, map_t& map, KString Key, KString Value = KString{});

	map_t m_To;
	map_t m_Cc;
	map_t m_Bcc;
	map_t m_From; // actually we only need one single key and value for From
	KString m_Subject;

	std::shared_ptr<KReplacer> m_Replacer;
	mutable KString m_sError;
	KUnixTime m_Time { Dekaf::getInstance().GetCurrentTime() };
	mutable KMIMEMultiPart m_Parts;
	mutable size_t m_iBody { 0 };

}; // KMail

DEKAF2_NAMESPACE_END
