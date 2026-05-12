/*
 //
 // DEKAF(tm): Lighter, Faster, Smarter (tm)
 //
 // Copyright (c) 2025, Ridgeware, Inc.
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
 // |/|   OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR          |\|
 // |/|   OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE         |/|
 // |\|   SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.            |\|
 // |/|                                                                     |/|
 // |/+---------------------------------------------------------------------+/|
 // |\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/ |
 // +-------------------------------------------------------------------------+
 */

#pragma once

/// @file khtmlparse_accumulator.h
/// Three-mode string accumulator for the HTML parser's char-by-char
/// state machines. Picks the cheapest viable strategy for each
/// `m_sName` / `m_sValue` / sValue field:
///
///   * **Heap KString**  — when no arena was injected (SAX-style callers).
///   * **Arena Builder** — when an arena is set but the source bytes
///                         are not in a registered stable region.
///                         Chars accumulate into arena memory via
///                         `KArenaStringBuilder`.
///   * **Source Slice**  — when the arena has a stable region covering
///                         the source bytes AND every char accumulated
///                         so far is identical to its source byte (no
///                         lowercasing, no entity decoding). The final
///                         view is `KStringView(start, end-start)`
///                         pointing straight into the caller's buffer,
///                         zero arena bytes consumed.
///
/// The Source-Slice mode is "speculative": it starts on a fresh
/// accumulation, stays active as long as every byte is offered via
/// `AppendOriginal()`, and promotes itself to Arena-Builder mode the
/// first time `AppendTransformed()` is called (because at that point
/// the source bytes diverge from the parsed bytes, e.g. an uppercase
/// `ch != tolower(ch)` or an entity expansion).

#include <dekaf2/core/init/kdefinitions.h>
#include <dekaf2/core/strings/kstring.h>
#include <dekaf2/core/strings/kstringview.h>
#include <dekaf2/containers/memory/karenaallocator.h>
#include <dekaf2/web/html/bits/khtmldom_node.h>

DEKAF2_NAMESPACE_BEGIN

namespace khtml {

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class DEKAF2_PRIVATE ParseAccumulator
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	enum class Mode : std::uint8_t
	{
		HeapKString,    ///< owned KString fallback (no arena)
		ArenaBuilder,   ///< KArenaStringBuilder writing into arena
		SourceSlice     ///< KStringView straight into a stable source range
	};

	ParseAccumulator() = default;
	ParseAccumulator(const ParseAccumulator&)            = delete;
	ParseAccumulator& operator=(const ParseAccumulator&) = delete;
	// Move-ctor defaulted (KArenaStringBuilder has a move-ctor) so
	// owning containers (e.g. KHTML) can default their own move-ctor.
	// Move-assign deleted (KArenaStringBuilder deletes move-assign) —
	// owning containers must not provide move-assign either.
	ParseAccumulator(ParseAccumulator&&) noexcept = default;
	ParseAccumulator& operator=(ParseAccumulator&&)        = delete;

	//-----------------------------------------------------------------------------
	/// Arm the accumulator. Picks the best available mode:
	///   - no arena            -> HeapKString
	///   - arena, src stable   -> SourceSlice  (speculative)
	///   - arena, src unstable -> ArenaBuilder
	///
	/// @param Owned                target KString for heap-mode bytes
	/// @param View                 output reference written at Finalize()
	/// @param pArena               nullptr disables arena modes
	/// @param pCurrentSourcePos    cursor pointing at the next char to be
	///                             accumulated (slice start when in slicing mode)
	void Start(KString& Owned, KStringView& View,
	           Document* pArena, const char* pCurrentSourcePos) noexcept
	{
		m_pOwned   = &Owned;
		m_pView    = &View;
		m_pArena   = pArena;

		// Reset state before re-use
		m_pSliceStart = nullptr;
		m_pSliceEnd   = nullptr;

		if (pArena == nullptr)
		{
			m_eMode = Mode::HeapKString;
			m_pOwned->clear();
			return;
		}

		if (pCurrentSourcePos != nullptr
		    && pArena->IsInStableRegion(KStringView(pCurrentSourcePos,
		                                            std::size_t{0})))
		{
			m_eMode       = Mode::SourceSlice;
			m_pSliceStart = pCurrentSourcePos;
			m_pSliceEnd   = pCurrentSourcePos;   // empty slice initially
			return;
		}

		m_eMode = Mode::ArenaBuilder;
		// builder is lazy-armed on first append
	}
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Append one byte that is identical to its source counterpart. In
	/// SourceSlice mode this only advances the tracked slice-end (no
	/// bytes are written). In Arena/Heap modes the byte is copied
	/// through.
	///
	/// @param ch          the byte to store
	/// @param pAfterPos   source cursor positioned **immediately after**
	///                    this byte (i.e. `InStream.CurrentPos()` right
	///                    after reading it). Used to extend the slice
	///                    end in SourceSlice mode; ignored otherwise.
	void AppendOriginal(char ch, const char* pAfterPos) noexcept
	{
		switch (m_eMode)
		{
			case Mode::SourceSlice:
				// slice end advances to one past the byte just appended
				m_pSliceEnd = pAfterPos;
				break;

			case Mode::ArenaBuilder:
				if (!m_Builder.IsActive()) m_Builder.Start(*m_pArena);
				m_Builder.Append(ch);
				break;

			case Mode::HeapKString:
				m_pOwned->push_back(ch);
				break;
		}
	}

