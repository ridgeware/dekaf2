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
#include "kstring.h"
#include "kwriter.h"
#include "kstack.h"
#include "kexception.h"
#include "kassociative.h"
#include <forward_list>
#include <functional>
#include <vector>


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
	class MissingParameterError : public KException
	{
		using KException::KException;
	};

	/// throw in case of a wrong parameter - it will report the param it got called with
	class WrongParameterError : public KException
	{
		using KException::KException;
	};

	/// throw in case of a bad option that is not allowed in a context - it will report the param it got called with
	class BadOptionError : public KException
	{
		using KException::KException;
	};

	/// throw in case of a general error - it will not report a specific param it got called with
	class Error : public KException
	{
		using KException::KException;
	};

	/// throw in case of no error, to simply terminate the option parsing
	class NoError : public KException
	{
		using KException::KException;
	};

	KOptions() = delete;
	KOptions(const KOptions&) = delete;
	KOptions(KOptions&&) = default;
	KOptions& operator=(const KOptions&) = delete;
	KOptions& operator=(KOptions&&) = default;

	/// ctor, requiring basic initialization
	explicit KOptions (bool bEmptyParmsIsError, KStringView sCliDebugTo = KLog::STDOUT, bool bThrow = false);

	/// set a brief description of the program, will appear in first line of generated help
	KOptions& SetBriefDescription(KString sBrief) { m_sBriefDescription = std::move(sBrief); return *this; }

	/// set the separator style for the generated help - default is ::
	KOptions& SetHelpSeparator(KStringView sSeparator) { m_sSeparator = sSeparator; return *this; }

	/// set max generated help width in characters, default = 100
	KOptions& SetMaxHelpWidth(std::size_t iMaxWidth) { m_iMaxHelpRowWidth = iMaxWidth; return *this; }

	/// set indent for wrapped help lines, default 1
	KOptions& SetWrappedHelpIndent(std::size_t iIndent) { m_iWrappedHelpIndent = iIndent; return *this; }

	/// throw on errors or not?
	void Throw(bool bYesNo = true)
	{
		m_bThrow = bYesNo;
	}

	/// Parse arguments and call the registered callback functions. Returns 0
	/// if valid, -1 if -help was called, and > 0 for error
	int Parse(int argc, char const* const* argv, KOutStream& out = KOut);

	/// Parse arguments globbed together in a single string as if it was a command line
	int Parse(KString sCLI, KOutStream& out = KOut);

	/// Parse arguments line by line from a stream, multiple arguments allowed per line (like a full CLI)
	int Parse(KInStream& In, KOutStream& out = KOut);

	/// Parse arguments line by line from a file, multiple arguments allowed per line (like a full CLI)
	int ParseFile(KStringViewZ sFileName, KOutStream& out = KOut);

	/// Parse arguments from CGI QUERY_PARMS
	int ParseCGI(KStringViewZ sProgramName, KOutStream& out = KOut);

	using ArgList   = KStack<KStringViewZ>;
	using Callback0 = std::function<void()>;
	using Callback1 = std::function<void(KStringViewZ)>;
	using CallbackN = std::function<void(ArgList&)>;

	enum ArgTypes
	{
		Integer,
		Unsigned,
		Float,
		Boolean,
		String,
		File,      // file must exist
		Directory, // directory must exist
		Path,      // path component of pathname must exist
		Email,
		URL,
	};

//----------
private:
//----------

	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	class CallbackParam
	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{
	public:

		enum Flag
		{
			fNone        = 0,
			fIsRequired  = 1 << 0,
			fIsCommand   = 1 << 1,
			fCheckBounds = 1 << 2,
			fToLower     = 1 << 3,
			fToUpper     = 1 << 4
		};

		CallbackParam() = default;

		CallbackParam(KStringView sNames, KStringViewZ sMissingArgs, uint16_t fFlags)
		: m_sNames       ( sNames          )
		, m_sMissingArgs ( sMissingArgs    )
		, m_iFlags       ( fFlags          )
		{
		}

		CallbackParam(KStringView sNames, uint16_t fFlags, uint16_t iMinArgs, KStringViewZ sMissingParms, CallbackN Func)
		: m_Callback     ( std::move(Func) )
		, m_sNames       ( sNames          )
		, m_sMissingArgs ( sMissingParms   )
		, m_iMinArgs     ( iMinArgs        )
		, m_iFlags       ( fFlags          )
		{
		}

		CallbackN    m_Callback;
		KStringView  m_sNames;
		KStringViewZ m_sMissingArgs;
		KStringView  m_sHelp;
		int64_t      m_iLowerBound  { 0 };
		int64_t      m_iUpperBound  { 0 };
		uint16_t     m_iMinArgs     { 0 };
		uint16_t     m_iMaxArgs     { 65535  };
		uint16_t     m_iFlags       { fNone  };
		ArgTypes     m_ArgType      { String };
		mutable bool m_bUsed        { false  };

		bool         IsRequired()  const { return m_iFlags & fIsRequired;   }
		bool         IsCommand()   const { return m_iFlags & fIsCommand;    }
		bool         CheckBounds() const { return m_iFlags & fCheckBounds;  }
		bool         ToLower()     const { return m_iFlags & fToLower;      }
		bool         ToUpper()     const { return m_iFlags & fToUpper;      }

	}; // CallbackParam

