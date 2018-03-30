/*
//-----------------------------------------------------------------------------//
//
// DEKAF(tm): Lighter, Faster, Smarter (tm)
//
// Copyright (c) 2018, Ridgeware, Inc.
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

#include "khttpserver.h"

namespace dekaf2 {


//-----------------------------------------------------------------------------
void KHTTPServer::clear()
//-----------------------------------------------------------------------------
{
	Request.clear();
	Response.clear();
	m_sError.clear();
	m_bRequestCompression = true;
}

//-----------------------------------------------------------------------------
KHTTPServer::KHTTPServer(KStream& Stream)
//-----------------------------------------------------------------------------
{
	Accept(Stream);

} // Ctor

//-----------------------------------------------------------------------------
bool KHTTPServer::Accept(KStream& Stream)
//-----------------------------------------------------------------------------
{
	SetError(KStringView{});

	Stream.SetReaderEndOfLine('\n');
	Stream.SetReaderLeftTrim("");
	Stream.SetReaderRightTrim("\r\n");
	Stream.SetWriterEndOfLine("\r\n");

	Response.SetOutputStream(Stream);
	Request.SetInputStream(Stream);

	return true;

} // Connect

//-----------------------------------------------------------------------------
void KHTTPServer::Disconnect()
//-----------------------------------------------------------------------------
{
	Response.close();
	Request.close();
}

//-----------------------------------------------------------------------------
bool KHTTPServer::Parse()
//-----------------------------------------------------------------------------
{
	if (!Request.Parse())
	{
		SetError(Request.Error());
		return false;
	}

	return true;

} // Parse

//-----------------------------------------------------------------------------
bool KHTTPServer::SetError(KStringView sError) const
//-----------------------------------------------------------------------------
{
	kDebug(1, "{}", sError);
	m_sError = sError;
	return false;

} // SetError


} // end of namespace dekaf2
