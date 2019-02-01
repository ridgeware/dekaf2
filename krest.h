/*
//
// DEKAF(tm): Lighter, Faster, Smarter (tm)
//
// Copyright (c) 2019, Ridgeware, Inc.
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

#include "krestserver.h"

/// @file krest.h
/// HTTP REST service implementation

namespace dekaf2 {

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class KREST
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	enum ServerType { UNDEFINED, HTTP, UNIX, CGI, FCGI, LAMBDA, CLI, SIMULATE_HTTP };

	//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	/// define options for the rest server
	struct Options
	//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{
		void AddHeader(KStringView sHeader, KStringView sValue);

		ServerType Type { UNDEFINED };
		uint16_t iPort { 0 };
		uint16_t iMaxConnections { 20 };
		uint16_t iTimeout { 5 };
		KStringViewZ sSocketFile;
		KStringViewZ sCert;
		KStringViewZ sKey;
		KStringView sBaseRoute;
		KRESTServer::ResponseHeaders ResponseHeaders;

	}; // Options

	/// handle one REST request, or start REST server in HTTP and UNIX modes
	bool Execute(const Options& Params, const KRESTRoutes& Routes);
	/// handle one REST request, read input from file, output to OutStream
	bool ExecuteFromFile(const Options& Params, const KRESTRoutes& Routes, KStringView sFilename, KOutStream& OutStream = KOut);
	/// simulate one REST request in HTTP/CGI mode, read input from sSimulate, output to OutStream
	bool Simulate(const Options& Params, const KRESTRoutes& Routes, KStringView sSimulate, KOutStream& OutStream = KOut);

	/// returns true if no error
	bool Good() const;
	/// returns error description
	const KString& Error() const;

//----------
protected:
//----------

	bool RealExecute(KStream& Stream, KRESTServer::OutputType Type, KStringView sBaseRoute, const KRESTRoutes& Routes, const KRESTServer::ResponseHeaders& Headers = KRESTServer::ResponseHeaders{}, KStringView sRemoteIP = "0.0.0.0");
	bool SetError(KStringView sError);

//----------
private:
//----------

	KString m_sError;

}; // KREST

} // end of namespace dekaf2
