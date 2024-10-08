/*
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

/// @file kformat.h
/// provides basic string formatter functionality

#include "bits/kformat.h"
#include "kstringview.h"
#include "kstring.h"
#include <ostream>
#include <locale>
#include <cstdio>

#if DEKAF2_HAS_FMT_FORMAT
// fmt v10.0 doesn't support enum to int conversion anymore - add a generic conversion
template<typename Enum, typename std::enable_if<std::is_enum<Enum>::value, int>::type = 0>
constexpr typename std::underlying_type<Enum>::type format_as(Enum e)
{
	return std::to_underlying(e);
}
#else
// std::format doesn't support enum to int conversion - add a generic conversion
template<typename Enum>
requires std::is_enum_v<Enum>
struct DEKAF2_FORMAT_NAMESPACE::formatter<Enum> : formatter<typename std::underlying_type_t<Enum>>
{
	template <typename FormatContext>
	auto format(const Enum& e, FormatContext& ctx) const
	{
		return formatter<std::underlying_type_t<Enum>>::format(std::to_underlying(e), ctx);
	}
};
#endif

DEKAF2_NAMESPACE_BEGIN

#if DEKAF2_HAS_FORMAT_RUNTIME
/// Create a runtime format string
template<typename String>
inline DEKAF2_FORMAT_NAMESPACE::runtime_format_string<> KRuntimeFormat(const String& sFormat)
{
	return DEKAF2_FORMAT_NAMESPACE::runtime(sFormat);
}
#else
// this is a replacement for the missing runtime_format_string in std::format < C++26
struct KRuntimeFormatString
{
	constexpr explicit KRuntimeFormatString(KStringView sFormat) : m_sFormat(sFormat) {}
	constexpr KStringView get() const { return m_sFormat; }
private:
	KStringView m_sFormat;
};

// this is a replacement for the missing std::runtime() in std::format < C++26
/// Create a runtime format string - this alias is a replacement for std::runtime on format implementations that do not have it
template<typename String>
inline KRuntimeFormatString KRuntimeFormat(const String& sFormat)
{
	return KRuntimeFormatString(sFormat);
}
#endif

// basic output functions

/// write sOutput to stdout
DEKAF2_PUBLIC bool kWrite(KStringView sOutput) noexcept;
/// write sOutput to FILE*
DEKAF2_PUBLIC bool kWrite(std::FILE* fp, KStringView sOutput) noexcept;
/// write sOutput to fd
DEKAF2_PUBLIC bool kWrite(int fd, KStringView sOutput) noexcept;
/// write sOutput to std::ostream
DEKAF2_PUBLIC std::ostream& kWrite(std::ostream& os, KStringView sOutput) noexcept;
/// write sOutput to stdout
DEKAF2_PUBLIC bool kWriteLine(KStringView sOutput) noexcept;
/// write sOutput to FILE*
DEKAF2_PUBLIC bool kWriteLine(std::FILE* fp, KStringView sOutput) noexcept;
/// write sOutput to fd
DEKAF2_PUBLIC bool kWriteLine(int fd, KStringView sOutput) noexcept;
/// write sOutput to std::ostream
DEKAF2_PUBLIC std::ostream& kWriteLine(std::ostream& os, KStringView sOutput) noexcept;

#if !DEKAF2_FORMAT_INLINE || !DEKAF2_HAS_FORMAT_RUNTIME

namespace kformat_detail {

DEKAF2_NODISCARD DEKAF2_PUBLIC KString Format(DEKAF2_FORMAT_NAMESPACE::string_view sFormat, const DEKAF2_FORMAT_NAMESPACE::format_args& args) noexcept;
DEKAF2_NODISCARD DEKAF2_PUBLIC KString Format(const std::locale& locale, DEKAF2_FORMAT_NAMESPACE::string_view sFormat, const DEKAF2_FORMAT_NAMESPACE::format_args& args) noexcept;

} // end of namespace kformat_detail

#endif // of !DEKAF2_FORMAT_INLINE || !DEKAF2_HAS_FORMAT_RUNTIME

// ---------------------------- kFormat() -> KString ---------------------------- //

/// format no-op
DEKAF2_NODISCARD DEKAF2_PUBLIC inline
KString kFormat(KFormatString<> sFormat) noexcept
{
	return KString(sFormat.get().data(), sFormat.get().size());
}

/// formats a KString using Python syntax
template<class... Args, typename std::enable_if<sizeof...(Args) != 0, int>::type = 0>
DEKAF2_NODISCARD DEKAF2_PUBLIC
KString kFormat(KFormatString<Args...> sFormat, Args&&... args) noexcept
{
#if	DEKAF2_FORMAT_INLINE
	return DEKAF2_FORMAT_NAMESPACE::format(sFormat, std::forward<Args>(args)...);
#else
	return kformat_detail::Format(sFormat.get(), DEKAF2_FORMAT_NAMESPACE::make_format_args(args...));
#endif
}

/// formats a KString using Python syntax, using locale specification for decimal points and time formatting (month and day names)
template<class... Args>
DEKAF2_NODISCARD DEKAF2_PUBLIC
KString kFormat(const std::locale& locale, KFormatString<Args...> sFormat, Args&&... args) noexcept
{
#if	DEKAF2_FORMAT_INLINE
	return DEKAF2_FORMAT_NAMESPACE::format(locale, sFormat, std::forward<Args>(args)...);
#else
	return kformat_detail::Format(locale, sFormat.get(), DEKAF2_FORMAT_NAMESPACE::make_format_args(args...));
#endif
}

#if !DEKAF2_HAS_FORMAT_RUNTIME
/// format no-op
DEKAF2_NODISCARD inline DEKAF2_PUBLIC
KString kFormat(KRuntimeFormatString sFormat) noexcept
{
	return KString(sFormat.get());
}

/// formats a KString using Python syntax
template<class... Args, typename std::enable_if<sizeof...(Args) != 0, int>::type = 0>
DEKAF2_NODISCARD DEKAF2_PUBLIC
KString kFormat(KRuntimeFormatString sFormat, Args&&... args) noexcept
{
#if DEKAF2_FORMAT_INLINE
	return DEKAF2_FORMAT_NAMESPACE::vformat(sFormat.get(), DEKAF2_FORMAT_NAMESPACE::make_format_args(std::forward<Args>(args)...));
#else
	return kformat_detail::Format(sFormat.get(), DEKAF2_FORMAT_NAMESPACE::make_format_args(args...));
#endif
}

/// formats a KString using Python syntax
template<class... Args>
DEKAF2_NODISCARD DEKAF2_PUBLIC
KString kFormat(const std::locale& locale, KRuntimeFormatString sFormat, Args&&... args) noexcept
{
#if DEKAF2_FORMAT_INLINE
	return DEKAF2_FORMAT_NAMESPACE::vformat(locale, sFormat.get(), DEKAF2_FORMAT_NAMESPACE::make_format_args(std::forward<Args>(args)...));
#else
	return kformat_detail::Format(locale, sFormat.get(), DEKAF2_FORMAT_NAMESPACE::make_format_args(args...));
#endif
}
#endif // of !DEKAF2_HAS_FORMAT_RUNTIME

// ---------------------------- kPrint(/* stdout */) ---------------------------- //