//----------
public:
//----------

	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	class OptionalParm : private CallbackParam
	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{
	public:
		OptionalParm(KOptions& base, KStringView sOption, KStringViewZ sArgDescription, bool bIsCommand);
		~OptionalParm();

		/// set the minimum count of expected arguments (default 0)
		OptionalParm& MinArgs(uint16_t iMinArgs)  { m_iMinArgs = iMinArgs;  return *this; }
		/// set the maximum count of expected arguments (default 65535)
		OptionalParm& MaxArgs(uint16_t iMaxArgs)  { m_iMaxArgs = iMaxArgs;  return *this; }
		/// set the type of the expected argument: Integer,Unsigned,Float,Boolean,String,File,Path,Directory,Email,URL
		OptionalParm& Type(ArgTypes Type)         { m_ArgType  = Type;      return *this; }
		/// set the range for integer and float arguments, or the size for string arguments
		OptionalParm& Range(int64_t iLowerBound, int64_t iUpperBound);
		/// make argument required to be used
		OptionalParm& Required()                  { m_iFlags |= fIsRequired;return *this; }
		/// convert argument to lower case
		OptionalParm& ToLower()                   { m_iFlags |= fToLower;   return *this; }
		/// convert argument to upper case
		OptionalParm& ToUpper()                   { m_iFlags |= fToUpper;   return *this; }
		/// set the callback for the parameter as a function void(KOptions::ArgList&)
		OptionalParm& Callback(CallbackN Func);
		/// set the callback for the parameter as a function void(KStringViewZ)
		OptionalParm& Callback(Callback1 Func);
		/// set the callback for the parameter as a function void()
		OptionalParm& Callback(Callback0 Func);
		OptionalParm& operator()(CallbackN Func)  { return Callback(std::move(Func)); }
		OptionalParm& operator()(Callback1 Func)  { return Callback(std::move(Func)); }
		OptionalParm& operator()(Callback0 Func)  { return Callback(std::move(Func)); }
		/// set the text shown in the help for this parameter
		OptionalParm& Help(KStringView sHelp);

	private:
		KOptions*    m_base;

	}; // OptionalParm

	/// Start definition of a new option. Have it follow by any chained count of methods of OptionalParms, like Option("clear").Help("clear all data").Callback([&](){ RunClear() });
	OptionalParm Option(KStringView sOption, KStringViewZ sArgDescription = KStringViewZ{});
	/// Start definition of a new command. Have it follow by any chained count of methods of OptionalParms, like Command("clear").Help("clear all data").Callback([&](){ RunClear() });
	OptionalParm Command(KStringView sCommand, KStringViewZ sArgDescription = KStringViewZ{});

	/// Register a CallbackParam (typically done by the destructor of CallbackParam..)
	void Register(CallbackParam OptionOrCommand);

	/// Deprecated, use the Option() method -
	/// Register a callback function for occurences of "-sOption" with no additional args
	void RegisterOption(KStringView sOption, Callback0 Function);

	/// Deprecated, use the Command() method -
	/// Register a callback function for occurences of "sCommand" with no additional args
	void RegisterCommand(KStringView sCommand, Callback0 Function);

	/// Deprecated, use the Option() method -
	/// Register a callback function for occurences of "-sOption" with exactly one additional arg.
	/// The sMissingParm string is output if there is no additional arg.
	void RegisterOption(KStringView sOption, KStringViewZ sMissingParm, Callback1 Function);

	/// Deprecated, use the Command() method -
	/// Register a callback function for occurences of "sCommand" with exactly one additional arg.
	/// The sMissingParm string is output if there is no additional arg.
	void RegisterCommand(KStringView sCommand, KStringViewZ sMissingParm, Callback1 Function);

	/// Deprecated, use the Option() method -
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

	/// Register an array of KStringViews as help output, if you do not want to use the automatically generated help
	template<std::size_t COUNT>
	void RegisterHelp(const KStringView (&sHelp)[COUNT])
	{
		m_sHelp = sHelp;
		m_iHelpSize = COUNT;
	}

	/// Output the registered help message, or automatically generate a help message
	void Help(KOutStream& out);

	/// Get the string representation of the current Argument
	KStringViewZ GetCurrentArg() const
	{
		return m_sCurrentArg;
	}

	/// Get the current output stream while parsing commands/args
	KOutStream& GetCurrentOutputStream();

	/// Returns true if we are executed inside a CGI server
	static bool IsCGIEnvironment();

	/// Returns arg[0] / the path and name of the called executable
	KStringViewZ GetProgramPath() const;

	/// Returns basename of arg[0] / the name of the called executable
	KStringView GetProgramName() const;

	/// Returns brief description of the called executable
	KStringView GetBriefDescription() const { return m_sBriefDescription; }

