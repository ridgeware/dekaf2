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
#include <dekaf2/kduration.h>
#include <dekaf2/kstringutils.h>
#include <dekaf2/kbar.h>

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

	void ServerQuery ();
	void ShowVersion ();
	void ShowProgress();
	void ShowStats   (KDuration tTotal, std::size_t iTotalRequests);

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
		KHTTPMethod Method          { KHTTPMethod::GET };
		enum Flags  Flags           { Flags::NONE };
		uint16_t    iSecondsTimeout { 5 };
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

		void CreateRequestList(std::size_t iRepeat)
		{
			m_iRepeat = iRepeat;

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
				if (m_iRepeat == 1)
				{
					return nullptr;
				}
				else
				{
					--m_iRepeat;
					*p = m_RequestList.begin();
				}
			}

			return &*(*p)++;
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
		std::size_t                m_iRepeat;

	};

	//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	struct Results
	//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{
	
	//----------
	public:
	//----------

		void Add(uint16_t iResultCode, KDuration tDuration)
		{
			auto Codes = m_ResultCodes.unique();
			auto it    = Codes->find(iResultCode);

			if (it == Codes->end())
			{
				auto pair = Codes->insert({iResultCode, Result{}});
				it = pair.first;
			}

			it->second.Add(tDuration);
		}

		KString Print()
		{
			auto Codes = m_ResultCodes.shared();

			KString sResult;

			for (const auto& it : *Codes)
			{
				sResult += kFormat("HTTP {}: count {}, avg {}",
								   it.first,
								   kFormNumber(it.second.m_tDuration.Rounds()),
								   it.second.m_tDuration.average());
			}

			return sResult;
		}


	//----------
	private:
	//----------

		//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
		struct Result
		//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
		{
			Result() = default;

			void Add(KDuration tDuration)
			{
				m_tDuration += tDuration;
			}

			KMultiDuration m_tDuration;

		}; // Result

		KThreadSafe<std::map<uint16_t, Result>> m_ResultCodes;

	}; // Results

	//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	struct Config
	//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{
		std::size_t iRepeat       { 1 };
		std::size_t iParallel     { 1 };
		bool        bShowStats    { false };
	};

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

		void SetType(Type t)
		{
			m_ProgressType = t;
		}

		void Start(std::size_t iSize)
		{
			if (m_ProgressType == Type::Bar)
			{
				m_Bar = std::make_unique<KBAR>(iSize, 
											   KBAR::DEFAULT_WIDTH,
											   KBAR::SLIDER,
											   KBAR::DEFAULT_DONE_CHAR,
											   KErr);
			}
		}

		void ProgressOne()
		{
			switch (m_ProgressType)
			{
				case Type::None:
					break;
				case Type::Wheel:
					PrintNextWheel();
					break;
				case Type::Bar:
					if (m_Bar)
					{
						m_Bar->Move();
					}
					break;
			}
		}

		void Finish()
		{
			switch (m_ProgressType)
			{
				case Type::None:
					break;
				case Type::Wheel:
					break;
				case Type::Bar:
					if (m_Bar)
					{
						m_Bar->Finish();
					}
					break;
			}
		}

	//----------
	private:
	//----------

		void PrintNextWheel()
		{
			static std::mutex mutex;
			std::lock_guard lock(mutex);

			KErr.Format("\b{:c}", GetNextWheelChar());
		}

		char GetNextWheelChar()
		{
			static constexpr std::array<char, 8> m_Wheel = {'/', '-', '\\', '|', '/', '-', '\\', '|'};

			if (m_WheelChar > 6) m_WheelChar = 0;
			else ++m_WheelChar;
			kDebug(2, "counter == {}, char = {:c}", m_WheelChar, m_Wheel[m_WheelChar]);
			return m_Wheel[m_WheelChar];
		}

		std::unique_ptr<KBAR> m_Bar;
		Type                  m_ProgressType { Type::None };
		uint8_t               m_WheelChar;

	}; // Progress

//----------
private:
//----------

	KOptions    m_CLI { true };
	Config      m_Config;
	RequestList m_RequestList;
	Results     m_Results;
	Progress    m_Progress;

	// this is the request that gets filled up subsequently by CLI options
	BuildMultiRequest BuildMRQ;

}; // kurl

DEKAF2_ENUM_IS_FLAG(kurl::Flags)