/// format no-op to stdout
DEKAF2_PUBLIC inline bool kPrint(KFormatString<> sFormat) noexcept 
{
	return kWrite(KStringView(sFormat.get().data(), sFormat.get().size()));
}

/// formats to stdout using Python syntax
template<class... Args, typename std::enable_if<sizeof...(Args) != 0, int>::type = 0>
DEKAF2_PUBLIC bool kPrint(KFormatString<Args...> sFormat, Args&&... args) noexcept
{
	return kWrite(kFormat(sFormat, std::forward<Args>(args)...));
}

/// formats to stdout using Python syntax
template<class... Args>
DEKAF2_PUBLIC bool kPrint(const std::locale& locale, KFormatString<Args...> sFormat, Args&&... args) noexcept
{
	return kWrite(kFormat(locale, sFormat, std::forward<Args>(args)...));
}

/// format no-op to stdout
DEKAF2_PUBLIC inline bool kPrintLine(KFormatString<> sFormat) noexcept
{
	return kWriteLine(KStringView(sFormat.get().data(), sFormat.get().size()));
}

/// formats to stdout using Python syntax
template<class... Args, typename std::enable_if<sizeof...(Args) != 0, int>::type = 0>
DEKAF2_PUBLIC bool kPrintLine(KFormatString<Args...> sFormat, Args&&... args) noexcept
{
	return kWriteLine(kFormat(sFormat, std::forward<Args>(args)...));
}

