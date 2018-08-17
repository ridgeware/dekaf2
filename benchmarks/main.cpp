#include <dekaf2/dekaf2.h>

extern void shared_ptr_bench();
extern void kprops_bench();
extern void kwriter_bench();
extern void kreader_bench();
extern void std_string_bench();
extern void kstring_bench();
extern void kstringview_bench();
extern void kcasestring_bench();

using namespace dekaf2;

int main()
{
	KLog().SetName("bench");
	Dekaf().SetMultiThreading();
	Dekaf().SetUnicodeLocale();

	shared_ptr_bench();
	kprops_bench();
	kwriter_bench();
	kreader_bench();
	std_string_bench();
	kstring_bench();
	kstringview_bench();
	kcasestring_bench();
	
	return 0;
}


