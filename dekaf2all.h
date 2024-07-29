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
#include "kaddrplus.h"
#include "kallocator.h"
#include "kassociative.h"
#include "katomic_object.h"
#include "kbar.h"
#include "kbase64.h"
#include "kbitfields.h"
#include "kbufferedreader.h"
#include "kcache.h"
#include "kcaseless.h"
#include "kcasestring.h"
#include "kcgistream.h"
#include "kchildprocess.h"
#include "kchunkedtransfer.h"
#include "kcompression.h"
#include "kcookie.h"
#include "kcountingstreambuf.h"
#include "kcrashexit.h"
#include "kcrc.h"
#include "kcsv.h"
#include "kctype.h"
#include "kdefinitions.h"
#include "kdiff.h"
#include "kduration.h"
#include "kencode.h"
#include "kexception.h"
#include "kfdstream.h"
#include "kfileserver.h"
#include "kfilesystem.h"
#include "kformat.h"
#include "kfrozen.h"
#include "kgetruntimestack.h"
#include "khash.h"
#include "kheapmon.h"
#include "khmac.h"
#include "khtmlcontentblocks.h"
#include "khtmldom.h"
#include "khtmlentities.h"
#include "khtmlparser.h"
#include "khttp_header.h"
#include "khttp_method.h"
#include "khttp_request.h"
#include "khttp_response.h"
#include "khttpclient.h"
#include "khttpcompression.h"
#include "khttperror.h"
#include "khttpinputfilter.h"
#include "khttplog.h"
#include "khttpoutputfilter.h"
#include "khttppath.h"
#include "khttprouter.h"
#include "khttpserver.h"
#include "kinpipe.h"
#include "kinshell.h"
#include "kinstringstream.h"
#include "kjoin.h"
#include "kjson.h"
#include "klambdastream.h"
#include "klog.h"
#include "kmail.h"
#include "kmakecopyable.h"
#include "kmessagedigest.h"
#include "kmime.h"
#include "kmpsearch.h"
#include "kmru.h"
#include "kopenid.h"
#include "koptions.h"
#include "koutpipe.h"
#include "koutputtemplate.h"
#include "koutshell.h"
#include "koutstringstream.h"
#include "kparallel.h"
#include "kpersist.h"
#include "kpipe.h"
#include "kpoll.h"
#include "kpool.h"
#include "kprof.h"
#include "kprops.h"
#include "kquotedprintable.h"
#include "kreader.h"
#include "kregex.h"
#include "kreplacer.h"
#include "krest.h"
#include "krestclient.h"
#include "krestroute.h"
#include "krestserver.h"
#include "kron.h"
#include "krow.h"
#include "krsakey.h"
#include "krsasign.h"
#include "kscopeguard.h"
#include "ksharedmemory.h"
#include "ksharedptr.h"
#include "ksharedref.h"
#include "ksignals.h"
#include "ksmtp.h"
#include "ksnippets.h"
#include "ksplit.h"
#include "ksql.h"
#include "ksqlite.h"
#include "ktlsstream.h"
#include "kstack.h"
#include "kstream.h"
#include "kstreambuf.h"
#include "kstring.h"
#include "kstringstream.h"
#include "kstringutils.h"
#include "kstringview.h"
#include "ksubscribe.h"
#include "ksystem.h"
#include "ksystemstats.h"
#include "ktcpclient.h"
#include "ktcpserver.h"
#include "ktcpstream.h"
#include "kthreadpool.h"
#include "kthreads.h"
#include "kthreadsafe.h"
#include "ktime.h"
#include "ktimer.h"
#include "kunixstream.h"
#include "kuntar.h"
#include "kurl.h"
#include "kurlencode.h"
#include "kuseragent.h"
#include "kutf8.h"
#include "kutf8iterator.h"
#include "kvariant.h"
#include "kversion.h"
#include "kwebclient.h"
#include "kwebobjects.h"
#include "kwebsocket.h"
#include "kwords.h"
#include "kwriter.h"
#include "kxml.h"
#include "kzip.h"
