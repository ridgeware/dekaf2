/*
 //
 // DEKAF(tm): Lighter, Faster, Smarter (tm)
 //
 // Copyright (c) 2022, Ridgeware, Inc.
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

#include <dekaf2/kdefinitions.h>
#include <dekaf2/kstring.h>
#include <dekaf2/kstringview.h>
#include <dekaf2/koptions.h>
#include <dekaf2/kurl.h>
#include <dekaf2/khttp_method.h>
#include <dekaf2/kmime.h>
#include <dekaf2/kassociative.h>
#include <dekaf2/kjson.h>
#include <dekaf2/kthreadsafe.h>
#include <dekaf2/kfilesystem.h>
#include <dekaf2/kduration.h>
#include <dekaf2/kstringutils.h>
#include <dekaf2/kbar.h>
#include <dekaf2/kcookie.h>

using namespace DEKAF2_NAMESPACE_NAME;

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class kurl
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	kurl ();

	int Main (int argc, char** argv);

	static constexpr KStringViewZ s_sProjectName    { "kurl"  };
	static constexpr KStringViewZ s_sProjectVersion { "1.0.0" };

	enum Flags
	{
		NONE             = 0,
		QUIET            = 1 << 0,
		PRETTY           = 1 << 1,
		VERBOSE          = 1 << 2,
		RESPONSE_HEADERS = 1 << 3,
		REQUEST_HEADERS  = 1 << 4,
		INSECURE_CERTS   = 1 << 5,
		CONNECTION_CLOSE = 1 << 6,
		FORCE_HTTP_1     = 1 << 7,
		FORCE_HTTP_2     = 1 << 8,
		FORCE_HTTP_3     = 1 << 9,
		FORCE_IPV4       = 1 << 10,
		FORCE_IPV6       = 1 << 11,
		ALLOW_SET_COOKIE = 1 << 12
	};

//----------
protected:
//----------

	void ServerQuery ();
	void ShowVersion ();
	void ShowStats   (KDuration tTotal, std::size_t iTotalRequests);
	void LoadConfig  ();

	static constexpr KStringView sOutputToLastPathComponent = "}.";

	using HeaderMap = KMap<KString, KString>;

	//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	struct BaseRequest
	//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{
		BaseRequest() = default;

		KString     sUnixSocket;
		KString     sRequestBody;
		KMIME       sRequestMIME;
		KString     sRequestCompression;
		KString     sUsername;
		KString     sPassword;
		KString     sAWSProvider;    //<provider1[:provider2[:region[:service]]]> --aws-sigv4 "aws:amz:us-west-1:es" --user "key:secret"
		HeaderMap   Headers;
		KCookies    Cookies;         // request cookies, loaded with -b,cookie
		KString     sLoadCookieJar;  // input filename for cookies
		KString     sStoreCookieJar; // output filename for set-cookies
		KHTTPMethod Method          { KHTTPMethod::GET };
		enum Flags  Flags           { Flags::NONE };
		uint16_t    iSecondsTimeout { 5 };
		uint16_t    iMaxRedirects   { 3 };
		bool        bMethodExplicitlySet { false };

	}; // BaseRequest

	//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	struct MultiRequest : BaseRequest
	//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{
		MultiRequest() = default;

		void BuildAuthenticationHeader ();

		std::vector<std::pair<KURL, KString>> URLs; // URL, output stream

	}; // MultiRequest

	//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	struct BuildMultiRequest : MultiRequest
	//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{
		void clear     ()                { *this = BuildMultiRequest();                  }
		void AddURL    (KURL url)        { m_BuildURLs.push_back(std::move(url));        }
		void AddOutput (KString sOutput) { m_BuildOutputs.push_back(std::move(sOutput)); }


		void BuildPairsOfURLAndOutput();

		std::vector<KURL>    m_BuildURLs;
		std::vector<KString> m_BuildOutputs;

	}; // BuildMultiRequest

	//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	struct SingleRequest
	//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{
		SingleRequest(const KURL& url, const KString& sOutput, const BaseRequest& base)
		: URL(url)
		, sOutput(sOutput)
		, Config(base)
		{
		}

		const KURL&        URL;
		const KString&     sOutput;
		const BaseRequest& Config;

	}; // SingleRequest

	//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	struct RequestList
	//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{

	//----------
	public:
	//----------

		void  Add               (BuildMultiRequest BMRQ);
		void  CreateRequestList (std::size_t iRepeat);

		const SingleRequest* GetNextRequest();

		void ResetRequest()
		{
			*it.unique() = m_RequestList.begin();
		}

		bool empty() const
		{
			return m_RequestList.empty();
		}

		std::size_t size() const
		{
			return m_RequestList.size();
		}

	//----------
	private:
	//----------

		std::vector<SingleRequest> m_RequestList;
		std::vector<MultiRequest>  m_MultiRequests;
		KThreadSafe<decltype(m_RequestList)::iterator> it;
		std::size_t                m_iRepeat;

	}; // RequestList

	//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	struct Results
	//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{
	
	//----------
	public:
	//----------

		void     Add   (uint16_t iResultCode, KDuration tDuration);
		KString  Print () const;
		uint16_t GetLastStatus() const;

	//----------
	private:
	//----------

		//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
		struct Result
		//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
		{
			Result() = default;

			void Add (KDuration tDuration)
			{
				m_tDuration += tDuration;
			}

			KMultiDuration m_tDuration;

		}; // Result

		KThreadSafe<std::map<uint16_t, Result>> m_ResultCodes;
		uint16_t m_iLastStatus { 0 };

	}; // Results

	//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	struct Config
	//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{
		std::size_t iRepeat       { 1 };
		std::size_t iParallel     { 1 };
		bool        bShowStats    { false };

	}; // Config

	//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	struct Progress
	//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{

	//----------
	public:
	//----------

		enum Type
		{
			None,
			Wheel,
			Bar
		};

		void SetType (Type t)
		{
			m_ProgressType = t;
		}

		void Start       (std::size_t iSize);
		void ProgressOne ();
		void Finish      ();

	//----------
	private:
	//----------

		void PrintNextWheel   ();
		char GetNextWheelChar ();

		std::unique_ptr<KBAR> m_Bar;
		Type                  m_ProgressType { Type::None };
		uint8_t               m_WheelChar;

	}; // Progress

//----------
private:
//----------

	void AddRequestData(KStringViewZ sArg, bool bEncodeAsForm, bool bTakeFile);

	KOptions    m_CLI { true };
	Config      m_Config;
	RequestList m_RequestList;
	Results     m_Results;
	Progress    m_Progress;

	// this is the request that gets filled up subsequently by CLI options
	BuildMultiRequest BuildMRQ;

	// search for .kurlrc at KURL_HOME, HOME ?
	bool        m_bLoadDefaultConfig { true };

}; // kurl

DEKAF2_ENUM_IS_FLAG(kurl::Flags)
