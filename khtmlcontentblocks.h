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

#include "khtmlparser.h"

namespace dekaf2 {

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class KHTMLContentBlocks : public KHTMLParser
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	class BlockContent
	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{

	//------
	public:
	//------

		using value_type = std::unique_ptr<KHTMLObject>;
		using ContentVector = std::vector<value_type>;
		using iterator = ContentVector::iterator;
		using const_iterator = ContentVector::const_iterator;
		using size_type = ContentVector::size_type;

		// data access

		void Serialize(KString& sOut) const;
		KString Serialize() const;
		bool HadTextContent() const { return m_bHadTextContent; }
		const_iterator begin() const { return m_Content.begin(); }
		const_iterator end() const { return m_Content.end(); }
		iterator begin() { return m_Content.begin(); }
		iterator end() { return m_Content.end(); }
		const KHTMLObject& operator[](size_type index) const { return *m_Content[index]; }
		KHTMLObject& operator[](size_type index) { return *m_Content[index]; }
		bool empty() const { return m_Content.empty(); }
		size_type size() const { return m_Content.size(); }

		// construction
		void Text(char ch);
		void NoText(char ch) { m_sContent += ch; }
		void InlineTag(const KHTMLTag& Tag);
		void Completed() { FlushText(); }
		void clear();

	//------
	protected:
	//------

		ContentVector m_Content;

	//------
	private:
	//------

		void FlushText();
		KString m_sContent;
		bool m_bHadTextContent { false };

	}; // BlockContent

//------
protected:
//------

	virtual void ContentBlock(BlockContent& Block);
	virtual void Skeleton(char ch);
	virtual void Skeleton(KStringView sSkeleton);
	virtual void Skeleton(const KHTMLObject& Object);

	virtual void Object(KHTMLObject& Object) override;
	virtual void Content(char ch) override;
	virtual void Script(char ch) override;
	virtual void Invalid(char ch) override;
	virtual void Finished() override;

	void FlushContentBlock();

//------
private:
//------

	BlockContent m_BlockContent;

}; // KHTMLContentBlocks


} // end of namespace dekaf2

