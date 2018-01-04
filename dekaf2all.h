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

// - - - - - - - - - - - - - - - - - -
// KEEP ALPHABETIZED!!
// - - - - - - - - - - - - - - - - - -
#include "dekaf2.h"
#include "kcache.h"
#include "kcasestring.h"
#include "kcgi.h"
//KTEMP1//#include "kconnection.h"
#include "kcrashexit.h"
#include "kcurl.h"
//KTEMP1//#include "kfdstream.h"
#include "kfile.h"
#include "kformat.h"
#include "kgetruntimestack.h"
#include "khash.h"
//KTEMP1//#include "khttp.h"
#include "khttp_header.h"
#include "khttp_method.h"
#include "kinpipe.h"
//KTEMP1//#include "kinshell.h"
#include "kjson.h"
#include "klog.h"
#include "kmime.h"
#include "kmru.h"
#include "kostringstream.h"
#include "koutpipe.h"
#include "koutshell.h"
#include "kparallel.h"
#include "kpipe.h"
#include "kprof.h"
#include "kprops.h"
#include "kreader.h"
#include "kregex.h"
#include "ksharedref.h"
#include "ksignals.h"
#include "ksplit.h"
//KTEMP1//#include "ksslclient.h"
//KTEMP1//#include "ksslstream.h"
#include "kstack.h"
//KTEMP1//#include "kstream.h"
//KTEMP1//#include "kstreambuf.h"
#include "kstring.h"
#include "kstringutils.h"
#include "kstringview.h"
#include "ksubscribe.h"
#include "ksystem.h"
#include "ktcpclient.h"
//KTEMP1//#include "ktcpserver.h"
#include "kurl.h"
#include "kurlencode.h"
#include "kuseragent.h"
//KTEMP1//#include "kwebio.h"
#include "kwriter.h"
