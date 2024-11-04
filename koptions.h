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
#include "kstringutils.h"
#include "kwriter.h"
#include "kstack.h"
#include "kexception.h"
#include "kassociative.h"
#include "kpersist.h"
#include <functional>
#include <vector>
#include <array>


/// @file koptions.h
/// KOption offers multiple approaches for option parsing, which have evolved over time. These approaches can be mixed.
///
/// In general, any input argument is either an option (if it starts with one or two dashes) or a command (if it does not
/// start with dashes). Options and commands can have arguments, that is, arguments following an option until the next 
/// argument that starts with dashes). How many of those arguments will be consumed by a single option or command is
/// determined by the callback type and a possible min/max range declaration.
///
/// Additional constructor arguments define whether empty input arguments are an error by itself, and if an exception will
/// be thrown in case of an error.
///
/// 1. The original approach requires the declaration of a callback function for each option or command, in which the
/// arguments (if any) are evaluated and assigned, and possibly other code is executed.
/// After all callbacks are setup the input arguments are parsed and the respective callbacks executed.
/// Typical code for this approach looks like:
/// @code
/// // instantiate the KOptions object
/// KOptions Options;
/// // add options with their callbacks
/// Options.Option("f,filename <name>").Help("the name for the file")     .Callback([&](KStringViewZ sArg) { m_sFilename = sArg;          });
/// Options.Option("m,max <count>")    .Help("the maximum count of words").Callback([&](KStringViewZ sArg) { m_iCount    = sArg.UInt64(); });
/// // finally parse the arguments and call the callbacks
/// int iResult = Options.Parse(argc, argv);
/// if (iResult != 0) return iResult;
/// @endcode
/// Three different callback signatures exist, for callbacks without arguments, for callbacks with one argument, or for callbacks with multiple arguments:
/// @code
/// Callback([](){});
/// Callback([](KStringViewZ sArg){});
/// Callback([](KOptions::ArgList& Args){});
/// @endcode
/// The callback with multiple arguments is called with args in a KStack object. After return, all popped arguments will be counted as consumed, all remaining
/// will either be parsed for being commands with a callback, or, if existing, an unknown command callback will be called with them. The latter is typically
/// used for unspecified counts of input arguments like e.g. filenames.
///
/// A number of requirements can be set for the expected arguments, like signed/unsigned/string/filename etc.:
/// @code
/// Options.Option("timeout <sec>")
///        .Help("timeout in seconds")
///        .Type(KOptions::Unsigned)
///        .Range(1, 60)
///        .Callback([&](KStringViewZ sArg) { m_iTimeout = sArg.UInt64(); });
/// @endcode
///
/// 2. Simplifications for the above syntax exist: The help text can be appended to the opion names, and single variables can be set with the argument value,
/// automatically converted into the variable type.
/// @code
/// Options.Option("f,filename <name> : the name for the file").Set(m_sFilename);
/// @endcode
///
/// 3. A third approach to argument parsing is the so-called ad-hoc parsing. In this approach (which can also be mixed with the previous ones)
/// the input arguments get parsed first, and then get requested by query functions. Using the call operator and some template magic let
/// this approach look very natural:
/// @code
/// // instantiate the KOptions object and parse all input arguments
/// KOptions Options(false, argc, argv);
/// // request options and values
/// KString  sFilename = Options("f,filename <name> : the name for the file");
/// // declare a default value of 1000 if the option is missing
/// uint16_t iCount    = Options("m,max <count>     : the maximum count of words", 1000);
/// bool     bReverse  = Options("r,reverse         : count from end of file", false);
/// // you can also explicitly request a value type
/// auto     iExpect   = Options("e                 : expected average, 1).Int16();
/// // get all remaining arguments (should be called last..)
/// auto ArgVec        = Options.GetUnknownCommands();
/// // finally check if all arguments were evaluated
/// if (Options.Check()) return false;
/// @endcode
/// All options without a default value are required options.
///
/// For all approaches, a help text will be auto generated and auto formatted, adapted to the output terminal size. Missing
/// arguments for options or commands will be annotated with the relevant part of the help.
/// It is not necessary to use white space formatting as in the above examples - all white space will be normalized,
/// above it was used to ease reading.



