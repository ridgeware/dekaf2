/*
 //
 // DEKAF(tm): Lighter, Faster, Smarter(tm)
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
 //
 */

#pragma once

/// @file krsakey.h
/// RSA key conversions

#include "kstringview.h"
#include "kjson.h"

namespace dekaf2 {

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// RSA key class to transform formats
class KRSAKey
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	/// Parameter struct to create an RSA key
	struct Parameters
	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{
		/// constructs the parameter set from a json node
		Parameters(const KJSON& json);

		KStringView n;
		KStringView e;
		KStringView d;
		KStringView p;
		KStringView q;
		KStringView dp;
		KStringView dq;
		KStringView qi;
	};

	KRSAKey() = default;
	/// construct the key from parameters
	KRSAKey(const Parameters& parms)
	{
		Create(parms);
	}
	/// construct the key from JSON
	KRSAKey(const KJSON& json)
	{
		Create(json);
	}
	/// construct the key from PEM strings
	KRSAKey(KStringView sPubKey, KStringView sPrivKey = KStringView{})
	{
		Create(sPubKey, sPrivKey);
	}

	/// copy construction
	KRSAKey(const KRSAKey&) = delete;
	/// move construction
	KRSAKey(KRSAKey&&);
	~KRSAKey()
	{
		clear();
	}

	/// copy assignment
	KRSAKey& operator=(const KRSAKey&) = delete;
	/// move assignment
	KRSAKey& operator=(KRSAKey&&);

	/// reset the key
	void clear();
	/// test if key is set
	bool empty() const { return !m_EVPPKey; }
	/// create the key from parameters
	bool Create(const Parameters& parms);
	/// create the key from JSON
	bool Create(const KJSON& json)
	{
		return Create(Parameters(json));
	}
	/// create the key from PEM strings
	bool Create(KStringView sPubKey, KStringView sPrivKey = KStringView{});
	/// get the key
	void* GetEVPPKey() const;

//------
private:
//------

	void* m_EVPPKey { nullptr }; // is a EVP_PKEY

}; // KRSAKey


} // end of namespace dekaf2

