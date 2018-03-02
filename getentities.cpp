#include <dekaf2/kjson.h>
#include <dekaf2/kstream.h>
#include <dekaf2/khttpclient.h>
#include <iostream>


using namespace dekaf2;


int main(int argc, char** argv)
{
	KURL URL("http://www.w3.org/TR/html5/entities.json");
	std::unique_ptr<KConnection> cx = KConnection::Create(URL, false);
	KHTTPClient cHTTP(*cx);
	cHTTP.Resource(URL);
	cHTTP.Request();
	KString sHTML;
	cHTTP.Read(sHTML);

	KOutStream COut(std::cout);

	KJSON json;
	json.Parse(sHTML);
	
	COut << "\nconstexpr entity_t s_NamedEntitiesHTML4[] =\n{\n";

	auto it = json.cbegin();
	auto ie = json.cend();

	size_t iMaxLen { 0 };
	for (; it != ie; ++it)
	{
		iMaxLen = std::max(iMaxLen, it.key().size());
	}

	it = json.cbegin();

	for (; it != ie; ++it)
	{
		auto array = it.value()["codepoints"];
		KString sKey(it.key());
		if (sKey.front() == '&')
		{
			sKey.remove_prefix(1);
			if (sKey.back() == ';')
			{
				// we only want keys that start with & and end with ; (but we do not want the chars themselves)
				sKey.remove_suffix(1);
				sKey += '"';
				sKey.PadRight(iMaxLen + 1);
				KString sValue("0x");
				sValue += KString::to_hexstring(array[0]);
				sValue.PadLeft(8);

				COut << "\t{ \"" << sKey << ", " << sValue;

				if (array.size() == 2)
				{
					COut << ", 0x" << KString::to_hexstring(array[1]);
				}

				COut << " },\n";
			}
		}
	}

	COut << "};\n\n";

	return 0;
}
