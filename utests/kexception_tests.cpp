#include "catch.hpp"

#include <dekaf2/core/errors/kexception.h>
#include <dekaf2/core/errors/kerror.h>
#include <dekaf2/core/strings/kstring.h>
#include <exception>
#include <thread>
#include <utility>
#include <vector>

using namespace dekaf2;

namespace {

// a class with errors, as the classes of dekaf2 have them
class WithError : public KErrorBase
{
public:

	bool Fail(KStringViewZ sError, uint16_t iErrorCode = -1) const { return SetError(sError, iErrorCode); }
	bool Take(const WithError& other) const                      { return SetError(other.CopyLastError()); }
	char chPayload { 0 };
};

} // end of anonymous namespace

TEST_CASE("KException")
{
	SECTION("no error")
	{
		KException ec;

		CHECK ( !ec );
		CHECK ( ec.value()   == 0  );
		CHECK ( ec.message() == "" );

		// clearing an object without an error leaves it as it is
		ec.clear();

		CHECK ( !ec );
		CHECK ( ec.value()   == 0  );
		CHECK ( ec.message() == "" );
	}

	SECTION("error")
	{
		KException ec("something failed", 42);

		CHECK ( ec );
		CHECK ( ec.value()   == 42                 );
		CHECK ( ec.message() == "something failed" );
		CHECK ( KStringView(ec.what()) == "something failed" );

		// the default error code
		CHECK ( KException("failed").value() == 65535 );

		// a copy has the code and the message
		KException Copy(ec);

		CHECK ( Copy.value()   == 42                 );
		CHECK ( Copy.message() == "something failed" );

		KError Assigned;
		Assigned = ec;

		CHECK ( Assigned.value()   == 42                 );
		CHECK ( Assigned.message() == "something failed" );

		ec.clear();

		CHECK ( !ec );
		CHECK ( ec.value()   == 0  );
		CHECK ( ec.message() == "" );

		// the copies keep their state
		CHECK ( Copy.value()       == 42                 );
		CHECK ( Assigned.message() == "something failed" );
	}

	SECTION("a message without an error code")
	{
		// with the code 0 this is no error, but clear() still removes the message
		KException ec("only a message", 0);

		CHECK ( !ec );
		CHECK ( ec.message() == "only a message" );

		ec.clear();

		CHECK ( ec.message() == "" );
	}

	SECTION("thrown and caught")
	{
		bool bCaught { false };

		try
		{
			throw KException("thrown", 7);
		}
		catch (const KException& ex)
		{
			bCaught = true;
			CHECK ( ex.value()   == 7        );
			CHECK ( ex.message() == "thrown" );
		}

		CHECK ( bCaught );

		// the standard base the exception can be caught as
		bool bCaughtAsStd { false };

		try
		{
			throw KException("thrown as std", 8);
		}
		catch (const std::exception& ex)
		{
			bCaughtAsStd = true;
			CHECK ( KStringView(ex.what()) == "thrown as std" );
		}

		CHECK ( bCaughtAsStd );
	}

	SECTION("an empty message with an error code")
	{
		KException ec("", 5);

		CHECK ( ec );
		CHECK ( ec.value()   == 5  );
		CHECK ( ec.message() == "" );
	}

	SECTION("moved")
	{
		KException ec("something failed", 42);
		KException Moved(std::move(ec));

		CHECK ( Moved.value()   == 42                 );
		CHECK ( Moved.message() == "something failed" );

		KException Target("another error", 1);
		Target = std::move(Moved);

		CHECK ( Target.value()   == 42                 );
		CHECK ( Target.message() == "something failed" );

		// assigned to itself
		auto& Self = Target;
		Target = Self;

		CHECK ( Target.message() == "something failed" );
	}

	SECTION("copies in several threads")
	{
		// the copies share the message: every thread copies and drops them
		KException ec("shared by all threads", 3);

		std::vector<std::thread> Threads;

		for (int iThread = 0; iThread < 4; ++iThread)
		{
			Threads.emplace_back([&ec]()
			{
				for (int iRound = 0; iRound < 10000; ++iRound)
				{
					KException Copy(ec);
					KException Second;
					Second = Copy;
					Copy.clear();
				}
			});
		}

		for (auto& Thread : Threads)
		{
			Thread.join();
		}

		CHECK ( ec.value()   == 3                       );
		CHECK ( ec.message() == "shared by all threads" );
	}
}