DEKAF2_NAMESPACE_BEGIN

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Option parsing for commandline, ini files, and CGI environments
class DEKAF2_PUBLIC KOptions
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

	/// ctor that immediately parses from the cli (creating ad hoc options)
	KOptions (bool bEmptyParmsIsError, int argc, char const* const* argv, KStringView sCliDebugTo = KLog::STDOUT, bool bThrow = false);

	/// dtor, may call Check(), but will not throw
	~KOptions();

	/// set a brief description of the program, will appear in first line of generated help
	KOptions& SetBriefDescription(KString sBrief)                     { m_HelpParams.sBriefDescription       = std::move(sBrief);             return *this; }

	/// set an additional description at the end of the automatic "usage:" string
	KOptions& SetAdditionalArgDescription(KString sAdditionalArgDesc) { m_HelpParams.sAdditionalArgDesc      = std::move(sAdditionalArgDesc); return *this; }

	/// set an additional help text at the end of the generated help
	KOptions& SetAdditionalHelp(KString sAdditionalHelp)              { m_HelpParams.sAdditionalHelp         = std::move(sAdditionalHelp);    return *this; }

	/// set the separator style for the generated help - default is ::
	KOptions& SetHelpSeparator(KString sSeparator)                    { m_HelpParams.sSeparator              = std::move(sSeparator);         return *this; }

	/// set max generated help width in characters if terminal size is unknown, default = 100
	KOptions& SetMaxHelpWidth(uint16_t iMaxWidth)                     { m_HelpParams.iMaxHelpRowWidth        = iMaxWidth;                     return *this; }

	/// set indent for wrapped help lines, default 1
	KOptions& SetWrappedHelpIndent(uint16_t iIndent)                  { m_HelpParams.iWrappedHelpIndent      = iIndent;                       return *this; }

	/// calculate column width for names per section, or same for all (default = false)
	KOptions& SetSpacingPerSection(bool bSpacingPerSection)           { m_HelpParams.bSpacingPerSection      = bSpacingPerSection;            return *this; }

	/// write an empty line between all options? (default = false)
	KOptions& SetLinefeedBetweenOptions(bool bLinefeedBetweenOptions) { m_HelpParams.bLinefeedBetweenOptions = bLinefeedBetweenOptions;       return *this; }

	/// throw on errors or not?
	KOptions& Throw(bool bYesNo = true)                               { m_bThrow = bYesNo;                                                    return *this; }

	/// allow unknown options to be parsed like regular options? (default at construction = false). If no regular options
	/// had been defined at time of parsing, ad-hoc args are automatically enabled. Such args then need to be fetched
	/// with Get() or the call operator()
	KOptions& AllowAdHocArgs(bool bYesNo = true)                      { m_bAllowAdHocArgs = bYesNo;                                           return *this; }

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

	/// Verify that all given parms have been consumed, and that all required parms were present.
	/// This method is called automatically after parsing when no ad-hoc parms were generated,
	/// but if there are ad-hoc parms it has to be called after all .Get() calls to request those. It will
	/// also be called automatically at destruction of KOptions, but that may be a bit late..
	/// May throw if KOptions may throw.
	/// @returns true if all parms have been consumed.
	bool Check(KOutStream& out = KOut);

	using ArgList   = KStack<KStringViewZ>;
	using Callback0 = std::function<void()>;
	using Callback1 = std::function<void(KStringViewZ)>;
	using CallbackN = std::function<void(ArgList&)>;

	/// Selects the type of an argument, used in value checking
	enum ArgTypes
	{
		Integer,   ///< signed integer
		Unsigned,  ///< unsigned integer
		Float,     ///< float
		Boolean,   ///< boolean
		String,    ///< string (the default)
		File,      ///< file must exist
		Directory, ///< directory must exist, with or without trailing slash
		Path,      ///< path component of pathname must exist
#ifdef DEKAF2_HAS_UNIX_SOCKETS
		Socket,    ///< socket pathname, must exist (and be a socket)
#endif
		Email,     ///< email address
		URL,       ///< URL
	};

