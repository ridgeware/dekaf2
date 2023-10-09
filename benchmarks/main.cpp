#include <dekaf2/dekaf2.h>
#include <dekaf2/klog.h>
#include <dekaf2/kprof.h>

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
extern void kutf8_bench();
extern void kfindsetofchars_bench();

using namespace dekaf2;

int main()
{
	KLog::getInstance().SetName("bench");
	Dekaf::getInstance().SetMultiThreading();
	Dekaf::getInstance().SetUnicodeLocale();

	kxml_bench();
	khtmlparser_bench();
	kbitfields_bench();
	shared_ptr_bench();
	kprops_bench();
	kwriter_bench();
	kreader_bench();
	kfindsetofchars_bench();
	std_string_bench();
	kstring_bench();
	kstringview_bench();
	kcasestring_bench();
 	other_bench();
	kutf8_bench();

	kProfFinalize();

	return 0;
}
