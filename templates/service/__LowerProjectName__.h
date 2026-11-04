
// dekaf2 generated project using the __ProjectType__ template

#pragma once

#include <dekaf2/core/strings/kstring.h>
#include <dekaf2/core/strings/kstringview.h>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>

using namespace dekaf2;

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class __ProjectName__
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	__ProjectName__ ();

	int Main (int argc, char** argv);

	/// ends the work loop after the current round, from any thread
	void Stop ();

	static constexpr KStringViewZ s_sProjectName    { "__ProjectName__" };
	static constexpr KStringViewZ s_sProjectVersion { "__ProjectVersion__" };

//----------
protected:
//----------

	/// one round of work
	void Work ();
	void ShowVersion ();

//----------
private:
//----------

	std::chrono::seconds    m_Interval { 60 };
	std::mutex              m_Mutex;
	std::condition_variable m_Wakeup;
	std::atomic<bool>       m_bStop    { false };
	std::size_t             m_iRounds  { 0 };

}; // __ProjectName__