/// formats to stdout using Python syntax
template<class... Args>
DEKAF2_PUBLIC bool kPrintLine(const std::locale& locale, KFormatString<Args...> sFormat, Args&&... args) noexcept
{
	return kWriteLine(kFormat(locale, sFormat, std::forward<Args>(args)...));
}

#if !DEKAF2_HAS_FORMAT_RUNTIME
/// format no-op to stdout
DEKAF2_PUBLIC inline bool kPrint(KRuntimeFormatString sFormat) noexcept
{
	return kWrite(sFormat.get());
}

/// formats to stdout using Python syntax
template<class... Args, typename std::enable_if<sizeof...(Args) != 0, int>::type = 0>
DEKAF2_PUBLIC bool kPrint(KRuntimeFormatString sFormat, Args&&... args) noexcept
{
	return kWrite(kFormat(sFormat, std::forward<Args>(args)...));
}

/// formats to stdout using Python syntax
template<class... Args>
DEKAF2_PUBLIC bool kPrint(const std::locale& locale, KRuntimeFormatString sFormat, Args&&... args) noexcept
{
	return kWrite(kFormat(locale, sFormat, std::forward<Args>(args)...));
}


/// format no-op to stdout, with newline
DEKAF2_PUBLIC inline bool kPrintLine(KRuntimeFormatString sFormat) noexcept
{
	return kWriteLine(sFormat.get());
}

/// formats to stdout using Python syntax
template<class... Args, typename std::enable_if<sizeof...(Args) != 0, int>::type = 0>
DEKAF2_PUBLIC bool kPrintLine(KRuntimeFormatString sFormat, Args&&... args) noexcept
{
	return kWriteLine(kFormat(sFormat, std::forward<Args>(args)...));
}

/// formats to stdout using Python syntax
template<class... Args>
DEKAF2_PUBLIC bool kPrintLine(const std::locale& locale, KRuntimeFormatString sFormat, Args&&... args) noexcept
{
	return kWriteLine(kFormat(locale, sFormat, std::forward<Args>(args)...));
}
#endif // of !DEKAF2_HAS_FORMAT_RUNTIME

// ---------------------------- kPrint(std::FILE*) ---------------------------- //

/// format no-op for std::FILE*
DEKAF2_PUBLIC inline bool kPrint(std::FILE* fp, KFormatString<> sFormat) noexcept
{
	return kWrite(fp, KStringView(sFormat.get().data(), sFormat.get().size()));
}

/// formats a std::FILE* using Python syntax
template<class... Args, typename std::enable_if<sizeof...(Args) != 0, int>::type = 0>
DEKAF2_PUBLIC bool kPrint(std::FILE* fp, KFormatString<Args...> sFormat, Args&&... args) noexcept
{
	return kWrite(fp, kFormat(sFormat, std::forward<Args>(args)...));
}

