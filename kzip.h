/*
//
// DEKAF(tm): Lighter, Faster, Smarter (tm)
//
// Copyright (c) 2020, Ridgeware, Inc.
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

#include <iterator>
#include "kstring.h"
#include "kexception.h"
#include "kwriter.h"
#include "kreader.h"

namespace dekaf2 {

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Wrapper class around libzip to give easy access from C++ to the files in
/// a zip archive or to create or append to one
class KZip
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	struct DirEntry
	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{
		KStringView sName;             ///< name of the file
		std::size_t iIndex;            ///< index within archive
		uint64_t    iSize;             ///< size of file (uncompressed)
		uint64_t    iCompSize;         ///< size of file (compressed)
		time_t      mtime;             ///< modification time
		uint32_t    iCRC;              ///< crc of file data
		uint16_t    iCompMethod;       ///< compression method used
		uint16_t    iEncryptionMethod; ///< encryption method used

		bool from_zip_stat(const void* vzip_stat);
	};

	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	class iterator
	{
	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

	//------
	public:
	//------

		using iterator_category = std::random_access_iterator_tag;
		using value_type        = const DirEntry;
		using pointer           = const value_type*;
		using reference         = const value_type&;
		using difference_type   = int64_t;
		using self_type         = iterator;

		iterator(KZip& Zip, uint64_t index = 0) noexcept
		: m_Zip(&Zip)
		, m_index(index)
		{
			if (index < m_Zip->size())
			{
				m_DirEntry = m_Zip->DirEntry(index);
			}
		}

		iterator() noexcept = default;

		reference operator*() const
		{
			if (m_index <= m_Zip->size())
			{
				return m_DirEntry;
			}
			else
			{
				throw KError("KZip::iterator out of range");
			}
		}

		pointer operator->() const
		{
			return & operator*();
		}

		// post increment
		iterator& operator++() noexcept
		{
			if (++m_index < m_Zip->size())
			{
				m_DirEntry = m_Zip->DirEntry(m_index);
			}
			return *this;
		}

		// pre increment
		iterator operator++(int) noexcept
		{
			iterator Copy = *this;
			if (++m_index < m_Zip->size())
			{
				m_DirEntry = m_Zip->DirEntry(m_index);
			}
			return Copy;
		}

		// post decrement
		iterator& operator--() noexcept
		{
			if (--m_index < m_Zip->size())
			{
				m_DirEntry = m_Zip->DirEntry(m_index);
			}
			return *this;
		}

		// pre decrement
		iterator operator--(int) noexcept
		{
			iterator Copy = *this;
			if (--m_index < m_Zip->size())
			{
				m_DirEntry = m_Zip->DirEntry(m_index);
			}
			return Copy;
		}

		// increment
		iterator operator+(difference_type iIncrement) const noexcept
		{
			return iterator(*m_Zip, m_index + iIncrement);
		}

		// decrement
		iterator operator-(difference_type iDecrement) const noexcept
		{
			return iterator(*m_Zip, m_index + iDecrement);
		}

		bool operator==(const iterator& other) const noexcept
		{
			return m_Zip == other.m_Zip && m_index == other.m_index;
		}

		bool operator!=(const iterator& other) const noexcept
		{
			return !operator==(other);
		}

		DirEntry operator[](difference_type iIndex)
		{
			return m_Zip->DirEntry(m_index + iIndex);
		}

	//------
	private:
	//------

		KZip* m_Zip;
		uint64_t m_index { 0 };
		mutable DirEntry m_DirEntry {};

	}; // iterator

	using const_iterator = iterator;

	KZip(bool bThrow = true)
	: m_bThrow(bThrow)
	{
	}

	KZip(KStringViewZ sFilename, bool bWrite = false, bool bThrow = true)
	{
		Open(sFilename, bWrite);
	}

	KZip(const KZip&) = delete;
	KZip(KZip&&) = default;
	KZip& operator=(const KZip&) = delete;
	KZip& operator=(KZip&&) = default;

	/// set password for encrypted archive entries
	KZip& SetPassword(KString sPassword)
	{
		m_sPassword = std::move(sPassword);
		return *this;
	}

	bool Open(KStringViewZ sFilename, bool bWrite = false);

	/// returns count of entries in zip
	std::size_t size() const noexcept;

	iterator begin() noexcept
	{
		return iterator(*this);
	}

	const_iterator begin() const noexcept
	{
		return const_iterator(const_cast<KZip&>(*this));
	}

	const_iterator cbegin() const noexcept
	{
		return const_iterator(const_cast<KZip&>(*this));
	}

	iterator end() noexcept
	{
		return iterator(*this, size());
	}

	const_iterator end() const noexcept
	{
		return const_iterator(const_cast<KZip&>(*this), size());
	}

	const_iterator cend() const noexcept
	{
		return const_iterator(const_cast<KZip&>(*this), size());
	}

	/// returns DirEntry at iIndex
	struct DirEntry DirEntry(std::size_t iIndex) const;

	/// returns DirEntry for file sName
	struct DirEntry DirEntry(KStringViewZ sName) const;

	/// returns DirEntry, alias of DirEntry()
	struct DirEntry Find(KStringViewZ sName) const
	{
		return DirEntry(sName);
	}

	/// returns true if file sName exists
	bool Contains(KStringViewZ sName) const noexcept;

	/// returns last error if class is not constructed to throw (default)
	const KString& Error() const
	{
		return m_sError;
	}

	bool Read(KOutStream& OutStream, const struct DirEntry& DirEntry);
	bool Read(KStringViewZ sFileName, const struct DirEntry& DirEntry);
	KString Read(const struct DirEntry& DirEntry);

//------
protected:
//------

//------
private:
//------

	bool SetError(KString sError) const;
	bool SetError(int iError) const;

	// helper types to allow for a unique_ptr<void>, which lets us hide all
	// implementation headers from the interface and nonetheless keep exception safety
	using deleter_t = std::function<void(void *)>;
	using unique_void_ptr = std::unique_ptr<void, deleter_t>;

	unique_void_ptr D;

	KString m_sPassword;
	mutable KString m_sError;
	bool m_bThrow { true };

}; // KZip

} // end of namespace dekaf2