	/// Backwards-compatible overload — extends slice to the *currently
	/// known* end (no advance). Useful when the caller doesn't have a
	/// cursor handy (e.g. when re-appending a previously-buffered byte).
	void AppendOriginal(char ch)
	{
		if (m_eMode == Mode::SourceSlice)
		{
			// keep slice end as-is; the caller is replaying a byte that
			// was already "consumed" from the source point of view.
			return;
		}
		if (m_eMode == Mode::ArenaBuilder)
		{
			if (!m_Builder.IsActive()) m_Builder.Start(*m_pArena);
			m_Builder.Append(ch);
		}
		else
		{
			m_pOwned->push_back(ch);
		}
	}
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Append one byte that DIFFERS from the corresponding source byte
	/// (e.g. lowercased, entity-decoded). Forces a SourceSlice → ArenaBuilder
	/// promotion: bytes accumulated so far via the slice are copied into
	/// the builder before the new byte is appended.
	///
	/// @param ch                   the transformed byte to store
	/// @param pCurrentSourcePos    cursor positioned **just after** the
	///                             source byte that produced `ch` — the
	///                             slice-up-to-now ends here (exclusive)
	void AppendTransformed(char ch, const char* pCurrentSourcePos)
	{
		switch (m_eMode)
		{
			case Mode::SourceSlice:
			{
				// promote: copy the slice-so-far into the builder,
				// then add the transformed byte. m_pSliceEnd is the
				// authoritative end position; it was advanced on each
				// preceding AppendOriginal call.
				const char* pEnd = (m_pSliceEnd != nullptr)
				                     ? m_pSliceEnd
				                     : m_pSliceStart;
				const std::size_t iSoFar =
				    static_cast<std::size_t>(pEnd - m_pSliceStart);
				m_Builder.Start(*m_pArena);
				if (iSoFar > 0)
				{
					m_Builder.Append(KStringView(m_pSliceStart, iSoFar));
				}
				m_Builder.Append(ch);
				m_eMode = Mode::ArenaBuilder;
				(void) pCurrentSourcePos;  // no longer needed
				break;
			}

			case Mode::ArenaBuilder:
				if (!m_Builder.IsActive()) m_Builder.Start(*m_pArena);
				m_Builder.Append(ch);
				break;

			case Mode::HeapKString:
				m_pOwned->push_back(ch);
				break;
		}
	}
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Append a multi-byte sequence that diverges from the source
	/// (typically the UTF-8 result of an entity decode). Promotes
	/// SourceSlice → ArenaBuilder if active.
	void AppendTransformed(KStringView sBytes, const char* pCurrentSourcePos)
	{
		switch (m_eMode)
		{
			case Mode::SourceSlice:
			{
				const char* pEnd = (m_pSliceEnd != nullptr)
				                     ? m_pSliceEnd
				                     : m_pSliceStart;
				const std::size_t iSoFar =
				    static_cast<std::size_t>(pEnd - m_pSliceStart);
				m_Builder.Start(*m_pArena);
				if (iSoFar > 0)
				{
					m_Builder.Append(KStringView(m_pSliceStart, iSoFar));
				}
				m_Builder.Append(sBytes);
				m_eMode = Mode::ArenaBuilder;
				(void) pCurrentSourcePos;
				break;
			}

			case Mode::ArenaBuilder:
				if (!m_Builder.IsActive()) m_Builder.Start(*m_pArena);
				m_Builder.Append(sBytes);
				break;

			case Mode::HeapKString:
				m_pOwned->append(sBytes);
				break;
		}
	}
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Finalise the accumulation. Writes the output view to the slot
	/// supplied at `Start()`.
	///
	/// @param pCurrentSourcePos    cursor positioned **just after** the
	///                             last accumulated source byte — used as
	///                             the slice end in SourceSlice mode
	void Finalize(const char* /* legacy positional hint, ignored */ = nullptr)
	{
		switch (m_eMode)
		{
			case Mode::SourceSlice:
				if (m_pSliceStart != nullptr && m_pSliceEnd != nullptr
				    && m_pSliceEnd > m_pSliceStart)
				{
					*m_pView = KStringView(
					    m_pSliceStart,
					    static_cast<std::size_t>(m_pSliceEnd - m_pSliceStart));
				}
				else
				{
					*m_pView = KStringView{};
				}
				break;

			case Mode::ArenaBuilder:
				if (m_Builder.IsActive())
				{
					*m_pView = m_Builder.Finalize();
				}
				else
				{
					*m_pView = KStringView{};
				}
				break;

			case Mode::HeapKString:
				*m_pView = m_pOwned->ToView();
				break;
		}
	}
	//-----------------------------------------------------------------------------

	Mode CurrentMode() const noexcept { return m_eMode; }

//------
private:
//------

	Mode                 m_eMode       { Mode::HeapKString };
	KString*             m_pOwned      { nullptr };
	KStringView*         m_pView       { nullptr };
	Document*            m_pArena      { nullptr };
	KArenaStringBuilder  m_Builder     {};
	const char*          m_pSliceStart { nullptr };
	const char*          m_pSliceEnd   { nullptr };  ///< exclusive, advances per AppendOriginal

}; // ParseAccumulator

} // namespace khtml

DEKAF2_NAMESPACE_END