//----------
protected:
//----------

	/// Set error string and throw or return with error int
	int SetError(KStringViewZ sError, KOutStream& out = KOut);

	bool m_bThrow { false };

//----------
private:
//----------

	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	class CLIParms
	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{

	public:

		//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
		struct Arg_t
		//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
		{
			Arg_t() = default;
			Arg_t(KStringViewZ sArg_);

			bool IsOption() const { return iDashes; }
			KStringViewZ Dashes() const;

			KStringViewZ sArg;
			bool bConsumed { false };

		private:

			static constexpr KStringViewZ s_sDoubleDash = "--";

			uint8_t iDashes { 0 };

		}; // Arg_t

		using ArgVec   = std::vector<Arg_t>;
		using iterator = ArgVec::iterator;
		using const_iterator = ArgVec::const_iterator;

		CLIParms() = default;
		CLIParms(int argc, char const* const* argv)
		{
			Create(argc, argv);
		}
		CLIParms(const std::vector<KStringViewZ>& parms)
		{
			Create(parms);
		}


		void Create(int argc, char const* const* argv);
		void Create(const std::vector<KStringViewZ>& parms);

		size_t size() const  { return m_ArgVec.size();  }
		size_t empty() const { return m_ArgVec.empty(); }
		iterator begin()     { return m_ArgVec.begin(); }
		iterator end()       { return m_ArgVec.end();   }
		const_iterator begin() const { return m_ArgVec.begin(); }
		const_iterator end()   const { return m_ArgVec.end();   }
		void clear()         { m_ArgVec.clear();        }

		KStringViewZ GetProgramPath() const;
		KStringView GetProgramName() const;

		ArgVec m_ArgVec;
		KStringViewZ m_sProgramPathName;

	}; // CLIParms

	std::vector<CallbackParam> m_Callbacks;

	using CommandLookup = KUnorderedMap<KStringView, std::size_t>;

	KStringViewZ ModifyArgument(KStringViewZ sArg, const CallbackParam* Callback);
	KString BadBoundsReason(ArgTypes Type, KStringView sParm, int64_t iMinBound, int64_t iMaxBound) const;
	bool ValidBounds(ArgTypes Type, KStringView sParm, int64_t iMinBound, int64_t iMaxBound) const;
	KString BadArgReason(ArgTypes Type, KStringView sParm) const;
	bool ValidArgType(ArgTypes Type, KStringViewZ sParm) const;
	const CallbackParam* FindParam(KStringView sName, bool bIsOption) const;
	int Execute(CLIParms Parms, KOutStream& out);
	int Evaluate(const CLIParms& Parms, KOutStream& out);
	void BuildHelp(KOutStream& out) const;

	// a forward_list, other than a vector, keeps all elements in place when
	// adding more elements, which makes it perfect for the general strategy
	// of KOptions to use string views for parameters (which are unbuffered
	// when coming directly from the CLI, this buffer is only for CGI and
	// ini file parms)
	std::forward_list<KString> m_ParmBuffer;

	KString            m_sProgramPathName;
	KString            m_sBriefDescription;
	KStringView        m_sSeparator { "::" };
	CommandLookup      m_Commands;
	CommandLookup      m_Options;
	KStringViewZ       m_sCurrentArg;
	KOutStream*        m_CurrentOutputStream { nullptr };
	const KStringView* m_sHelp { nullptr };
	size_t             m_iHelpSize { 0 };
	std::size_t        m_iMaxHelpRowWidth { 100 };
	std::size_t        m_iWrappedHelpIndent { 1 };
	bool               m_bEmptyParmsIsError { true };
	bool               m_bHaveOptions  { false };
	bool               m_bHaveCommands { false };

}; // KOptions

} // end of namespace dekaf2









