/*
 * Copyright 2017 Facebook, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <cstdint>
#include <cstring>
#include <type_traits>

namespace folly {

namespace detail {

// A constexpr std::char_traits::length replacement for pre-C++17.
#if (__cplusplus >= 201402L)
// >= C++14
template <typename Char>
constexpr size_t constexpr_strlen_internal(const Char *s)
{
  const Char *start = s;
  while (*s) ++s;
  return s - start;
}
#else
// C++11
template <typename Char>
constexpr size_t constexpr_strlen_internal_1(const Char* s, size_t len)
{
  return *s == Char(0) ? len : constexpr_strlen_internal_1(s + 1, len + 1);
}

template <typename Char>
constexpr size_t constexpr_strlen_internal(const Char* s)
{
  return constexpr_strlen_internal_1(s, 0);
}
#endif

static_assert(
    constexpr_strlen_internal("123456789") == 9,
    "Someone appears to have broken constexpr_strlen...");

} // namespace detail

template <typename Char>
constexpr size_t constexpr_strlen(const Char* s) {
	return detail::constexpr_strlen_internal(s);
}

template <>
constexpr size_t constexpr_strlen(const char* s) {
#if defined(__clang__)
  return s ? __builtin_strlen(s) : 0;
#elif defined(_MSC_VER) || defined(__CUDACC__)
  return detail::constexpr_strlen_internal(s);
#elif defined(__GNUC__)
  #if (__GNUC__ > 7)
	return s ? std::char_traits<char>::length(s) : 0;
  #else
    return s ? detail::constexpr_strlen_internal(s) : 0;
  #endif
#else
  return std::strlen(s);
#endif
}

} // namespace folly
