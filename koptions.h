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
#include "kstack.h"
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

	/// throw in case of a missing parameter - it will report the param it got called with
	class MissingParameterError : public std::runtime_error
	{
		using runtime_error::runtime_error;
	};

	/// throw in case of a wrong parameter - it will report the param it got called with
	class WrongParameterError : public std::runtime_error
	{
		using runtime_error::runtime_error;
	};

	/// throw in case of a general error - it will not report a specific param it got called with
	class Error : public std::runtime_error
	{
		using runtime_error::runtime_error;
	};

	KOptions() = delete;
	KOptions(const KOptions&) = delete;
	KOptions(KOptions&&) = default;
	KOptions& operator=(const KOptions&) = delete;
	KOptions& operator=(KOptions&&) = default;

	/// ctor, requiring basic initialization
	explicit KOptions (bool bEmptyParmsIsError, KStringView sCliDebugTo = KLog::STDOUT);

	/// Parse arguments and call the registered callback functions. Returns 0
	/// if valid, -1 if -help was called, and > 0 for error
	int Parse(int argc, char** argv, KOutStream& out);

	using ArgList   = KStack<KStringViewZ>;
	using Callback0 = std::function<void()>;
	using Callback1 = std::function<void(KStringViewZ)>;
	using CallbackN = std::function<void(ArgList&)>;

	/// Register a callback function for occurences of "-sOption" with no additional args
	void RegisterOption(KStringView sOption, Callback0 Function);

	/// Register a callback function for occurences of "sCommand" with no additional args
	void RegisterCommand(KStringView sCommand, Callback0 Function);

	/// Register a callback function for occurences of "-sOption" with exactly one additional arg.
	/// The sMissingParm string is output if there is no additional arg.
	void RegisterOption(KStringView sOption, KStringViewZ sMissingParm, Callback1 Function);

	/// Register a callback function for occurences of "sCommand" with exactly one additional arg.
	/// The sMissingParm string is output if there is no additional arg.
	void RegisterCommand(KStringView sCommand, KStringViewZ sMissingParm, Callback1 Function);

	/// Register a callback function for occurences of "-sOption" with an arbitrary, but defined minimal amount of additional args
	/// The sMissingParms string is output if there are less than iMinArgs args.
	void RegisterOption(KStringView sOption, uint16_t iMinArgs, KStringViewZ sMissingParms, CallbackN Function);

	/// Register a callback function for occurences of "sCommand" with an arbitrary, but defined minimal amount of additional args
	/// The sMissingParms string is output if there are less than iMinArgs args.
	void RegisterCommand(KStringView sCommand, uint16_t iMinArgs, KStringViewZ sMissingParms, CallbackN Function);

	/// Register a callback function for unhandled options
	void RegisterUnknownOption(CallbackN Function);

	/// Register a callback function for unhandled commands
	void RegisterUnknownCommand(CallbackN Function);

	/// Register an array of KStringViews as help output
	template<std::size_t COUNT>
	void RegisterHelp(KStringView (&sHelp)[COUNT])
	{
		m_sHelp = sHelp;
		m_sHelpSize = COUNT;
	}

	/// Output the registered help message
	void Help(KOutStream& out);

//----------
private:
//----------

	class CLIParms
	{

	public:

		struct Arg_t
		{
			Arg_t() = default;
			Arg_t(KStringViewZ sArg_);

			bool IsOption() const { return iDashes; }
			KStringViewZ Dashes() const;

			KStringViewZ sArg;
			bool bConsumed { false };

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
		KStringViewZ sProgramName;
		size_t iArg { 0 };

	}; // CLIParms

	int Evaluate(KOutStream& out);

	class CallbackParams
	{

	public:

		CallbackParams() = default;
		CallbackParams(const CallbackParams&) = default;
		CallbackParams(CallbackParams&&) = default;
		CallbackParams& operator=(const CallbackParams&) = default;
		CallbackParams& operator=(CallbackParams&&) = default;

		CallbackParams(uint16_t _iMinArgs, KStringViewZ _sMissingParms, CallbackN _func)
		: func(_func)
		, sMissingParms(_sMissingParms)
		, iMinArgs(_iMinArgs)
		{}

		CallbackN    func { nullptr };
		KStringViewZ sMissingParms { };
		uint16_t     iMinArgs { 0 };

	}; // CallbackParams

	using CommandStore = std::unordered_map<KString, CallbackParams>;

	CLIParms       m_CLIParms;
	CommandStore   m_Commands;
	CommandStore   m_Options;
	CallbackParams m_UnknownCommand;
	CallbackParams m_UnknownOption;
	KString        m_sCliDebugTo;
	KStringView*   m_sHelp { nullptr };
	size_t         m_sHelpSize { 0 };
	bool           m_bEmptyParmsIsError { true };

}; // KOptions

} // end of namespace dekaf2









