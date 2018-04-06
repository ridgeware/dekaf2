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
#include <functional>
#include <unordered_map>


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
	KOptions(KOptions&&) = default;
	KOptions& operator=(const KOptions&) = delete;
	KOptions& operator=(KOptions&&) = default;

	// ctor, requiring basic initialization
	explicit KOptions (int& retval, bool bEmptyParmsIsError, KStringView sCliDebugTo=KLog::STDOUT);

	void SetHelp(KStringView* sHelp, size_t iCount)
	{
		m_sHelp = sHelp;
		m_sHelpSize = iCount;
	}

	bool Options(int argc, char** argv, KOutStream& out);

	using ArgList = std::vector<KStringView>;
	using Callback = std::function<size_t(const ArgList&)>;

	void RegisterOption(KStringView sCmd, Callback Function)
	{
		m_Options.insert({sCmd, Function});
	}

	void RegisterCommand(KStringView sCmd, Callback Function)
	{
		m_Commands.insert({sCmd, Function});
	}

//----------
private:
//----------

	class CLIParms
	{

	public:

		struct Arg_t
		{
			Arg_t() = default;
			Arg_t(KStringView sArg_);

			bool IsOption() const { return iDashes; }
			KStringView Dashes() const;

			KStringView sArg;
			bool bConsumed  { false };

		private:

			static constexpr KStringView sDoubleDash = "--";

			uint8_t iDashes { 0 };

		};

		using ArgVec   = std::vector<Arg_t>;
		using iterator = ArgVec::iterator;

		CLIParms() = default;
		CLIParms(int argc, char** argv)
		{
			Create(argc, argv);
		}

		void Create(int argc, char** argv);
		size_t size() const  { return Args.size();  }
		size_t empty() const { return Args.empty(); }
		iterator begin()     { return Args.begin(); }
		iterator end()       { return Args.end();   }
		void clear()         { Args.clear();        }

		ArgVec Args;
		KStringView sProgramName;
		size_t iArg { 0 };

	};

	bool Evaluate(KOutStream& out);

	CLIParms m_CLIParms;

	void Help();
	void SetRetval(int iVal);

	using CommandStore = std::unordered_map<KString, Callback>;
	CommandStore m_Commands;
	CommandStore m_Options;

	static const Callback* FindCommand(const CommandStore& Store, KStringView sCommand);

	KString      m_sCliDebugTo;
	KStringView* m_sHelp { nullptr };
	size_t       m_sHelpSize { 0 };
	int*         m_retval { nullptr };
	bool         m_bEmptyParmsIsError { true };

}; // KOptions

} // end of namespace dekaf2