//----------
private:
//----------

	static constexpr KStringView UNKNOWN_ARG { "!" };

	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	class DEKAF2_PUBLIC CallbackParam
	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{
	public:

		enum Flag
		{
			fNone        = 0,
			fIsRequired  = 1 <<  0,
			fIsCommand   = 1 <<  1,
			fCheckBounds = 1 <<  2,
			fToLower     = 1 <<  3,
			fToUpper     = 1 <<  4,
			fIsHidden    = 1 <<  5,
			fIsSection   = 1 <<  6,
			fIsUnknown   = 1 <<  7,
			fIsFinal     = 1 <<  8,
			fIsStop      = 1 <<  9,
			fIsAdHoc     = 1 << 10
		};

		CallbackParam() = default;
		CallbackParam(KStringView sNames, KStringView sMissingArgs, uint16_t fFlags, uint16_t iMinArgs, uint16_t iMaxArgs, CallbackN Func);
		CallbackParam(KStringView sNames, KStringView sMissingArgs, uint16_t fFlags, uint16_t iMinArgs = 0, CallbackN Func = CallbackN{})
		: CallbackParam(sNames, sMissingArgs, fFlags, iMinArgs, 65535, std::move(Func)) {}

		CallbackN    m_Callback;
		KStringView  m_sNames;
		KStringView  m_sMissingArgs;
		KStringView  m_sHelp;
		mutable std::vector<KStringViewZ> m_Args;
		int64_t      m_iLowerBound      {      0 };
		int64_t      m_iUpperBound      {      0 };
		uint16_t     m_iMinArgs         {      0 };
		uint16_t     m_iMaxArgs         {  65535 };
		uint16_t     m_iFlags           {  fNone };
		uint16_t     m_iHelpRank        {      0 };
		ArgTypes     m_ArgType          { String };
		mutable bool m_bUsed            {  false };
		mutable bool m_bArgsAreDefaults {  false };

		/// returns true if this parameter is required
		DEKAF2_NODISCARD
		bool         IsRequired()  const { return m_iFlags & fIsRequired;   }
		/// returns true if this parameter is an option, not a command
		DEKAF2_NODISCARD
		bool         IsOption()    const { return IsCommand() == false;     }
		/// returns true if this parameter is a command, not an option
		DEKAF2_NODISCARD
		bool         IsCommand()   const { return m_iFlags & fIsCommand;    }
		/// returns true if this parameter shall be hidden from auto-documentation
		DEKAF2_NODISCARD
		bool         IsHidden()    const { return m_iFlags & fIsHidden;     }
		/// returns true if this parameter is a section break for the auto-documentation
		DEKAF2_NODISCARD
		bool         IsSection()   const { return m_iFlags & fIsSection;    }
		/// returns true if this parameter is an "unknown" catchall, either for options or commands
		DEKAF2_NODISCARD
		bool         IsUnknown()   const { return m_iFlags & fIsUnknown;    }
		/// returns true if this parameter ends the execution of a program immediately
		DEKAF2_NODISCARD
		bool         IsFinal()     const { return m_iFlags & fIsFinal;      }
		/// returns true if this parameter ends the execution of a program after all args are parsed
		DEKAF2_NODISCARD
		bool         IsStop()      const { return m_iFlags & fIsStop;       }
		/// returns true if this parameter was added ad hoc during cli parsing
		DEKAF2_NODISCARD
		bool         IsAdHoc()     const { return m_iFlags & fIsAdHoc;      }
		/// returns true if the boundaries of this parameter shall be checked
		DEKAF2_NODISCARD
		bool         CheckBounds() const { return m_iFlags & fCheckBounds;  }
		/// returns true if the string shall be converted to lowercase
		DEKAF2_NODISCARD
		bool         ToLower()     const { return m_iFlags & fToLower;      }
		/// returns true if the string shall be converted to uppercase
		DEKAF2_NODISCARD
		bool         ToUpper()     const { return m_iFlags & fToUpper;      }

	}; // CallbackParam

	friend class OptionalParm;