/// format no-op for std::FILE*
DEKAF2_PUBLIC inline bool kPrintLine(std::FILE* fp, KFormatString<> sFormat) noexcept
{
	return kWriteLine(fp, KStringView(sFormat.get().data(), sFormat.get().size()));
}

/// formats a std::FILE* using Python syntax
template<class... Args, typename std::enable_if<sizeof...(Args) != 0, int>::type = 0>
DEKAF2_PUBLIC bool kPrintLine(std::FILE* fp, KFormatString<Args...> sFormat, Args&&... args) noexcept
{
	return kWriteLine(fp, kFormat(sFormat, std::forward<Args>(args)...));
}

#if !DEKAF2_HAS_FORMAT_RUNTIME
/// format no-op for std::FILE*
DEKAF2_PUBLIC inline bool kPrint(std::FILE* fp, KRuntimeFormatString sFormat) noexcept
{
	return kWrite(fp, sFormat.get());
}

/// format no-op for std::FILE*
DEKAF2_PUBLIC inline bool kPrintLine(std::FILE* fp, KRuntimeFormatString sFormat) noexcept
{
	return kWriteLine(fp, sFormat.get());
}
#endif

// ---------------------------- kPrint(int fd) ---------------------------- //

/// format no-op for filedesc
DEKAF2_PUBLIC inline bool kPrint(int fd, KFormatString<> sFormat) noexcept
{
	return kWrite(fd, KStringView(sFormat.get().data(), sFormat.get().size()));
}

/// formats a filedesc using Python syntax
template<class... Args, typename std::enable_if<sizeof...(Args) != 0, int>::type = 0>
DEKAF2_PUBLIC bool kPrint(int fd, KFormatString<Args...> sFormat, Args&&... args) noexcept
{
	return kWrite(fd, kFormat(sFormat, std::forward<Args>(args)...));
}

/// format no-op for filedesc
DEKAF2_PUBLIC inline bool kPrintLine(int fd, KFormatString<> sFormat) noexcept
{
	return kWriteLine(fd, KStringView(sFormat.get().data(), sFormat.get().size()));
}

/// formats a filedesc using Python syntax
template<class... Args, typename std::enable_if<sizeof...(Args) != 0, int>::type = 0>
DEKAF2_PUBLIC bool kPrintLine(int fd, KFormatString<Args...> sFormat, Args&&... args) noexcept
{
	return kWriteLine(fd, kFormat(sFormat, std::forward<Args>(args)...));
}

#if !DEKAF2_HAS_FORMAT_RUNTIME
/// format no-op for filedesc
DEKAF2_PUBLIC inline bool kPrint(int fd, KRuntimeFormatString sFormat) noexcept
{
	return kWrite(fd, sFormat.get());
}

/// format no-op for filedesc
DEKAF2_PUBLIC inline bool kPrintLine(int fd, KRuntimeFormatString sFormat) noexcept
{
	return kWriteLine(fd, sFormat.get());
}

/// formats a filedesc using Python syntax
template<class... Args, typename std::enable_if<sizeof...(Args) != 0, int>::type = 0>
DEKAF2_PUBLIC bool kPrint(int fd, KRuntimeFormatString sFormat, Args&&... args) noexcept
{
	return kWrite(fd, kFormat(sFormat, std::forward<Args>(args)...));
}

/// formats a filedesc using Python syntax
template<class... Args, typename std::enable_if<sizeof...(Args) != 0, int>::type = 0>
DEKAF2_PUBLIC bool kPrintLine(int fd, KRuntimeFormatString sFormat, Args&&... args) noexcept
{
	return kWriteLine(fd, kFormat(sFormat, std::forward<Args>(args)...));
}
#endif // of !DEKAF2_HAS_FORMAT_RUNTIME

// ---------------------------- kPrint(std::ostream) ---------------------------- //

/// format no-op for std::ostream
DEKAF2_PUBLIC inline std::ostream& kPrint(std::ostream& os, KFormatString<> sFormat) noexcept
{
	return kWrite(os, KStringView(sFormat.get().data(), sFormat.get().size()));
}

