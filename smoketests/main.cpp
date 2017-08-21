
#define CATCH_CONFIG_RUNNER
#include "catch.hpp"
#include <dekaf2/dekaf2.h>

#include <string>
#include <iostream>

int main( int argc, char* const argv[] )
{
	dekaf2::Dekaf().SetMultiThreading();

    int result = Catch::Session().run( argc, argv );

    // global clean-up...

    return result;
}
