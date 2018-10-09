#include <dekaf2/dekaf2.h>

extern void other_bench();
extern void kbitfields_bench();
extern void shared_ptr_bench();
extern void kprops_bench();
extern void kwriter_bench();
extern void kreader_bench();
extern void std_string_bench();
extern void kstring_bench();
extern void kstringview_bench();
extern void kcasestring_bench();
extern void kxml_bench();
extern void khtmlparser_bench();

using namespace dekaf2;

int main()
{
	KLog().SetName("bench");
	Dekaf().SetMultiThreading();
	Dekaf().SetUnicodeLocale();

	kxml_bench();
	khtmlparser_bench();
	kbitfields_bench();
	shared_ptr_bench();
	kprops_bench();
	kwriter_bench();
	kreader_bench();
	std_string_bench();
	kstring_bench();
	kstringview_bench();
	kcasestring_bench();
	other_bench();

	return 0;
}


