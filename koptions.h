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
#include "kpersist.h"
#include <functional>
#include <vector>


/// @file koptions.h
/// option parsing


namespace dekaf2 {

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
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

	/// set a brief description of the program, will appear in first line of generated help
	KOptions& SetBriefDescription(KString sBrief) { m_HelpParams.sBriefDescription = std::move(sBrief); return *this; }

	/// set an additional description at the end of the automatic "usage:" string
	KOptions& SetAdditionalArgDescription(KString sAdditionalArgDesc) { m_HelpParams.sAdditionalArgDesc = std::move(sAdditionalArgDesc); return *this; }

	/// set the separator style for the generated help - default is ::
	KOptions& SetHelpSeparator(KString sSeparator) { m_HelpParams.sSeparator = std::move(sSeparator); return *this; }

	/// set max generated help width in characters if terminal size is unknown, default = 100
	KOptions& SetMaxHelpWidth(uint16_t iMaxWidth) { m_HelpParams.iMaxHelpRowWidth = iMaxWidth; return *this; }

	/// set indent for wrapped help lines, default 1
	KOptions& SetWrappedHelpIndent(uint16_t iIndent) { m_HelpParams.iWrappedHelpIndent = iIndent; return *this; }

	/// calculate column width for names per section, or same for all (default = false)
	KOptions& SetSpacingPerSection(bool bSpacingPerSection) { m_HelpParams.bSpacingPerSection = bSpacingPerSection; return *this; }

	/// write an empty line between all options? (default = false)
	KOptions& SetLinefeedBetweenOptions(bool bLinefeedBetweenOptions) { m_HelpParams.bLinefeedBetweenOptions = bLinefeedBetweenOptions; return *this; }

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
		Email,     ///< email address
		URL,       ///< URL
	};

