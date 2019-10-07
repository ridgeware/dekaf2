#include <dekaf2/kjson.h>
#include <dekaf2/kstream.h>
#include <dekaf2/kwebclient.h>


using namespace dekaf2;


int main(int argc, char** argv)
{
	KString sHTML = kHTTPGet("http://www.w3.org/TR/html5/entities.json");

	KJSON json;
	kjson::Parse(json, sHTML);
	
	KOut << "\nconstexpr entity_map_t s_NamedEntitiesHTML4 =\n{\n";

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

				KOut << "\t{ \"" << sKey << ", { " << sValue;

				if (array.size() == 2)
				{
					KOut << ", 0x" << KString::to_hexstring(array[1]);
				}

				KOut << " }},\n";
			}
		}
	}

	KOut << "};\n\n";

	return 0;
}