//----------
public:
//----------

	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	class DEKAF2_PUBLIC OptionalParm : private CallbackParam
	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{
		friend class KOptions;

	protected:

		// we make the ctor protected to ensure that all passed strings are already persisted
		OptionalParm(KOptions& base, KStringView sOption, KStringView sArgDescription, bool bIsCommand);

	public:

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
		/// hide this parameter from automatic help text generation
		OptionalParm& Hidden()                    { m_iFlags |= fIsHidden;  return *this; }
		/// when at least one parsed argument has Stop() set, after parsing the args the program is terminated (with success)
		OptionalParm& Stop()                      { m_iFlags |= fIsStop;    return *this; }
		/// after parsing _this_ arg the program is terminated (with success if this _is_ the final arg)
		OptionalParm& Final()                     { m_iFlags |= fIsFinal;   return *this; }
		/// mark this arg as an ad hoc arg (never do this manually, the parser does it internally)
		OptionalParm& AdHoc()                     { m_iFlags |= fIsAdHoc;   return *this; }
		/// adds given flag(s)
		OptionalParm& AddFlag(Flag flag)          { m_iFlags |= flag;       return *this; }
		/// set the callback for the parameter as a function void(KOptions::ArgList&)
		OptionalParm& Callback(CallbackN Func);
		/// set the callback for the parameter as a function void(KStringViewZ)
		OptionalParm& Callback(Callback1 Func);
		/// set the callback for the parameter as a function void()
		OptionalParm& Callback(Callback0 Func);
		OptionalParm& operator()(CallbackN Func)  { return Callback(std::move(Func)); }
		OptionalParm& operator()(Callback1 Func)  { return Callback(std::move(Func)); }
		OptionalParm& operator()(Callback0 Func)  { return Callback(std::move(Func)); }

		/// set any value that can be converted from the input parameter string
		template<typename T>
		OptionalParm& Set(T& Value)
		{
			return Callback([&Value](KStringViewZ sValue)
			{
				kFromString(Value, sValue);
			});
		}

		/// set any value that can be converted from the provided parameter
		template<typename T>
		OptionalParm& Set(T& Value, T SetValue)
		{
			return Callback([&Value, SetValue]()
			{
				Value = std::move(SetValue);
			});
		}

		/// set any value that can be converted from the provided parameter string
		template<typename T>
		OptionalParm& Set(T& Value, KStringView sValue)
		{
			return Callback([&Value, sValue]()
			{
				kFromString(Value, sValue);
			});
		}

		/// set the text shown in the help for this parameter, iHelpRank controls the order of help output shown, 0 = highest
		template<class String>
		OptionalParm& Help(String&& sHelp, uint16_t iHelpRank = 0)
		{
			return IntHelp(m_base->m_Strings.Persist(std::forward<String>(sHelp)), iHelpRank);
		}
		/// insert a section break in front of this parameter, with sSection as the section break title
		template<class String>
		OptionalParm& Section(String&& sSection)
		{
			return IntSection(m_base->m_Strings.Persist(std::forward<String>(sSection)));
		}

	private:

		OptionalParm& IntHelp(KStringView sHelp, uint16_t iHelpRank);
		OptionalParm& IntSection(KStringView sSection);

		KOptions*    m_base;

	}; // OptionalParm

	/// Start definition of a new option. Have it follow by any chained count of methods of OptionalParms, like Option("clear").Help("clear all data").Callback([&](){ RunClear() });
	template<class String1>
	DEKAF2_NODISCARD
	OptionalParm Option(String1&& sOption)
	{
		return IntOptionOrCommand(m_Strings.Persist(std::forward<String1>(sOption)), "", false);
	}
	/// Start definition of a new command. Have it follow by any chained count of methods of OptionalParms, like Command("clear").Help("clear all data").Callback([&](){ RunClear() });
	template<class String1>
	DEKAF2_NODISCARD
	OptionalParm Command(String1&& sCommand)
	{
		return IntOptionOrCommand(m_Strings.Persist(std::forward<String1>(sCommand)), "", true);
	}

	/// Start definition of a new option. Have it follow by any chained count of methods of OptionalParms, like Option("clear").Help("clear all data").Callback([&](){ RunClear() });
	template<class String1, class String2>
	DEKAF2_NODISCARD
	OptionalParm Option(String1&& sOption, String2&& sArgDescription)
	{
		return IntOptionOrCommand(m_Strings.Persist(std::forward<String1>(sOption)), m_Strings.Persist(std::forward<String2>(sArgDescription)), false);
	}
	/// Start definition of a new command. Have it follow by any chained count of methods of OptionalParms, like Command("clear").Help("clear all data").Callback([&](){ RunClear() });
	template<class String1, class String2>
	DEKAF2_NODISCARD
	OptionalParm Command(String1&& sCommand, String2&& sArgDescription)
	{
		return IntOptionOrCommand(m_Strings.Persist(std::forward<String1>(sCommand)), m_Strings.Persist(std::forward<String2>(sArgDescription)), true);
	}

	/// Register a callback function for unhandled options
	void UnknownOption(CallbackN Function);

	/// Register a callback function for unhandled commands
	void UnknownCommand(CallbackN Function);

	// ========================== deprecated/legacy interface ============================

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

	/// Deprecated, use the Command() method -
	/// Register a callback function for occurences of "sCommand" with an arbitrary, but defined minimal amount of additional args
	/// The sMissingParms string is output if there are less than iMinArgs args.
	void RegisterCommand(KStringView sCommand, uint16_t iMinArgs, KStringViewZ sMissingParms, CallbackN Function);

	/// Deprecated, use the UnknownOption() method -
	/// Register a callback function for unhandled options
	void RegisterUnknownOption(CallbackN Function) { UnknownOption(std::move(Function)); }

	/// Deprecated, use the UnknownCommand() method -
	/// Register a callback function for unhandled commands
	void RegisterUnknownCommand(CallbackN Function) { UnknownCommand(std::move(Function)); }

	// ===================== deprecated/legacy interface until here =======================

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
	DEKAF2_NODISCARD
	KStringViewZ GetCurrentArg() const
	{
		return m_sCurrentArg;
	}

	/// Get the current output stream while parsing commands/args
	DEKAF2_NODISCARD
	KOutStream& GetCurrentOutputStream() const;

	/// Get the current output stream width while parsing commands/args
	DEKAF2_NODISCARD
	uint16_t GetCurrentOutputStreamWidth() const;

	/// Returns true if we are executed inside a CGI server
	DEKAF2_NODISCARD
	static bool IsCGIEnvironment();

	/// Returns arg[0] / the path and name of the called executable
	DEKAF2_NODISCARD
	const KString& GetProgramPath() const { return m_HelpParams.GetProgramPath(); }

	/// Returns basename of arg[0] / the name of the called executable
	DEKAF2_NODISCARD
	KStringView GetProgramName() const { return m_HelpParams.GetProgramName(); }

	/// Returns brief description of the called executable
	DEKAF2_NODISCARD
	const KString& GetBriefDescription() const { return m_HelpParams.GetBriefDescription(); }

	/// Terminate app immediately? (check after parsing)
	DEKAF2_NODISCARD
	bool Terminate() const { return m_bStopAppAfterParsing; }

	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	/// helper to convert arguments into different data types
	class Values
	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{
	public:
		/// construct Values with data from parameter parsing
		Values(KOptions& base, const std::vector<KStringViewZ>& Params, bool bFound) noexcept
		: m_base(base), m_Params(Params), m_bFound(bFound) {}
		/// dtor to push unused parameters into the unknown commands list
		~Values();
		/// Get the values associated to an option name as a vector<KStringViewZ>
		DEKAF2_NODISCARD
		std::vector<KStringViewZ> Vector() noexcept;
		/// Get the value associated to an option name as string
		DEKAF2_NODISCARD
		KStringViewZ String() const noexcept;
		/// Get the value associated to an option name as C string
		DEKAF2_NODISCARD
		const char* c_str()   const noexcept { return String().c_str();  }
		/// Get the value associated to an option name as signed integer
		DEKAF2_NODISCARD
		int64_t Int64()       const noexcept { return String().Int64();  }
		/// Get the value associated to an option name as unsigned integer
		DEKAF2_NODISCARD
		uint64_t UInt64()     const noexcept { return String().UInt64(); }
		/// Get the value associated to an option name as signed integer
		DEKAF2_NODISCARD
		int32_t Int32()       const noexcept { return String().Int32();  }
		/// Get the value associated to an option name as unsigned integer
		DEKAF2_NODISCARD
		uint32_t UInt32()     const noexcept { return String().UInt32(); }
		/// Get the value associated to an option name as signed integer
		DEKAF2_NODISCARD
		int16_t Int16()       const noexcept { return String().Int16();  }
		/// Get the value associated to an option name as unsigned integer
		DEKAF2_NODISCARD
		uint16_t UInt16()     const noexcept { return String().UInt16(); }
		/// Get the value associated to an option name as floating point value
		DEKAF2_NODISCARD
		float Float()         const noexcept { return String().Float();  }
		/// Get the value associated to an option name as floating point value
		DEKAF2_NODISCARD
		double Double()       const noexcept { return String().Double(); }
		/// Get a boolean - the option name does not have/needs a value
		/// - if the option is present the value is true, else false
		DEKAF2_NODISCARD
		bool Bool()           const noexcept;
		/// Get the value associated to an option name as a duration
		template < typename DurationType = KDuration >
		DEKAF2_NODISCARD
		DurationType Duration() const noexcept { DurationType Value; kFromString(Value, String()); return Value; }
		/// Get the count of arguments for this option
		std::size_t size()    const noexcept { return m_Params.size();   }
		/// Is the argument container empty?
		bool empty()          const noexcept { return m_Params.empty();  }
		/// Was this option used in the cli params?
		bool Exists()         const noexcept { return m_bFound;          }

		// all C++ string(view) types
		template < typename ValueType,
			typename std::enable_if <std::is_assignable<ValueType, KStringViewZ>::value, int >::type = 0 >
		operator    ValueType () const noexcept { return String(); }

		// all duration types
		template < typename ValueType,
			typename std::enable_if <detail::is_duration<ValueType>::value, int >::type = 0 >
		operator    ValueType () const noexcept { return Duration<ValueType>(); }

		// all unsigned integers
		template < typename ValueType,
			typename std::enable_if <
				std::conjunction < std::is_integral<ValueType>,
								   std::is_unsigned<ValueType>,
					std::negation< std::is_same    <ValueType, std::remove_cv<bool>::type>    >,
					std::negation< std::is_same    <ValueType, char>                          >,
					std::negation< std::is_pointer <ValueType>                                >,
					std::negation< std::is_same    <ValueType, std::nullptr_t>                >
				>::value
			, int >::type = 0>
		operator    ValueType () const noexcept { return static_cast<ValueType>(UInt64()); }

		// all signed integers
		template < typename ValueType,
			typename std::enable_if <
				std::conjunction < std::is_integral<ValueType>,
					std::negation< std::is_unsigned<ValueType>                                >,
					std::negation< std::is_same    <ValueType, std::remove_cv<bool>::type>    >,
					std::negation< std::is_same    <ValueType, char>                          >,
					std::negation< std::is_pointer <ValueType>                                >,
					std::negation< std::is_same    <ValueType, std::nullptr_t>                >
				>::value
			, int >::type = 0>
		operator    ValueType () const noexcept { return static_cast<ValueType>(Int64()); }

		// bool
		template < typename ValueType,
			typename std::enable_if <
				std::is_same   <ValueType, std::remove_cv<bool>::type>::value
			, int >::type = 0>
		operator    ValueType () const noexcept { return Bool(); }

		// floating point types
		template < typename ValueType,
			typename std::enable_if <
				std::is_floating_point<ValueType>::value
			, int >::type = 0>
		operator    ValueType () const noexcept { return Double(); }

		// the vector type
		operator std::vector<KStringViewZ> () noexcept { return Vector(); }

		/// index access throws if out of range
		KStringViewZ operator [] (std::size_t index) const;

		/// get a begin( ) iterator on the list of parameters
		std::vector<KStringViewZ>::const_iterator begin() const noexcept { return m_Params.begin(); }
		/// get an end( ) iterator on the list of parameters
		std::vector<KStringViewZ>::const_iterator end()   const noexcept { return m_Params.end();   }

	private:
		KOptions&                        m_base;
		const std::vector<KStringViewZ>& m_Params;
		mutable std::size_t              m_iConsumed        { 0 };
		bool                             m_bFound       { false };

	}; // Values

	/// returns parameters belonging to sOptionName (after parsing the arguments). Throws if option is not found.
	DEKAF2_NODISCARD
	Values Get(KStringView sOptionName);

	/// call operator returns associated parameters. Throws if option is not found.
	DEKAF2_NODISCARD
	Values operator()(KStringView sOptionName) { return Get(sOptionName); }

	/// returns parameters belonging to sOptionName (after parsing the arguments). Returns default value if not found.
	DEKAF2_NODISCARD
	Values Get(KStringView sOptionName, const KStringViewZ& sDefaultValue) noexcept;

	/// returns parameters belonging to sOptionName (after parsing the arguments). Returns default value if not found.
	template<typename T,
	         typename std::enable_if<detail::is_kstringviewz_assignable<const T&>::value, int>::type = 0>
	DEKAF2_NODISCARD
	Values Get(KStringView sOptionName, const T& sDefaultValue) noexcept
	{ return Get(sOptionName, KStringViewZ(sDefaultValue)); }

	/// returns parameters belonging to sOptionName (after parsing the arguments). Returns default value if not found.
	template<typename T,
	         typename std::enable_if<!detail::is_kstringviewz_assignable<const T&>::value, int>::type = 0>
	DEKAF2_NODISCARD
	Values Get(KStringView sOptionName, const T& DefaultValue) noexcept
	{ return Get(sOptionName, KString::to_string(DefaultValue)); }

	/// returns parameters belonging to sOptionName (after parsing the arguments). Returns default value if not found.
	DEKAF2_NODISCARD
	Values Get(KStringView sOptionName, const std::vector<KStringViewZ>& DefaultValues) noexcept;

	/// call operator returns associated parameters. Returns default value if not found.
	DEKAF2_NODISCARD
	Values operator()(KStringView sOptionName, const KStringViewZ& sDefaultValue) noexcept
	{ return Get(sOptionName, sDefaultValue); }

	/// call operator returns associated parameters. Returns default value if not found.
	template<typename T>
	DEKAF2_NODISCARD
	Values operator()(KStringView sOptionName, const T& DefaultValue) noexcept 
	{ return Get(sOptionName, DefaultValue); }

	/// call operator returns associated parameters. Returns default value if not found.
	DEKAF2_NODISCARD
	Values operator()(KStringView sOptionName, const std::vector<KStringViewZ>& DefaultValues) noexcept 
	{ return Get(sOptionName, DefaultValues); }

	/// returns all commands that were not covered by any declared command in current parsing. For ad-hoc parsing
	/// call after all other options were checked. Can only be called once.
	std::vector<KStringViewZ> GetUnknownCommands() { return std::move(m_UnknownCommands); }