TEST_CASE("KErrorBase")
{
	SECTION("size")
	{
		// the base class of many small classes: one pointer
		CHECK ( sizeof(KErrorBase) == sizeof(void*)     );
		CHECK ( sizeof(KException) == 2 * sizeof(void*) );
	}

	SECTION("no error")
	{
		WithError Object;

		CHECK ( Object.HasError()          == false );
		CHECK ( Object.GetLastErrorCode()  == 0     );
		CHECK ( Object.GetLastError()      == ""    );
		CHECK ( Object.Error()             == ""    );
		CHECK ( Object.WouldThrowOnError() == false );

		Object.ClearError();

		CHECK ( Object.HasError()          == false );
	}

	SECTION("error")
	{
		WithError Object;

		CHECK ( Object.Fail("it failed", 17) == false );
		CHECK ( Object.HasError()            == true        );
		CHECK ( Object.GetLastErrorCode()    == 17          );
		CHECK ( Object.GetLastError()        == "it failed" );
		CHECK ( Object.Error()               == "it failed" );

		// the default error code
		Object.Fail("failed again");
		CHECK ( Object.GetLastErrorCode()    == 65535          );
		CHECK ( Object.GetLastError()        == "failed again" );

		Object.ClearError();

		CHECK ( Object.HasError()            == false );
		CHECK ( Object.GetLastError()        == ""    );
	}

	SECTION("an error taken from another object")
	{
		WithError Source;
		WithError Target;

		Source.Fail("from the source", 4);
		Target.SetThrowOnError(false);

		CHECK ( Target.Take(Source) == false );
		CHECK ( Target.GetLastError()     == "from the source" );
		CHECK ( Target.GetLastErrorCode() == 4                 );

		// the source can go, the target keeps the error
		Source.ClearError();

		CHECK ( Source.HasError()     == false             );
		CHECK ( Target.GetLastError() == "from the source" );
	}

	SECTION("throw on error")
	{
		WithError Object;
		Object.SetThrowOnError(true);

		CHECK ( Object.WouldThrowOnError() == true  );
		CHECK ( Object.HasError()          == false );

		bool bCaught { false };

		try
		{
			Object.Fail("thrown from the object", 9);
		}
		catch (const KException& ex)
		{
			bCaught = true;
			CHECK ( ex.value()   == 9                        );
			CHECK ( ex.message() == "thrown from the object" );
		}

		CHECK ( bCaught );

		// the object keeps the error, and its throw setting when the error is cleared
		CHECK ( Object.GetLastError()      == "thrown from the object" );
		Object.ClearError();
		CHECK ( Object.HasError()          == false );
		CHECK ( Object.WouldThrowOnError() == true  );

		// taking the error of another object throws nothing by itself, and keeps the setting
		WithError Source;
		Source.Fail("from the source", 4);
		CHECK ( Source.WouldThrowOnError() == false );
		CHECK ( Object.Take(Source)        == false );
		CHECK ( Object.WouldThrowOnError() == true  );
		CHECK ( Object.GetLastErrorCode()  == 4     );

		// switching it on with an error already there throws
		WithError Late;
		Late.Fail("an earlier error", 2);
		CHECK_THROWS_AS ( Late.SetThrowOnError(true), KException );
		CHECK ( Late.WouldThrowOnError() == true );

		// it can be caught as the standard exception, too
		CHECK_THROWS_AS ( Late.Fail("once more"), std::exception );

		Late.SetThrowOnError(false);
		CHECK ( Late.Fail("no longer thrown") == false );
	}

	SECTION("copied and moved")
	{
		WithError Object;
		Object.SetThrowOnError(false);
		Object.Fail("to be copied", 6);
		Object.chPayload = 'x';

		WithError Copy(Object);

		CHECK ( Copy.GetLastError()     == "to be copied" );
		CHECK ( Copy.GetLastErrorCode() == 6              );
		CHECK ( Copy.chPayload          == 'x'            );

		WithError Moved(std::move(Copy));

		CHECK ( Moved.GetLastError()     == "to be copied" );
		CHECK ( Moved.GetLastErrorCode() == 6              );

		WithError Assigned;
		Assigned = Object;

		CHECK ( Assigned.GetLastError() == "to be copied" );

		// a copy has the throw setting of its source
		WithError Throwing;
		Throwing.SetThrowOnError(true);
		WithError ThrowingCopy(Throwing);

		CHECK ( ThrowingCopy.WouldThrowOnError() == true );
	}
}