//----------
private:
//----------

	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	class DEKAF2_PUBLIC CallbackParam
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
			fToUpper     = 1 << 4,
			fIsHidden    = 1 << 5,
			fIsSection   = 1 << 6,
			fIsUnknown   = 1 << 7
		};

		CallbackParam() = default;
		CallbackParam(KStringView sNames, KStringView sMissingArgs, uint16_t fFlags, uint16_t iMinArgs = 0, CallbackN Func = CallbackN{});

		CallbackN    m_Callback;
		KStringView  m_sNames;
		KStringView  m_sMissingArgs;
		KStringView  m_sHelp;
		int64_t      m_iLowerBound  { 0 };
		int64_t      m_iUpperBound  { 0 };
		uint16_t     m_iMinArgs     { 0 };
		uint16_t     m_iMaxArgs     { 65535  };
		uint16_t     m_iFlags       { fNone  };
		uint16_t     m_iHelpRank    { 0 };
		ArgTypes     m_ArgType      { String };
		mutable bool m_bUsed        { false  };

		/// returns true if this parameter is required
		bool         IsRequired()  const { return m_iFlags & fIsRequired;   }
		/// returns true if this parameter is an option, not a command
		bool         IsOption()    const { return IsCommand() == false;     }
		/// returns true if this parameter is a command, not an option
		bool         IsCommand()   const { return m_iFlags & fIsCommand;    }
		/// returns true if this parameter shall be hidden from auto-documentation
		bool         IsHidden()    const { return m_iFlags & fIsHidden;     }
		/// returns true if this parameter is a section break for the auto-documentation
		bool         IsSection()   const { return m_iFlags & fIsSection;    }
		/// returns true if this parameter is an "unknown" catchall, either for options or commands
		bool         IsUnknown()   const { return m_iFlags & fIsUnknown;    }
		/// returns true if the boundaries of this parameter shall be checked
		bool         CheckBounds() const { return m_iFlags & fCheckBounds;  }
		/// returns true if the string shall be converted to lowercase
		bool         ToLower()     const { return m_iFlags & fToLower;      }
		/// returns true if the string shall be converted to uppercase
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
		OptionalParm(KOptions& base, KStringView sOption, KStringView sArgDescription, bool bIsCommand)
		: CallbackParam(sOption,
						sArgDescription,
						bIsCommand ? fIsCommand : fNone)
		, m_base(&base)
		{
		}

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
		/// set the callback for the parameter as a function void(KOptions::ArgList&)
		OptionalParm& Callback(CallbackN Func);
		/// set the callback for the parameter as a function void(KStringViewZ)
		OptionalParm& Callback(Callback1 Func);
		/// set the callback for the parameter as a function void()
		OptionalParm& Callback(Callback0 Func);
		OptionalParm& operator()(CallbackN Func)  { return Callback(std::move(Func)); }
		OptionalParm& operator()(Callback1 Func)  { return Callback(std::move(Func)); }
		OptionalParm& operator()(Callback0 Func)  { return Callback(std::move(Func)); }
		/// set the text shown in the help for this parameter, iHelpRank controls the order of help output shown, 0 = highest
		template<class String>
		OptionalParm& Help(String&& sHelp, uint16_t iHelpRank = 0)
		{
			return IntHelp(m_base->m_Strings.Persist(std::forward<String>(sHelp)), iHelpRank);
		}
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
	OptionalParm Option(String1&& sOption)
	{
		return IntOptionOrCommand(m_Strings.Persist(std::forward<String1>(sOption)), "", false);
	}
	/// Start definition of a new command. Have it follow by any chained count of methods of OptionalParms, like Command("clear").Help("clear all data").Callback([&](){ RunClear() });
	template<class String1>
	OptionalParm Command(String1&& sCommand)
	{
		return IntOptionOrCommand(m_Strings.Persist(std::forward<String1>(sCommand)), "", true);
	}

	/// Start definition of a new option. Have it follow by any chained count of methods of OptionalParms, like Option("clear").Help("clear all data").Callback([&](){ RunClear() });
	template<class String1, class String2>
	OptionalParm Option(String1&& sOption, String2&& sArgDescription)
	{
		return IntOptionOrCommand(m_Strings.Persist(std::forward<String1>(sOption)), m_Strings.Persist(std::forward<String2>(sArgDescription)), false);
	}
	/// Start definition of a new command. Have it follow by any chained count of methods of OptionalParms, like Command("clear").Help("clear all data").Callback([&](){ RunClear() });
	template<class String1, class String2>
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

	/// Register a callback function for occurences of "sCommand" with an arbitrary, but defined minimal amount of additional args
	/// The sMissingParms string is output if there are less than iMinArgs args.
	void RegisterCommand(KStringView sCommand, uint16_t iMinArgs, KStringViewZ sMissingParms, CallbackN Function);

	/// Register a callback function for unhandled options
	void RegisterUnknownOption(CallbackN Function) { UnknownOption(std::move(Function)); }

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
	KStringViewZ GetCurrentArg() const
	{
		return m_sCurrentArg;
	}

	/// Get the current output stream while parsing commands/args
	KOutStream& GetCurrentOutputStream() const;

	/// Get the current output stream width while parsing commands/args
	uint16_t GetCurrentOutputStreamWidth() const;

	/// Returns true if we are executed inside a CGI server
	static bool IsCGIEnvironment();

	/// Returns arg[0] / the path and name of the called executable
	const KString& GetProgramPath() const { return m_HelpParams.GetProgramPath(); }

	/// Returns basename of arg[0] / the name of the called executable
	KStringView GetProgramName() const { return m_HelpParams.GetProgramName(); }

	/// Returns brief description of the called executable
	const KString& GetBriefDescription() const { return m_HelpParams.GetBriefDescription(); }

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

			bool         IsOption() const { return iDashes; }
			KStringViewZ Dashes()   const;

			KStringViewZ sArg;
			bool         bConsumed { false };

		//----------
		private:
		//----------

			static constexpr KStringViewZ s_sDoubleDash = "--";

			uint8_t      iDashes { 0 };

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

		void           Create(int argc, char const* const* argv);
		void           Create(const std::vector<KStringViewZ>& parms);

		size_t         size()  const { return m_ArgVec.size();  }
		size_t         empty() const { return m_ArgVec.empty(); }
		iterator       begin()       { return m_ArgVec.begin(); }
		iterator       end()         { return m_ArgVec.end();   }
		const_iterator begin() const { return m_ArgVec.begin(); }
		const_iterator end()   const { return m_ArgVec.end();   }
		void           clear()       { m_ArgVec.clear();        }

		KStringViewZ   GetProgramPath() const;
		KStringView    GetProgramName() const;

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
			KStringView    GetProgramName()      const;
			const KString& GetProgramPath()      const { return sProgramPathName;  }
			const KString& GetBriefDescription() const { return sBriefDescription; }

			KString        sProgramPathName;
			KString        sBriefDescription;
			KString        sAdditionalArgDesc;
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
			Mask(const HelpFormatter& Formatter, bool bForCommands);

			KString        sFormat;
			KString        sOverflow;
			uint16_t       iMaxHelp     {    80 };
			bool           bOverlapping { false };

		}; // Mask

		static KStringView SplitAtLinefeed(KStringView& sInput);
		static KStringView WrapOutput(KStringView& sInput, std::size_t iMaxSize);
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
	DEKAF2_PRIVATE
	OptionalParm IntOptionOrCommand(KStringView sOption, KStringView sArgDescription, bool bIsCommand)
	{
		return OptionalParm(*this, sOption, sArgDescription, bIsCommand);
	}
	DEKAF2_PRIVATE
	KStringViewZ ModifyArgument(KStringViewZ sArg, const CallbackParam* Callback);
	DEKAF2_PRIVATE
	KString BadBoundsReason(ArgTypes Type, KStringView sParm, int64_t iMinBound, int64_t iMaxBound) const;
	DEKAF2_PRIVATE
	bool ValidBounds(ArgTypes Type, KStringView sParm, int64_t iMinBound, int64_t iMaxBound) const;
	DEKAF2_PRIVATE
	KString BadArgReason(ArgTypes Type, KStringView sParm) const;
	DEKAF2_PRIVATE
	bool ValidArgType(ArgTypes Type, KStringViewZ sParm) const;
	DEKAF2_PRIVATE
	const CallbackParam* FindParam(KStringView sName, bool bIsOption) const;
	DEKAF2_PRIVATE
	int Execute(CLIParms Parms, KOutStream& out);
	DEKAF2_PRIVATE
	int Evaluate(const CLIParms& Parms, KOutStream& out);
	DEKAF2_PRIVATE
	void AutomaticHelp();
	DEKAF2_PRIVATE
	KString BuildParameterError(const CallbackParam& Callback, KString sMessage);

	using CommandLookup = KUnorderedMap<KStringView, std::size_t>;

	HelpFormatter::HelpParams  m_HelpParams;
	std::vector<CallbackParam> m_Callbacks;
	KPersistStrings<false>     m_Strings;
	CommandLookup              m_Commands;
	CommandLookup              m_Options;
	KStringViewZ               m_sCurrentArg;
	KOutStream*                m_CurrentOutputStream     { nullptr };
	const KStringView*         m_sHelp                   { nullptr };
	std::size_t                m_iHelpSize               {       0 };
	uint16_t                   m_iRecursedHelp           {       0 };
	bool                       m_bEmptyParmsIsError      {    true };

}; // KOptions

} // end of namespace dekaf2









