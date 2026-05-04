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
#include <dekaf2/core/init/dekaf2.h>
#include <dekaf2/core/types/kaddrplus.h>
#include <dekaf2/crypto/cipher/kaes.h>
#include <dekaf2/containers/memory/kallocator.h>
#include <dekaf2/crypto/cipher/karia.h>
#include <dekaf2/containers/associative/kassociative.h>
#include <dekaf2/threading/primitives/katomic_object.h>
#include <dekaf2/crypto/auth/kawsauth.h>
#include <dekaf2/core/format/kbar.h>
#include <dekaf2/crypto/encoding/kbase64.h>
#include <dekaf2/crypto/auth/kbcrypt.h>
#include <dekaf2/core/types/kbit.h>
#include <dekaf2/core/types/kbitfields.h>
#include <dekaf2/crypto/cipher/kblockcipher.h>
#include <dekaf2/containers/sequential/kbuffer.h>
#include <dekaf2/io/readwrite/kbufferedreader.h>
#include <dekaf2/containers/associative/kcache.h>
#include <dekaf2/crypto/cipher/kcamellia.h>
#include <dekaf2/core/strings/kcaseless.h>
#include <dekaf2/core/strings/kcasestring.h>
#include <dekaf2/core/strings/kcesu8.h>
#include <dekaf2/http/server/kcgistream.h>
#include <dekaf2/crypto/cipher/kchacha.h>
#include <dekaf2/system/process/kchildprocess.h>
#include <dekaf2/http/protocol/kchunkedtransfer.h>
#include <dekaf2/core/init/kcompatibility.h>
#include <dekaf2/io/compression/kcompression.h>
#include <dekaf2/rest/limits/kconnectionlimiter.h>
#include <dekaf2/http/cookie/kcookie.h>
#include <dekaf2/io/streams/kcountingstreambuf.h>
#include <dekaf2/core/errors/kcrashexit.h>
#include <dekaf2/crypto/hash/kcrc.h>
#include <dekaf2/data/csv/kcsv.h>
#include <dekaf2/crypto/ec/keckey.h>
#include <dekaf2/crypto/ec/kecsign.h>
#include <dekaf2/core/types/kctype.h>
#include <dekaf2/io/pipes/kdataconsumer.h>
#include <dekaf2/io/pipes/kdataprovider.h>
#include <dekaf2/time/clock/kdate.h>
#include <dekaf2/core/init/kdefinitions.h>
#include <dekaf2/net/udp/kdtlsserver.h>
#include <dekaf2/net/udp/kdtlssocket.h>
#include <dekaf2/util/text/kdiff.h>
#include <dekaf2/time/duration/kduration.h>
#include <dekaf2/crypto/encoding/kencode.h>
#include <dekaf2/core/types/keraseremove.h>
#include <dekaf2/core/errors/kerror.h>
#include <dekaf2/core/errors/kexception.h>
#include <dekaf2/io/streams/kfdstream.h>
#include <dekaf2/rest/serving/kfileserver.h>
#include <dekaf2/system/filesystem/kfilesystem.h>
#include <dekaf2/core/format/kformat.h>
#include <dekaf2/core/format/kformtable.h>
#include <dekaf2/core/types/kfrozen.h>
#include <dekaf2/system/os/kgetruntimestack.h>
#include <dekaf2/crypto/hash/khash.h>
#include <dekaf2/system/os/kheapmon.h>
#include <dekaf2/crypto/encoding/khex.h>
#include <dekaf2/crypto/kdf/khkdf.h>
#include <dekaf2/containers/sequential/khistory.h>
#include <dekaf2/crypto/hash/khmac.h>
#include <dekaf2/web/html/khtmlcontentblocks.h>
#include <dekaf2/web/html/khtmldom.h>
#include <dekaf2/web/html/khtmlentities.h>
#include <dekaf2/web/html/khtmlparser.h>
#include <dekaf2/http/protocol/khttp2.h>
#include <dekaf2/http/protocol/khttp3.h>
#include <dekaf2/http/protocol/khttp_header.h>
#include <dekaf2/http/protocol/khttp_method.h>
#include <dekaf2/http/protocol/khttp_request.h>
#include <dekaf2/http/protocol/khttp_response.h>
#include <dekaf2/http/protocol/khttp_version.h>
#include <dekaf2/http/client/khttpclient.h>
#include <dekaf2/http/protocol/khttpcompression.h>
#include <dekaf2/http/server/khttperror.h>
#include <dekaf2/http/server/khttpinputfilter.h>
#include <dekaf2/http/server/khttplog.h>
#include <dekaf2/http/server/khttpoutputfilter.h>
#include <dekaf2/http/server/khttppath.h>
#include <dekaf2/http/server/khttprouter.h>
#include <dekaf2/http/server/khttpserver.h>
#include <dekaf2/io/pipes/kinpipe.h>
#include <dekaf2/system/process/kinshell.h>
#include <dekaf2/io/streams/kinstringstream.h>
#include <dekaf2/net/util/kiostreamsocket.h>
#include <dekaf2/net/address/kipaddress.h>
#include <dekaf2/net/address/kipnetwork.h>
#include <dekaf2/core/strings/kjoin.h>
#include <dekaf2/data/json/kconfig.h>
#include <dekaf2/data/json/kjson.h>
#include <dekaf2/data/json/kjson2.h>
#include <dekaf2/time/clock/kjuliandate.h>
#include <dekaf2/io/streams/klambdastream.h>
#include <dekaf2/containers/associative/klockmap.h>
#include <dekaf2/core/logging/klog.h>
#include <dekaf2/core/logging/klogrotate.h>
#include <dekaf2/util/mail/kmail.h>
#include <dekaf2/core/types/kmakecopyable.h>
#include <dekaf2/crypto/hash/kmessagedigest.h>
#include <dekaf2/web/url/kmime.h>
#include <dekaf2/io/streams/kmodifyingstreambuf.h>
#include <dekaf2/core/strings/kmpsearch.h>
#include <dekaf2/containers/associative/kmru.h>
#include <dekaf2/net/address/knetworkinterface.h>
#include <dekaf2/util/text/kngram.h>
#include <dekaf2/crypto/auth/kopenid.h>
#include <dekaf2/util/cli/koptions.h>
#include <dekaf2/io/pipes/koutpipe.h>
#include <dekaf2/data/template/koutputtemplate.h>
#include <dekaf2/system/process/koutshell.h>
#include <dekaf2/io/streams/koutstringstream.h>
#include <dekaf2/threading/execution/kparallel.h>
#include <dekaf2/containers/memory/kpersist.h>
#include <dekaf2/io/pipes/kpipe.h>
#include <dekaf2/net/util/kpoll.h>
#include <dekaf2/containers/memory/kpool.h>
#include <dekaf2/time/duration/kprof.h>
#include <dekaf2/containers/associative/kprops.h>
#include <dekaf2/system/process/kpty.h>
#include <dekaf2/net/quic/kquicstream.h>
#include <dekaf2/crypto/encoding/kquotedprintable.h>
#include <dekaf2/crypto/random/krandom.h>
#include <dekaf2/rest/limits/kratelimiter.h>
#include <dekaf2/io/readwrite/kread.h>
#include <dekaf2/io/readwrite/kreader.h>
#include <dekaf2/core/types/kreference_proxy.h>
#include <dekaf2/core/strings/kregex.h>
#include <dekaf2/data/template/kreplacer.h>
#include <dekaf2/net/address/kresolve.h>
#include <dekaf2/rest/framework/krest.h>
#include <dekaf2/rest/framework/krestclient.h>
#include <dekaf2/rest/framework/krestroute.h>
#include <dekaf2/rest/framework/krestserver.h>
#include <dekaf2/time/scheduler/kron.h>
#include <dekaf2/data/sql/krow.h>
#include <dekaf2/crypto/rsa/krsa.h>
#include <dekaf2/crypto/rsa/krsacert.h>
#include <dekaf2/crypto/rsa/krsakey.h>
#include <dekaf2/crypto/rsa/krsasign.h>
#include <dekaf2/core/types/kscopeguard.h>
#include <dekaf2/system/shared/ksharedmemory.h>
#include <dekaf2/containers/memory/ksharedptr.h>
#include <dekaf2/containers/memory/ksharedref.h>
#include <dekaf2/system/os/ksignals.h>
#include <dekaf2/util/mail/ksmtp.h>
#include <dekaf2/data/template/ksnippets.h>
#include <dekaf2/core/errors/ksourcelocation.h>
#include <dekaf2/core/strings/ksplit.h>
#include <dekaf2/data/sql/ksql.h>
#include <dekaf2/data/sql/ksqlite.h>
#include <dekaf2/containers/sequential/kstack.h>
#include <dekaf2/io/streams/kstream.h>
#include <dekaf2/io/streams/kstreambuf.h>
#include <dekaf2/io/streams/kstreambufadaptor.h>
#include <dekaf2/net/util/kstreamoptions.h>
#include <dekaf2/core/strings/kstring.h>
#include <dekaf2/io/streams/kstringstream.h>
#include <dekaf2/core/strings/kstringutils.h>
#include <dekaf2/core/strings/kstringview.h>
#include <dekaf2/core/types/ksubscribe.h>
#include <dekaf2/system/os/ksystem.h>
#include <dekaf2/system/os/ksystemstats.h>
#include <dekaf2/net/tcp/ktcpclient.h>
#include <dekaf2/net/tcp/ktcpserver.h>
#include <dekaf2/net/tcp/ktcpstream.h>
#include <dekaf2/util/misc/ktemperature.h>
#include <dekaf2/core/types/ktemplate.h>
#include <dekaf2/threading/execution/kthreadpool.h>
#include <dekaf2/threading/execution/kthreads.h>
#include <dekaf2/threading/primitives/kthreadsafe.h>
#include <dekaf2/time/clock/ktime.h>
#include <dekaf2/time/duration/ktimer.h>
#include <dekaf2/time/series/ktimeseries.h>
#include <dekaf2/net/tls/ktlscontext.h>
#include <dekaf2/net/tls/ktlsstream.h>
#include <dekaf2/net/util/ktunnel.h>
#include <dekaf2/net/udp/kudpserver.h>
#include <dekaf2/net/udp/kudpsocket.h>
#include <dekaf2/net/udp/kudpstream.h>
#include <dekaf2/net/tcp/kunixstream.h>
#include <dekaf2/util/archive/kuntar.h>
#include <dekaf2/web/url/kurl.h>
#include <dekaf2/web/url/kurlencode.h>
#include <dekaf2/web/url/kuseragent.h>
#include <dekaf2/core/strings/kutf.h>
#include <dekaf2/core/strings/kutf_iterator.h>
#include <dekaf2/util/id/kuuid.h>
#include <dekaf2/core/types/kvariant.h>
#include <dekaf2/util/misc/kversion.h>
#include <dekaf2/http/client/kwebclient.h>
#include <dekaf2/rest/serving/kwebdav.h>
#include <dekaf2/web/objects/kwebobjects.h>
#include <dekaf2/web/push/kwebpush.h>
#include <dekaf2/rest/serving/kwebserver.h>
#include <dekaf2/rest/serving/kwebserverpermissions.h>
#include <dekaf2/http/websocket/kwebsocket.h>
#include <dekaf2/http/websocket/kwebsocketclient.h>
#include <dekaf2/core/strings/kwords.h>
#include <dekaf2/io/readwrite/kwrite.h>
#include <dekaf2/io/readwrite/kwriter.h>
#include <dekaf2/data/xml/kxml.h>
#include <dekaf2/util/cli/kxterm.h>
#include <dekaf2/util/archive/kzip.h>
