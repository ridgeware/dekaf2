#include <dekaf2/kjson.h>
#include <dekaf2/kstream.h>
#include <dekaf2/khttp.h>
#include <iostream>


using namespace dekaf2;


int main(int argc, char** argv)
{
	KURL URL("http://www.w3.org/TR/html5/entities.json");
//	KURL URL("https://w3c.github.io/i18n-drafts/articles/inline-bidi-markup/index.en");
	std::unique_ptr<KConnection> cx = KConnection::Create(URL, false);
	KHTTP cHTTP(*cx);
	cHTTP.Resource(URL);
	cHTTP.Request();
	KString sHTML;
	cHTTP.Read(sHTML);
	std::cout << sHTML << std::endl;
	std::cout.flush();
	std::cout << sHTML.size() << std::endl;
/*
	KOutStream COut(std::cout);

	KJSON json;
	json.Parse(sHTML);
	for (const auto& it : json.Object())
	{
		std::cout << it << std::endl;
//		COut.FormatLine("{} : {}", it.key(), it.value());
	}
*/
	return 0;
}