//----------
protected:
//----------

	/// Set error string and throw or return with error int
	int SetError(KStringViewZ sError, KOutStream& out = KOut);

	bool m_bThrow { false };

//----------
private:
//----------

	using PersistedStrings = KPersistStrings<KString, false>;

	static constexpr KStringView::size_type iMaxAdHocOptionLength { 200 };

	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	class DEKAF2_PRIVATE CLIParms
	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{

	//----------
	public:
	//----------

		//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
		struct DEKAF2_PRIVATE Arg_t
		//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
		{
			Arg_t() = default;
			Arg_t(KStringViewZ sArg_);
			Arg_t(KStringViewZ sArg_, uint8_t iDashes_)
			: sArg(sArg_)
			, iDashes(iDashes_)
			{}

			DEKAF2_NODISCARD uint8_t      DashCount() const { return iDashes; }
			DEKAF2_NODISCARD bool         IsOption () const { return DashCount() > 0; }
			DEKAF2_NODISCARD KStringViewZ Dashes   () const;

			KStringViewZ sArg;
			bool         bConsumed { false };

		//----------
		private:
		//----------

			static constexpr KStringViewZ s_sDoubleDash = "--";

			uint8_t      iDashes { 0 };

		}; // Arg_t

		using ArgVec         = std::vector<Arg_t>;
		using iterator       = ArgVec::iterator;
		using const_iterator = ArgVec::const_iterator;

		CLIParms() = default;
		CLIParms(int argc, char const* const* argv, PersistedStrings& Strings) { Create(argc, argv, Strings); }
		CLIParms(const std::vector<KStringViewZ>& parms, PersistedStrings& Strings) { Create(parms, Strings); }

		                 void           Create(KStringViewZ sArg, PersistedStrings& Strings);
		                 void           Create(const std::vector<KStringViewZ>& parms, PersistedStrings& Strings);
		                 void           Create(int argc, char const* const* argv, PersistedStrings& Strings);

		DEKAF2_NODISCARD size_t         size()  const { return m_ArgVec.size();  }
		DEKAF2_NODISCARD size_t         empty() const { return m_ArgVec.empty(); }
		DEKAF2_NODISCARD iterator       begin()       { return m_ArgVec.begin(); }
		DEKAF2_NODISCARD iterator       end()         { return m_ArgVec.end();   }
		DEKAF2_NODISCARD const_iterator begin() const { return m_ArgVec.begin(); }
		DEKAF2_NODISCARD const_iterator end()   const { return m_ArgVec.end();   }
		                 void           clear()       { m_ArgVec.clear();        }

		DEKAF2_NODISCARD KStringViewZ   GetProgramPath() const;
		DEKAF2_NODISCARD KStringView    GetProgramName() const;

		                 iterator       ExpandToSingleCharArgs(iterator it, const std::vector<KStringViewZ>& SplittedArgs);

		ArgVec         m_ArgVec;
		KStringViewZ   m_sProgramPathName;

	}; // CLIParms

	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	class DEKAF2_PRIVATE HelpFormatter
	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{

	//----------
	public:
	//----------

		//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
		struct DEKAF2_PRIVATE HelpParams
		//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
		{
			DEKAF2_NODISCARD KStringView    GetProgramName()      const;
			DEKAF2_NODISCARD const KString& GetProgramPath()      const { return sProgramPathName;  }
			DEKAF2_NODISCARD const KString& GetBriefDescription() const { return sBriefDescription; }
			DEKAF2_NODISCARD const KString& GetAdditionalHelp()   const { return sAdditionalHelp;   }

			KString        sProgramPathName;
			KString        sBriefDescription;
			KString        sAdditionalArgDesc;
			KString        sAdditionalHelp;
			KString        sSeparator              {    "::" };
			uint16_t       iWrappedHelpIndent      {       1 };
			uint16_t       iMaxHelpRowWidth        {     100 };
			bool           bSpacingPerSection      {   false };
			bool           bLinefeedBetweenOptions {   false };
		};

		HelpFormatter(KOutStream&                       out,
					  const std::vector<CallbackParam>& Callbacks,
					  const HelpParams&                 HelpParams,
					  uint16_t                          iTerminalWidth);

		HelpFormatter(KOutStream&          out,
					  const CallbackParam& Callback,
					  const HelpParams&    HelpParams,
					  uint16_t             iTerminalWidth);

	//----------
	private:
	//----------

		//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
		struct DEKAF2_PRIVATE Mask
		//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
		{
			Mask(const HelpFormatter& Formatter, bool bForCommands, bool bIsRequired);

			KString        sFormat;
			KString        sOverflow;
			uint16_t       iMaxHelp     {    80 };
			bool           bOverlapping { false };

		}; // Mask

		DEKAF2_NODISCARD
		static KStringView SplitAtLinefeed(KStringView& sInput);
		DEKAF2_NODISCARD
		static KStringView WrapOutput(KStringView& sInput, std::size_t iMaxSize, bool bKeepLineFeeds);
		DEKAF2_NODISCARD
		static KStringView::size_type AdjustPos(KStringView::size_type iPos, int iAdjust);

		void           GetEnvironment();
		void           CalcExtends(const std::vector<CallbackParam>& Callbacks);
		void           CalcExtends(const CallbackParam& Callback);
		void           FormatOne(KOutStream& out, const CallbackParam& Callback, const Mask& mask);

		const HelpParams&
		               m_Params;

		std::array<std::size_t, 2>
		               m_MaxLens                 { 0, 0                             };
		KStringView    m_sSeparator              { m_Params.sSeparator              };
		uint16_t       m_iColumns                { m_Params.iMaxHelpRowWidth        };
		uint16_t       m_iWrappedHelpIndent      { m_Params.iWrappedHelpIndent      };
		bool           m_bLinefeedBetweenOptions { m_Params.bLinefeedBetweenOptions };
		bool           m_bSpacingPerSection      { m_Params.bSpacingPerSection      };
		bool           m_bHaveOptions            { false                            };
		bool           m_bHaveCommands           { false                            };

	}; // HelpFormatter

	DEKAF2_PRIVATE
	void Register(CallbackParam OptionOrCommand);
	DEKAF2_NODISCARD DEKAF2_PRIVATE
	OptionalParm IntOptionOrCommand(KStringView sOption, KStringView sArgDescription, bool bIsCommand)
	{
		return OptionalParm(*this, sOption, sArgDescription, bIsCommand);
	}
	DEKAF2_NODISCARD DEKAF2_PRIVATE
	KStringViewZ ModifyArgument(KStringViewZ sArg, const CallbackParam* Callback);
	DEKAF2_NODISCARD DEKAF2_PRIVATE
	KString BadBoundsReason(ArgTypes Type, KStringView sParm, int64_t iMinBound, int64_t iMaxBound) const;
	DEKAF2_NODISCARD DEKAF2_PRIVATE
	bool ValidBounds(ArgTypes Type, KStringView sParm, int64_t iMinBound, int64_t iMaxBound) const;
	DEKAF2_NODISCARD DEKAF2_PRIVATE
	KString BadArgReason(ArgTypes Type, KStringView sParm) const;
	DEKAF2_NODISCARD DEKAF2_PRIVATE
	bool ValidArgType(ArgTypes Type, KStringViewZ sParm) const;
	DEKAF2_NODISCARD DEKAF2_PRIVATE
	const CallbackParam* FindParam(KStringView sName, bool bIsOption) const;
	DEKAF2_NODISCARD DEKAF2_PRIVATE
	const CallbackParam* FindParam(KStringView sName, bool bIsOption, bool bMarkAsUsed) const;
	DEKAF2_NODISCARD DEKAF2_PRIVATE
	const CallbackParam* FindParamForGet(KStringView sOptionName) const;
	DEKAF2_PRIVATE
	void ResetBeforeParsing();
	DEKAF2_PRIVATE
	int Execute(CLIParms Parms, KOutStream& out);
	DEKAF2_PRIVATE
	bool Evaluate(const CLIParms& Parms, KOutStream& out);
	DEKAF2_PRIVATE
	void SetDefaults(KStringView sCliDebugTo);
	DEKAF2_PRIVATE
	void AutomaticHelp() const;
	DEKAF2_PRIVATE
	bool AdHocOptionSanityCheck(KStringView sOption);
	DEKAF2_NODISCARD DEKAF2_PRIVATE
	KString BuildParameterError(const CallbackParam& Callback, KString sMessage) const;
	/// Is this arg maybe a combination of single char args?
	DEKAF2_NODISCARD DEKAF2_PRIVATE
	std::vector<KStringViewZ> CheckForCombinedArg(const CLIParms::Arg_t& arg);
	/// returns the list of isolated option names in the first, and help text in the second string view
	DEKAF2_NODISCARD DEKAF2_PRIVATE
	static std::pair<KStringView, KStringView> IsolateOptionNamesFromHelp(KStringView sOptionNamesAndHelp);
	DEKAF2_NODISCARD DEKAF2_PRIVATE
	static KStringView IsolateOptionNamesFromSuffix(KStringView sOptionName);


	using CommandLookup = KUnorderedMap<KStringView, std::size_t>;

	HelpFormatter::HelpParams  m_HelpParams;
	std::vector<CallbackParam> m_Callbacks;
	KPersistStrings<KString, false>
	                           m_Strings;
	CommandLookup              m_Commands;
	CommandLookup              m_Options;
	KStringViewZ               m_sCurrentArg;
	KOutStream*                m_CurrentOutputStream     { nullptr };
	std::vector<KStringViewZ>  m_UnknownCommands;
	const KStringView*         m_sHelp                   { nullptr };
	std::size_t                m_iHelpSize               {       0 };
	std::size_t                m_iAutomaticOptionCount   {       0 };
	uint16_t                   m_iRecursedHelp           {       0 };
	uint16_t                   m_iExecutions             {       0 };
	bool                       m_bEmptyParmsIsError      {    true };
	bool                       m_bStopAppAfterParsing    {   false };
	bool                       m_bAllowAdHocArgs         {   false };
	bool                       m_bHaveAdHocArgs          {   false };
	bool                       m_bCheckWasCalled         {   false };
	bool                       m_bHaveErrors             {   false };
	mutable bool               m_bProcessAdHocForHelp    {   false };

}; // KOptions

DEKAF2_NAMESPACE_END