/// formats a std::ostream using Python syntax
template<class... Args, typename std::enable_if<sizeof...(Args) != 0, int>::type = 0>
DEKAF2_PUBLIC std::ostream& kPrint(std::ostream& os, KFormatString<Args...> sFormat, Args&&... args) noexcept
{
	return kWrite(os, kFormat(sFormat, std::forward<Args>(args)...));
}

/// formats a std::ostream using Python syntax, with locale
template<class... Args>
DEKAF2_PUBLIC std::ostream& kPrint(const std::locale& locale, std::ostream& os, KFormatString<Args...> sFormat, Args&&... args) noexcept
{
	return kWrite(os, kFormat(locale, sFormat, std::forward<Args>(args)...));
}

/// format no-op for std::ostream
DEKAF2_PUBLIC inline std::ostream& kPrintLine(std::ostream& os, KFormatString<> sFormat) noexcept
{
	return kWriteLine(os, KStringView(sFormat.get().data(), sFormat.get().size()));
}

/// formats a std::ostream using Python syntax
template<class... Args, typename std::enable_if<sizeof...(Args) != 0, int>::type = 0>
DEKAF2_PUBLIC std::ostream& kPrintLine(std::ostream& os, KFormatString<Args...> sFormat, Args&&... args) noexcept
{
	return kWriteLine(os, kFormat(sFormat, std::forward<Args>(args)...));
}

/// formats a std::ostream using Python syntax, with locale
template<class... Args>
DEKAF2_PUBLIC std::ostream& kPrintLine(const std::locale& locale, std::ostream& os, KFormatString<Args...> sFormat, Args&&... args) noexcept
{
	return kWriteLine(os, kFormat(locale, sFormat, std::forward<Args>(args)...));
}

#if !DEKAF2_HAS_FORMAT_RUNTIME
/// format no-op for std::ostream
DEKAF2_PUBLIC inline std::ostream& kPrint(std::ostream& os, KRuntimeFormatString sFormat) noexcept
{
	return kWrite(os, sFormat.get());
}

/// formats a std::ostream using Python syntax
template<class... Args, typename std::enable_if<sizeof...(Args) != 0, int>::type = 0>
DEKAF2_PUBLIC std::ostream& kPrint(std::ostream& os, KRuntimeFormatString sFormat, Args&&... args) noexcept
{
	return kWrite(os, kFormat(sFormat, std::forward<Args>(args)...));
}

/// formats a std::ostream using Python syntax, with locale
template<class... Args>
DEKAF2_PUBLIC std::ostream& kPrint(const std::locale& locale, std::ostream& os, KRuntimeFormatString sFormat, Args&&... args) noexcept
{
	return kWrite(os, kFormat(locale, sFormat, std::forward<Args>(args)...));
}

/// format no-op for std::ostream
DEKAF2_PUBLIC inline std::ostream& kPrintLine(std::ostream& os, KRuntimeFormatString sFormat) noexcept
{
	return kWriteLine(os, sFormat.get());
}

/// formats a std::ostream using Python syntax
template<class... Args, typename std::enable_if<sizeof...(Args) != 0, int>::type = 0>
DEKAF2_PUBLIC std::ostream& kPrintLine(std::ostream& os, KRuntimeFormatString sFormat, Args&&... args) noexcept
{
	return kWriteLine(os, kFormat(sFormat, std::forward<Args>(args)...));
}

/// formats a std::ostream using Python syntax, with locale
template<class... Args>
DEKAF2_PUBLIC std::ostream& kPrintLine(const std::locale& locale, std::ostream& os, KRuntimeFormatString sFormat, Args&&... args) noexcept
{
	return kWriteLine(os, kFormat(locale, sFormat, std::forward<Args>(args)...));
}
#endif // of !DEKAF2_HAS_FORMAT_RUNTIME

DEKAF2_NAMESPACE_END
