#include <dekaf2/kjson.h>
#include <dekaf2/kstream.h>
#include <dekaf2/khttp.h>
#include <iostream>


using namespace dekaf2;


int main(int argc, char** argv)
{
	KURL URL("http://www.w3.org/TR/html5/entities.json");
//    KURL URL("https://w3c.github.io/i18n-drafts/articles/inline-bidi-markup/index.en");
	std::unique_ptr<KConnection> cx = KConnection::Create(URL, false);
	KHTTP cHTTP(*cx);
	cHTTP.Resource(URL);
	cHTTP.Request();
	KString sHTML;
	cHTTP.Read(sHTML);

	KOutStream COut(std::cout);

	LJSON json = LJSON::parse(sHTML);

	for (const auto& it : json)
	{
		std::cout << it << std::endl;
//		COut.FormatLine("{} : {}", it.key(), it.value());
	}

	std::cout << json.dump(1, '\t');

	return 0;
}
