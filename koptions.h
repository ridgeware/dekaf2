/*
 //
 // DEKAF(tm): Lighter, Faster, Smarter(tm)
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
 //
 */

#pragma once

#include "kstringview.h"
#include "kwriter.h"
#include "klog.h"
#include <exception>


/// @file koptions.h
/// option parsing


namespace dekaf2 {

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class KOptions
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	class MissingParameterError : public std::runtime_error
	{
		using runtime_error::runtime_error;
	};

	class WrongParameterError : public std::runtime_error
	{
		using runtime_error::runtime_error;
	};

	KOptions() = delete;
	KOptions(const KOptions&) = delete;
	KOptions(KOptions&&) = delete;
	virtual ~KOptions();
	KOptions& operator=(const KOptions&) = delete;
	KOptions& operator=(KOptions&&) = delete;

	// The only ctor, requiring basic initialization
	explicit KOptions (int& retval, bool bEmptyParmsIsError, KStringView sCliDebugTo=KLog::STDOUT);

	void SetHelp(KStringView* sHelp, size_t iCount)
	{
		m_sHelp = sHelp;
		m_sHelpSize = iCount;
	}

	bool Options(int argc, char** argv, KOutStream& out);

//----------
protected:
//----------

	class CParms
	{

	public:

		struct Arg_t
		{
			Arg_t() = default;
			Arg_t(KStringView sArg_);

			bool IsCommand() const { return bIsSingleDashed || bIsDoubleDashed; }

			KStringView sArg;
			bool bIsSingleDashed { false };
			bool bIsDoubleDashed { false };
			bool bConsumed       { false };
		};

		using ArgVec   = std::vector<Arg_t>;
		using iterator = ArgVec::iterator;

		CParms() = default;
		CParms(int argc_, char** argv_);
		virtual ~CParms();

		size_t size() const  { return Args.size();  }
		size_t empty() const { return Args.empty(); }
		iterator begin()     { return Args.begin(); }
		iterator end()       { return Args.end();   }

		ArgVec Args;
		KStringView sProgramName;
		size_t iArg { 0 };

	};

	virtual std::unique_ptr<CParms> CreateParms(int argc, char** argv);

	/// Hook for derived classes to check for commands
	virtual bool Command(CParms::iterator& Arg) { return false; }
	/// Hook for derived classes to check for parameters without dashes
	virtual void Parameter(CParms::iterator& Arg) {}

	virtual bool Evaluate(KOutStream& out);

	std::unique_ptr<CParms> m_Parms;

//----------
private:
//----------

	void Help();
	void SetRetval(int iVal);

	int*         m_retval { nullptr };
	KStringView* m_sHelp { nullptr };
	size_t       m_sHelpSize { 0 };
	bool         m_bEmptyParmsIsError { true };
	KString      m_sCliDebugTo;
};


/*
struct Parms
{
	uint16_t iPort { 80 };
	bool bCGI { false };
};

class MyMain : public KMain
{
public:

	explicit MyMain(int argc, char** argv, int& retval, bool bEmptyParmsIsError)
	: KMain(argc, argv, retval, bEmptyParmsIsError)
	{}

	class MyParms : public CParms, public Parms
	{
	public:

		MyParms(int argc, char** argv)
		: CParms(argc, argv)
		{}

	};

	virtual std::unique_ptr<CParms> CreateParms(int argc, char** argv) override
	{
		return std::make_unique<MyParms>(argc, argv);
	}

	virtual bool Command(CParms::iterator& Arg) override
	{
		MyParms& parms = static_cast<MyParms&>(*m_Parms);

		if (Arg->sArg == "p")
		{
			++Arg;
			if (Arg == parms.end())
			{
				throw MissingParameterError("need port number");
			}
			parms.iPort = Arg->sArg.UInt16();
			if (!parms.iPort)
			{
				throw WrongParameterError(kFormat("need port number > 1, got {}", Arg->sArg));
			}
		}
		else
		{
			return false;
		}
	}

};

int main(int argc, char** argv)
{
	int retval { 0 };

	KOutStream out(std::cerr);

	MyMain Main(argc, argv, retval, true);

	if (Main.Options(out))
	{

	}

	return retval;
}
*/

} // end of namespace dekaf2









