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

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// refresh by running (and capturing):
//   ls -1 *.h | awk 'BEGIN {print "#include \"dekaf2.h\""} ($0 !~ /^dekaf2/) {printf ("#include \"%s\"\n", $0)}'
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
#include "dekaf2.h"
#include "kbase64.h"
#include "kcache.h"
#include "kcasestring.h"
#include "kcgi.h"
#include "kchunkedtransfer.h"
#include "kcompression.h"
#include "kconnection.h"
#include "kcrashexit.h"
#include "kencode.h"
#include "kfdstream.h"
#include "kfile.h"
#include "kformat.h"
#include "kgetruntimestack.h"
#include "khash.h"
#include "khtmlentities.h"
#include "khttp_header.h"
#include "khttp_method.h"
#include "khttp_request.h"
#include "khttp_response.h"
#include "khttpclient.h"
#include "kinpipe.h"
#include "kinshell.h"
#include "kjson.h"
#include "klog.h"
#include "kmime.h"
#include "kmru.h"
#include "koptions.h"
#include "koutpipe.h"
#include "koutshell.h"
#include "kparallel.h"
#include "kpipe.h"
#include "kprof.h"
#include "kprops.h"
#include "kreader.h"
#include "kregex.h"
#include "krow.h"
#include "ksharedref.h"
#include "ksignals.h"
#include "ksmtp.h"
#include "ksplit.h"
#include "ksql.h"
#include "ksslclient.h"
#include "ksslstream.h"
#include "kstack.h"
#include "kstream.h"
#include "kstreambuf.h"
#include "kstring.h"
#include "kstringstream.h"
#include "kstringutils.h"
#include "kstringview.h"
#include "ksubscribe.h"
#include "ksystem.h"
#include "ktcpclient.h"
#include "ktcpserver.h"
#include "ktcpstream.h"
#include "ktimer.h"
#include "kurl.h"
#include "kurlencode.h"
#include "kuseragent.h"
#include "kutf8.h"
#include "kwords.h"
#include "kwriter.h"

