/*
//
// DEKAF(tm): Lighter, Faster, Smarter(tm)
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
//
*/

#pragma once

/// @file dekaf2all.h
/// globbed include of all dekaf2 header files

#include <dekaf2/dekaf2.h>
#include <dekaf2/kcache.h>
#include <dekaf2/kcasestring.h>
#include <dekaf2/kconnection.h>
#include <dekaf2/kcrashexit.h>
#include <dekaf2/kcurl.h>
#include <dekaf2/kfdstream.h>
#include <dekaf2/kfile.h>
#include <dekaf2/kformat.h>
#include <dekaf2/kgetruntimestack.h>
#include <dekaf2/khash.h>
#include <dekaf2/khttp.h>
#include <dekaf2/khttp_header.h>
#include <dekaf2/khttp_method.h>
#include <dekaf2/kinpipe.h>
#include <dekaf2/kinshell.h>
#include <dekaf2/klog.h>
#include <dekaf2/kmru.h>
#include <dekaf2/kostringstream.h>
#include <dekaf2/koutpipe.h>
#include <dekaf2/koutshell.h>
#include <dekaf2/kparallel.h>
#include <dekaf2/kpipe.h>
#include <dekaf2/kprof.h>
#include <dekaf2/kprops.h>
#include <dekaf2/kreader.h>
#include <dekaf2/kregex.h>
#include <dekaf2/ksharedref.h>
#include <dekaf2/ksignals.h>
#include <dekaf2/ksplit.h>
#include <dekaf2/ksslclient.h>
#include <dekaf2/ksslstream.h>
#include <dekaf2/kstack.h>
#include <dekaf2/kstream.h>
#include <dekaf2/kstreambuf.h>
#include <dekaf2/kstring.h>
#include <dekaf2/kstringutils.h>
#include <dekaf2/kstringview.h>
#include <dekaf2/ksubscribe.h>
#include <dekaf2/ksystem.h>
#include <dekaf2/ktcpclient.h>
#include <dekaf2/ktcpserver.h>
#include <dekaf2/kurl.h>
#include <dekaf2/kurlencode.h>
#include <dekaf2/kuseragent.h>
#include <dekaf2/kwebio.h>
#include <dekaf2/kwriter.h>
