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

using namespace dekaf2;

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
	static constexpr KStringViewZ s_sProjectVersion { "0.0.2" };

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
	};

//----------
protected:
//----------

	void ServerQuery (std::size_t iRepeat = 1);
	void ShowVersion ();

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
		HeaderMap   Headers;
		KHTTPMethod Method         { KHTTPMethod::GET };
		enum Flags  Flags          { Flags::NONE };
	};

	//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	struct MultiRequest : BaseRequest
	//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{
		MultiRequest() = default;

		std::vector<std::pair<KURL, KString>> URLs; // URL, output stream
	};

	struct BuildMultiRequest : MultiRequest
	{
		void clear()                    { *this = BuildMultiRequest();                  }
		void AddURL(KURL url)           { m_BuildURLs.push_back(std::move(url));        }
		void AddOutput(KString sOutput) { m_BuildOutputs.push_back(std::move(sOutput)); }


		void BuildPairsOfURLAndOutput()
		{
			KStringView sLastOut;
			auto it = m_BuildOutputs.begin();

			for (auto& URL : m_BuildURLs)
			{
				if (it != m_BuildOutputs.end())
				{
					sLastOut = *it++;
				}
				if (sLastOut == sOutputToLastPathComponent)
				{
					KString sComponent = kBasename(URL.Path.get());
					URLs.push_back({std::move(URL), sComponent});
				}
				else
				{
					URLs.push_back({std::move(URL), sLastOut});
				}
			}

			m_BuildURLs.clear();
			m_BuildOutputs.clear();
		}

		std::vector<KURL>    m_BuildURLs;
		std::vector<KString> m_BuildOutputs;
	};

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

		const KURL& URL;
		const KString& sOutput;
		const BaseRequest& Config;
	};

	//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	struct RequestList
	//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{

	//----------
	public:
	//----------

		void Add(BuildMultiRequest BMRQ)
		{
			BMRQ.BuildPairsOfURLAndOutput();

			if (!BMRQ.URLs.empty())
			{
				m_MultiRequests.push_back(std::move(BMRQ));
			}
		}

		void CreateRequestList()
		{
			// now create the single request list out of the multi-requests that may have
			// been configured - we keep the MultiRequests as the data store, and only
			// create references to it in the RequestList
			for (auto& MRQ : m_MultiRequests)
			{
				for (auto& URL : MRQ.URLs)
				{
					m_RequestList.push_back(SingleRequest(URL.first, URL.second, MRQ));
				}
			}
			it.unique().get() = m_RequestList.begin();
		}


		const SingleRequest* GetNextRequest()
		{
			auto p = it.unique();
			if (*p == m_RequestList.end())
			{
				return nullptr;
			}
			else
			{
				return &*(*p)++;
			}
		}

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

	};

	//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	struct Config
	//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{
		KThreadSafe<std::size_t> iRepeat       { 1 };
		std::size_t iParallel                  { 1 };
	};

	Config m_Config;

//----------
private:
//----------

	KOptions m_CLI { true };

	RequestList m_RequestList;
	// this is the request that gets filled up subsequently by CLI options
	BuildMultiRequest BuildMRQ;

}; // kurl

DEKAF2_ENUM_IS_FLAG(kurl::Flags)
