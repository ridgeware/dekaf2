/*
// DEKAF(tm): Lighter, Faster, Smarter (tm)
//
// Copyright (c) 2021, Ridgeware, Inc.
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

/// @file ksnippets.h
/// adds a class that outputs and replaces text sections

#include "kstringview.h"
#include "kstring.h"
#include "koutstringstream.h"
#include "kstream.h"
#include "kreplacer.h"
#include "klog.h"
#include <unordered_map>
#include <forward_list>

namespace dekaf2 {

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Replace variables with values
class DEKAF2_PUBLIC KSnippets
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	KSnippets() = default;
	KSnippets(const KSnippets&) = delete;
	KSnippets(KSnippets&&) = default;
	KSnippets& operator=(const KSnippets&) = delete;
	KSnippets& operator=(KSnippets&&) = default;

	KSnippets(KInStream& InStream, KStringView sLeadIn = {"{{"}, KStringView sLeadOut = {"}}"});
	KSnippets(KString sInput, KStringView sLeadIn = {"{{"}, KStringView sLeadOut = {"}}"});

	bool Add(KSnippets&& other);
	bool Add(KInStream& InStream,         KStringView sLeadIn = {"{{"}, KStringView sLeadOut = {"}}"});
	bool Add(KString sInput,              KStringView sLeadIn = {"{{"}, KStringView sLeadOut = {"}}"});
	bool AddFile(KStringViewZ sInputFile, KStringView sLeadIn = {"{{"}, KStringView sLeadOut = {"}}"});

	void clear();

	void SetOutput(KOutStream& OutStream);
	void SetOutput(KString& sOutput);

	KSnippets& operator+=(KSnippets&& other)   { Add(std::move(other));  return *this; }
	KSnippets& operator+=(KString sInput)      { Add(std::move(sInput)); return *this; }
	KSnippets& operator+=(KInStream& InStream) { Add(InStream);          return *this; }

	bool Compose(KStringView sSnippet, const KReplacer& Replacer = KReplacer{});
	bool operator()(KStringView sSnippet, const KReplacer& Replacer = KReplacer{})
	{
		return Compose(sSnippet, Replacer);
	}

//----------
private:
//----------

	DEKAF2_PRIVATE
	void IsolateSnippets(KStringView sBuffer, KStringView sLeadIn, KStringView sLeadOut);

	std::forward_list<KString> m_Buffer;
	std::unordered_map<KStringView, KStringView> m_Snippets;
	std::unique_ptr<KOutStringStream> m_OSS;
	KOutStream* m_OutStream { nullptr };

}; // KSnippets

} // of namespace dekaf2

