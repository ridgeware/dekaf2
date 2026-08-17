/*
 //
 // DEKAF(tm): Lighter, Faster, Smarter (tm)
 //
 // Copyright (c) 2026, Ridgeware, Inc.
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

// kmqtt — explore a broker on the cli

#include <dekaf2/net/mqtt/kmqttclient.h>
#include <dekaf2/core/init/dekaf2.h>
#include <dekaf2/core/errors/kexception.h>
#include <dekaf2/core/format/kformat.h>
#include <dekaf2/io/readwrite/kwriter.h>
#include <dekaf2/system/os/ksystem.h>
#include <dekaf2/util/cli/koptions.h>

using namespace dekaf2;

//-----------------------------------------------------------------------------
int main(int argc, char** argv)
//-----------------------------------------------------------------------------
{
	try
	{
		KInit(false);

		// options in the inline single string style, commands with callbacks -
		// the true ad-hoc style (parse first, query later) cannot express
		// commands: an unknown option binds all following non-option words as
		// its values and would swallow the command
		KOptions Options(true, KLog::STDOUT, /*bThrow*/true);

		Options.SetBriefDescription("explore an MQTT broker on the cli");

		KString      sBroker;
		KString      sUser;
		KString      sPass;
		KString      sClientID = "kmqtt";
		bool         bNoVerify = false;
		uint32_t     iWait     = 0;
		KStringViewZ sCommand;
		KStringViewZ sTopic;
		KStringViewZ sPayload;

		Options.Option("broker <url>    : broker address, e.g. mqtt://192.168.1.20 (default: $MQTT_BROKER)").Set(sBroker);
		Options.Option("user <name>     : broker user name").Set(sUser);
		Options.Option("pass <password> : broker password").Set(sPass);
		Options.Option("id <clientid>   : client id, must be unique on the broker").Set(sClientID);
		Options.Option("k,noverify      : do not verify the broker certificate (mqtts only)").Set(bNoVerify, true);
		Options.Option("wait <seconds>  : for sub: stop collecting after this many seconds (default: run until Ctrl-C)").Set(iWait);

		Options.Command("sub <topic>").Help("subscribe and print every message, until Ctrl-C or for -wait seconds")
		([&](KStringViewZ sArg) { sCommand = "sub"; sTopic = sArg; });

		Options.Command("pub <topic> [<payload>]").Help("publish one message and exit")
		([&](KOptions::ArgList& Args)
		{
			sCommand = "pub";
			sTopic   = Args.pop();
			if (!Args.empty()) sPayload = Args.pop();
		});

		if (Options.Parse(argc, argv, KOut) != 0) return 1;

		if (sBroker.empty()) sBroker = kGetEnv("MQTT_BROKER");

		if (sCommand.empty()) throw KError("no command - try -help");
		if (sBroker.empty())  throw KError("no broker - use -broker or $MQTT_BROKER");

		KMQTTClient Client(sBroker, sClientID);

		if (!sUser.empty()) Client.SetCredentials(sUser, sPass);
		if (bNoVerify)      Client.SetVerifyCerts(false);

		Client.SetMessageCallback([](KStringView sMsgTopic, KStringView sMsgPayload)
		{
			// one line per message: topic, then the payload verbatim - this is
			// the tool for finding out what a broker actually publishes, so
			// nothing here interprets or prettifies anything
			kPrintLine("{} {}", sMsgTopic, sMsgPayload);
			// flushed per line: piped into a file or a filter, stdout is fully
			// buffered, and a monitoring tool must not sit on its messages
			KOut.Flush();
		});

		Client.SetStateCallback([](bool bConnected)
		{
			kPrintLine(KErr, ">> {}", bConnected ? "connected" : "disconnected");
		});

		if (sCommand == "sub")
		{
			Client.Subscribe(sTopic);
			Client.Start();

			if (iWait)
			{
				// bounded collection window for scripted use, e.g. topic inventories
				kSleep(chrono::seconds(iWait));
				Client.Stop();
				return 0;
			}

			for (;;) kSleep(chrono::seconds(1));   // Ctrl-C to quit
		}

		if (sCommand == "pub")
		{
			Client.Start();

			// Publish() needs the session to be up, and connecting is
			// asynchronous - wait for it rather than failing on a race
			for (uint16_t iTenth = 0; iTenth < 100 && !Client.IsConnected(); ++iTenth)
			{
				kSleep(chrono::milliseconds(100));
			}

			if (!Client.IsConnected())
			{
				throw KError(Client.LastError().empty() ? KString("cannot connect") : Client.LastError());
			}

			if (!Client.Publish(sTopic, sPayload)) throw KError("publish failed");

			// give the packet a moment to leave before the destructor closes
			// the socket under it
			kSleep(chrono::milliseconds(200));
			Client.Stop();
			return 0;
		}

		throw KError(kFormat("unknown command: {}", sCommand));
	}
	catch (const std::exception& ex)
	{
		kPrintLine(KErr, ">> {}", ex.what());
	}

	return 1;

} // main
