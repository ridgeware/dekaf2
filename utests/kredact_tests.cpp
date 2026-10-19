#include "catch.hpp"

#include <dekaf2/core/logging/kredact.h>

using namespace dekaf2;

TEST_CASE("kRedactCredentials")
{
	SECTION("a text without credentials stays untouched")
	{
		KString sBuffer = "unchanged";
		CHECK ( kRedactCredentials("pass=3 fail=0 key=value", sBuffer) == false );
		CHECK ( sBuffer == "unchanged" );
		CHECK ( kRedactCredentials("pass=3 fail=0 key=value") == "pass=3 fail=0 key=value" );
	}

	SECTION("the shapes of credentials")
	{
		KString sBuffer;
		CHECK ( kRedactCredentials("Authorization: Bearer abc.def", sBuffer) );
		CHECK ( sBuffer == "Authorization: Bearer [redacted]" );

		CHECK ( kRedactCredentials("https://user:secret@example.com/path") == "https://user:[redacted]@example.com/path" );
		CHECK ( kRedactCredentials("login with password=hunter2 done")     == "login with password=[redacted] done" );
		CHECK ( kRedactCredentials("GET /api?api_key=abc123&x=1")          == "GET /api?api_key=[redacted]&x=1" );
		CHECK ( kRedactCredentials("token eyJhbGciOiJIUzI1NiJ9.eyJzdWIiOiIxIn0.c2lnbmF0dXJl end") == "token eyJ...redacted...XJl end" );
	}

	SECTION("the schemes of an Authorization header")
	{
		// a known scheme is kept, the credentials after it are redacted
		CHECK ( kRedactCredentials("Authorization: DeepL-Auth-Key 00000000-1111-2222-3333-444444444444:fx")
		        == "Authorization: DeepL-Auth-Key 000...redacted...:fx" );
		CHECK ( kRedactCredentials("Authorization: Bearer abc123 sent"  ) == "Authorization: Bearer [redacted] sent" );
		CHECK ( kRedactCredentials("Authorization: Basic  dXNlcjpwYXNz" ) == "Authorization: Basic  [redacted]"      );
		CHECK ( kRedactCredentials("Authorization:\tBasic\tdXNlcjpwYXNz") == "Authorization:\tBasic\t[redacted]"     );

		// a scheme that ends with "key" or "token" is kept as well
		CHECK ( kRedactCredentials("Authorization: ApiKey abc123 sent"  ) == "Authorization: ApiKey [redacted] sent" );
		CHECK ( kRedactCredentials("authorization: token abc123"        ) == "authorization: token [redacted]"       );
		CHECK ( kRedactCredentials("Authorization: Api-Token abc123"    ) == "Authorization: Api-Token [redacted]"   );

		// but not a word with a digit, nor one without a word after it
		CHECK ( kRedactCredentials("Authorization: c2VjcmV0key sent"    ) == "Authorization: c2V...redacted...ent"   );
		CHECK ( kRedactCredentials("Authorization: secretkey"           ) == "Authorization: [redacted]"             );
		CHECK ( kRedactCredentials("Authorization: secretkey\nnext line") == "Authorization: [redacted]\nnext line"  );

		// an unknown scheme is redacted together with its credentials
		CHECK ( kRedactCredentials("Authorization: CustomScheme c2VjcmV0 sent") == "Authorization: Cus...redacted...mV0 sent" );

		// so is a credential without a scheme, also when other text follows
		CHECK ( kRedactCredentials("Authorization: c2VjcmV0"            ) == "Authorization: [redacted]"             );
		CHECK ( kRedactCredentials("Authorization: c2VjcmV0 from client") == "Authorization: [redacted] client"      );
		CHECK ( kRedactCredentials("'Authorization: c2VjcmV0' sent"     ) == "'Authorization: [redacted]' sent"      );
	}

	SECTION("a credential in a JSON body")
	{
		// the shape an error from a service actually has
		CHECK ( kRedactCredentials(R"({"access_token":"ya29.a0AfH6SMBxQQQQ","expires_in":3599})")
		        ==                 R"({"access_token":"ya2...redacted...QQQ","expires_in":3599})" );

		CHECK ( kRedactCredentials(R"({"ApiTokens":["7f9c2b11-4a3e-4d55-9f10-abc123def456"],"Name":"sbemail"})")
		        ==                 R"({"ApiTokens":["7f9...redacted...456"],"Name":"sbemail"})" );

		// every element of an array belongs to the name that introduces it
		CHECK ( kRedactCredentials(R"({"ApiTokens":["aaaaaaaaaaaaaaaa","bbbbbbbbbbbbbbbb"]})")
		        ==                 R"({"ApiTokens":["aaa...redacted...aaa","bbb...redacted...bbb"]})" );

		// white space around the colon, and a pretty-printed body
		CHECK ( kRedactCredentials(R"({ "client_secret" : "s3crets3crets3cret" })")
		        ==                 R"({ "client_secret" : "s3c...redacted...ret" })" );
		CHECK ( kRedactCredentials("{\n\t\"refresh_token\": \"abc123\"\n}")
		        ==                 "{\n\t\"refresh_token\": \"[redacted]\"\n}" );

		// a ':' before the body and inside values, ahead of the field
		CHECK ( kRedactCredentials(R"(12:00:01 got {"url":"https://example.com/a","note":"a: b","api_key":"abc123"})")
		        ==                 R"(12:00:01 got {"url":"https://example.com/a","note":"a: b","api_key":"[redacted]"})" );

		// case does not matter, and neither does the vendor's spelling
		CHECK ( kRedactCredentials(R"({"ApiKey":"abc123","X-Api-Token":"def456"})")
		        ==                 R"({"ApiKey":"[redacted]","X-Api-Token":"[redacted]"})" );
	}

	SECTION("a credential in SQL")
	{
		// the form of an update that KROW creates
		CHECK ( kRedactCredentials("update USERS set\n\tname='bob'\n\t,password='hunter2'\nwhere id=1")
		        ==                 "update USERS set\n\tname='bob'\n\t,password='[redacted]'\nwhere id=1" );

		// white space around the '=', identifier quotes, a qualified name
		CHECK ( kRedactCredentials("update USERS set `password` = 'hunter2' where id=1")
		        ==                 "update USERS set `password` = '[redacted]' where id=1" );
		CHECK ( kRedactCredentials("update [USERS] set [api_key] = N'abc123' where id=1")
		        ==                 "update [USERS] set [api_key] = N'[redacted]' where id=1" );
		CHECK ( kRedactCredentials("select * from USERS U where U.password = 'hunter2'")
		        ==                 "select * from USERS U where U.password = '[redacted]'" );

		// the escapes of SQL do not end the value
		CHECK ( kRedactCredentials(R"(set client_secret='a''b\'c', name='bob')")
		        ==                 R"(set client_secret='[redacted]', name='bob')" );

		// double quotes, as in an attribute
		CHECK ( kRedactCredentials(R"(<login user="bob" password="hunter2"/>)")
		        ==                 R"(<login user="bob" password="[redacted]"/>)" );

		// an empty value, and a value without quotes after white space, which in
		// SQL is a column or a number
		KString sBuffer = "unchanged";
		CHECK ( kRedactCredentials("update USERS set password='' where id=1", sBuffer) == false );
		CHECK ( kRedactCredentials("join KEYS K on K.api_key = U.api_key", sBuffer) == false );
		CHECK ( sBuffer == "unchanged" );
	}

	SECTION("how much of a credential survives")
	{
		// A LONG ONE KEEPS THREE CHARACTERS AT EACH END, so a log line can still
		// answer "which key is this?" without carrying the key.
		CHECK ( kRedactCredentials("password=sk-proj-abcdefghijklmnop done")
		        ==                 "password=sk-...redacted...nop done" );

		// A SHORT ONE DOES NOT. Six of eight characters is the password itself.
		CHECK ( kRedactCredentials("password=hunter2 done")  == "password=[redacted] done" );
		CHECK ( kRedactCredentials("password=abcdefghijklmno done")
		        ==                 "password=[redacted] done" );            // 15, one short
		CHECK ( kRedactCredentials("password=abcdefghijklmnop done")
		        ==                 "password=abc...redacted...nop done" );  // 16, the threshold

		// A SECOND PASS CHANGES NOTHING. The marker no longer starts the value, so
		// it is looked for anywhere in it.
		auto sOnce = kRedactCredentials(R"({"access_token":"ya29.a0AfH6SMBxQQQQ"})");
		CHECK ( kRedactCredentials(sOnce) == sOnce );

		KString sBuffer = "unchanged";
		CHECK ( kRedactCredentials(sOnce, sBuffer) == false );
		CHECK ( sBuffer == "unchanged" );

		// EVERY PASS HAS TO BE IDEMPOTENT, not just the JSON one: a masked value no
		// longer starts with the marker, so each pass looks for it anywhere.
		for (KStringView sLine : {
			KStringView("Authorization: Bearer sk-live-abcdef1234567890"),
			KStringView("Authorization: DeepL-Auth-Key 00000000-1111-2222-3333-444444444444:fx"),
			KStringView("got token: eyJhbGciOiJIUzI1NiJ9.eyJzdWIiOiJhIn0.c2lnbmF0dXJlc2lnbmF0dXJl"),
			KStringView("https://bob:hunter2hunter2hunter2@example.com/path"),
			KStringView("GET /api?api_key=sk-proj-abcdefghijklmnop&x=1"),
			KStringView("update USERS set password='sk-proj-abcdefghijklmnop' where id=1"),
			KStringView(R"({"ApiTokens":["7f9c2b11-4a3e-4d55-9f10-abc123def456"]})"),
		})
		{
			auto sOnce  = kRedactCredentials(sLine);
			auto sTwice = kRedactCredentials(sOnce);
			INFO  ( sLine );
			CHECK ( sTwice == sOnce );
		}

		// A BACKTICK ENDS A VALUE. Slack and markdown wrap text in them, and the
		// xapis CLI wraps a whole message, so a span that swallowed them masked
		// the formatting instead of the key: "sk-...redacted...```".
		CHECK ( kRedactCredentials("```Authorization: Bearer sk-live-abcdef1234567890```")
		        ==                 "```Authorization: Bearer sk-...redacted...890```" );
		CHECK ( kRedactCredentials("`password=sk-proj-abcdefghijklmnop`")
		        ==                 "`password=sk-...redacted...nop`" );

		// AND THE TEST FOR IT IS BOUNDED TO ITS OWN VALUE: a masked field further
		// along must not answer for a live one before it.
		CHECK ( kRedactCredentials(R"({"b_token":"ccc...redacted...ddd","a_token":"livesecret123456"})")
		        ==                 R"({"b_token":"ccc...redacted...ddd","a_token":"liv...redacted...456"})" );
		CHECK ( kRedactCredentials(R"({"a_token":"livesecret123456","b_token":"ccc...redacted...ddd"})")
		        ==                 R"({"a_token":"liv...redacted...456","b_token":"ccc...redacted...ddd"})" );
	}

	SECTION("a JSON body that holds no credential")
	{
		KString sBuffer = "unchanged";

		// A NUMBER IS A COUNT, NOT A CREDENTIAL. The token ledger of a service that
		// reports its usage is full of these, and redacting them would corrupt the
		// figures it reports.
		CHECK ( kRedactCredentials(R"({"input_tokens":1500,"output_tokens":820,"tokens":2320})", sBuffer) == false );
		CHECK ( sBuffer == "unchanged" );

		// an identifier that ends with "_key" is a key in the database sense
		CHECK ( kRedactCredentials(R"({"project_key":"P-123","brand_key":"B-456"})", sBuffer) == false );
		CHECK ( sBuffer == "unchanged" );

		// an empty value, and a null
		CHECK ( kRedactCredentials(R"({"access_token":"","refresh_token":null})", sBuffer) == false );
		CHECK ( sBuffer == "unchanged" );

		// a timestamp is the common ':' of ordinary output, and costs one compare
		CHECK ( kRedactCredentials("Thu 18:26:57 : 59 model(s) ok", sBuffer) == false );
		CHECK ( sBuffer == "unchanged" );

		// AN ADDRESS AFTER A LABEL IS NOT THE USERINFO OF A URL. This shipped once:
		// every "email:", "from:" and "to:" in every redacted line lost its address.
		CHECK ( kRedactCredentials("env:PROD, email:joe.kuefler@example.com, done", sBuffer) == false );
		CHECK ( sBuffer == "unchanged" );
		CHECK ( kRedactCredentials("From: alice@example.com To: bob@example.com", sBuffer) == false );
		CHECK ( sBuffer == "unchanged" );

		// and the userinfo of a real URL still is one
		CHECK ( kRedactCredentials("https://user:secret@example.com/path") == "https://user:[redacted]@example.com/path" );
		CHECK ( kRedactCredentials("mysql://root:hunter2@db.internal:3306/x") == "mysql://root:[redacted]@db.internal:3306/x" );

		// redacting twice changes nothing the second time
		CHECK ( kRedactCredentials(R"({"access_token":"[redacted]"})", sBuffer) == false );
		CHECK ( sBuffer == "unchanged" );
	}
}

TEST_CASE("kRedactSQLCredentials")
{
	SECTION("the values of an insert statement")
	{
		// the form of an insert that KROW creates
		CHECK ( kRedactSQLCredentials("insert into USERS (\n\tname\n\t,password\n\t,email\n) values (\n\t'bob'\n\t,'hunter2'\n\t,'bob@example.com'\n)")
		        ==                    "insert into USERS (\n\tname\n\t,password\n\t,email\n) values (\n\t'bob'\n\t,'[redacted]'\n\t,'bob@example.com'\n)" );

		// several rows, identifier quotes, a unicode literal, an escaped quote
		CHECK ( kRedactSQLCredentials("INSERT INTO [USERS] ([name], [api_key]) VALUES ('bob', N'abc123'), ('ann', 'de''f456')")
		        ==                    "INSERT INTO [USERS] ([name], [api_key]) VALUES ('bob', N'[redacted]'), ('ann', '[redacted]')" );

		// a string in an expression, and null in a credential column
		CHECK ( kRedactSQLCredentials("insert into T (`id`, `client_secret`) values (1, aes_encrypt('s3cret', @k)), (2, null)")
		        ==                    "insert into T (`id`, `client_secret`) values (1, aes_encrypt('[redacted]', @k)), (2, null)" );

		// numbers in a column named like a credential, and a column list without
		// such a column
		KString sBuffer = "unchanged";
		CHECK ( kRedactSQLCredentials("insert into USAGE (input_tokens, output_tokens) values (1500, 820)", sBuffer) == false );
		CHECK ( kRedactSQLCredentials("insert into T (a, b) values ('x', 'y') on duplicate key update b=values(b)", sBuffer) == false );
		CHECK ( sBuffer == "unchanged" );
	}

	SECTION("the forms that kRedactCredentials recognizes")
	{
		CHECK ( kRedactSQLCredentials("update USERS set password='hunter2' where id=1")
		        ==                    "update USERS set password='[redacted]' where id=1" );
	}

	SECTION("a second pass changes nothing")
	{
		auto sOnce = kRedactSQLCredentials("insert into USERS (name, password) values ('bob', 'sk-proj-abcdefghijklmnop')");
		CHECK ( !sOnce.contains("defghijklm") );
		CHECK ( kRedactSQLCredentials(sOnce) == sOnce );
	}
}
