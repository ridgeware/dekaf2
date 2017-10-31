/*
//
// DEKAF(tm): Lighter, Faster, Smarter(tm)
//
// Copyright (c) 2000-2017, Ridgeware, Inc.
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

#include "kconfiguration.h"

#ifdef DEKAF1_INCLUDE_PATH

#include "../kstack.h"
#include "../ksplit.h"

namespace dekaf2 {
namespace compat {

// adaptors for KStack

template<class Stack_Type>
class KStack : public dekaf2::KStack<Stack_Type>
{
public:
	using base_type = dekaf2::KStack<Stack_Type>;

	const Stack_Type& Get(size_t index) const
	{
		return base_type::GetItem(index);
	}
	bool Get(size_t index, Stack_Type& item) const
	{
		return base_type::GetItem(index, item);
	}
	bool Set(size_t index, Stack_Type& item)
	{
		return base_type::SetItem(index, item);
	}
	size_t Size() const
	{
		return base_type::size();
	}
	void TruncateList()
	{
		base_type::clear();
	}
};

} // of namespace compat
} // of namespace dekaf2

//-----------------------------------------------------------------------------
template <class String, class Stack>
unsigned int kParseDelimedList (const String& sBuffer, Stack& List, int chDelim=',', bool bTrim=true, bool bCombineDelimiters=false, int chEscape='\0')
//inline unsigned int kParseDelimedList (const dekaf2::compat::KString& sBuffer, MyKStack& List, int chDelim=',', bool bTrim=true, bool bCombineDelimiters=false, int chEscape='\0')
//-----------------------------------------------------------------------------
{
	char delim(chDelim);
	return dekaf2::kSplit(List,
	                      sBuffer,
	                      dekaf2::KStringView(&delim, 1),
	                      bTrim ? " \t\r\n" : "",
	                      chEscape);
}

#endif // of DEKAF1_INCLUDE_PATH
