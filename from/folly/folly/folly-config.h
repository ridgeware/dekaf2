/*
 * Copyright 2016 Facebook, Inc.
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

#define FOLLY_FINAL final
#define FOLLY_OVERRIDE override

#define FOLLY_HAVE_PTHREAD 1
#define FOLLY_HAVE_PTHREAD_ATFORK 1

#define FOLLY_HAVE_LIBGFLAGS 1
#define FOLLY_UNUSUAL_GFLAGS_NAMESPACE 1
#define FOLLY_GFLAGS_NAMESPACE google

#define FOLLY_HAVE_CLOCK_GETTIME 1
#define FOLLY_HAVE_INT128_T 1
#define FOLLY_HAVE_INTTYPES_H 1
#define FOLLY_HAVE_STDINT_H 1
#define FOLLY_HAVE_STDLIB_H 1
#define FOLLY_HAVE_MEMORY_H 1
#define FOLLY_HAVE_STRINGS_H 1
#define FOLLY_HAVE_STRING_H 1
#define FOLLY_HAVE_SYS_STAT_H 1
#define FOLLY_HAVE_SYS_TYPES_H 1
#define FOLLY_HAVE_UNISTD_H 1
#define FOLLY_STDC_HEADERS 1
#define FOLLY_TIME_WITH_SYS_TIME 1

#ifdef __APPLE__
#define FOLLY_USE_LIBCPP 1
#else
#define FOLLY_HAVE_MALLOC_H 1
#define FOLLY_HAVE_MEMRCHR 1
#define FOLLY_HAVE_PREADV 1
#define FOLLY_HAVE_PWRITEV 1
#define FOLLY_HAVE_BITS_C__CONFIG_H 1
#define FOLLY_HAVE_BITS_FUNCTEXCEPT_H 1
#define FOLLY_HAVE_CPLUS_DEMANGLE_V3_CALLBACK 1
#define FOLLY_HAVE_DLFCN_H 1
#define FOLLY_HAVE_DWARF_H 1
#define FOLLY_HAVE_IFUNC 1
#define FOLLY_HAVE_PIPE2 1
#define FOLLY_HAVE_PTHREAD_SPINLOCK_T 1
#define FOLLY_HAVE_WEAK_SYMBOLS 1
#endif

#define FOLLY_HAVE_WCHAR_SUPPORT 1
#define FOLLY_LT_OBJDIR ".libs/"
#define FOLLY_HAVE_SCHED_H 1
#define FOLLY_HAVE_STD__IS_TRIVIALLY_COPYABLE 1
#define FOLLY_HAVE_STD_THIS_THREAD_SLEEP_FOR 1
#define FOLLY_HAVE_UNALIGNED_ACCESS 1
#define FOLLY_HAVE_VLA 1

#define FOLLY_VERSION "${PACKAGE_VERSION}"

#define FOLLY_PROVIDE_EXPONENTIAL_MALLOC_FALLBACK

//#define FOLLY_HAVE_LIBLZ4 1
//#define FOLLY_HAVE_LIBLZMA 1
//#define FOLLY_HAVE_LIBSNAPPY 1
//#define FOLLY_HAVE_LIBZ 1
//#define FOLLY_HAVE_LIBBZ2 1
//#define FOLLY_HAVE_LIBZSTD 1
