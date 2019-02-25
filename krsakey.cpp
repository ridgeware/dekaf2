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

#include <openssl/evp.h>
#include <openssl/pem.h>
#include "krsakey.h"
#include "kbase64.h"
#include "klog.h"

namespace dekaf2 {

//---------------------------------------------------------------------------
BIGNUM* Base64ToBignum(KStringView sBase64)
//---------------------------------------------------------------------------
{
	auto sBin = KBase64Url::Decode(sBase64);

	return BN_bin2bn(reinterpret_cast<unsigned char*>(sBin.data()), sBin.size(), nullptr);

} // Base64ToBignum

//---------------------------------------------------------------------------
void KRSAKey::Release()
//---------------------------------------------------------------------------
{
	if (m_EVPPKey)
	{
		EVP_PKEY_free(static_cast<EVP_PKEY*>(m_EVPPKey));
		m_EVPPKey = nullptr;
	}

	m_bCreated = false;

} // dtor

//---------------------------------------------------------------------------
void KRSAKey::clear()
//---------------------------------------------------------------------------
{
	n.clear();
	e.clear();
	d.clear();
	p.clear();
	q.clear();
	dp.clear();
	dq.clear();
	qi.clear();

	Release();

} // clear

//---------------------------------------------------------------------------
bool KRSAKey::Create()
//---------------------------------------------------------------------------
{
	Release();

	auto rsa = RSA_new();

	if (!rsa)
	{
		return false;
	}

	rsa->n = Base64ToBignum(n);
	rsa->e = Base64ToBignum(e);
	if (!d.empty())
	{
		rsa->d = Base64ToBignum(d);
	}
	if (!p.empty())
	{
		rsa->p = Base64ToBignum(p);
	}
	if (!q.empty())
	{
		rsa->q = Base64ToBignum(q);
	}
	if (!dp.empty())
	{
		rsa->dmp1 = Base64ToBignum(dp);
	}
	if (!dq.empty())
	{
		rsa->dmq1 = Base64ToBignum(dq);
	}
	if (!qi.empty())
	{
		rsa->iqmp = Base64ToBignum(qi);
	}

	m_EVPPKey = EVP_PKEY_new();
	if (!m_EVPPKey)
	{
		RSA_free(rsa);
		return false;
	}

	EVP_PKEY_assign(static_cast<EVP_PKEY*>(m_EVPPKey), EVP_PKEY_RSA, rsa);

	m_bCreated = true;
	
	return true;

} // Create

//---------------------------------------------------------------------------
void* KRSAKey::GetEVPPKey() const
//---------------------------------------------------------------------------
{
	if (!m_bCreated)
	{
		kDebug(1, "EVP Key not yet created..");
	}

	return m_EVPPKey;

} // GetEVPPKey

} // end of namespace dekaf2


