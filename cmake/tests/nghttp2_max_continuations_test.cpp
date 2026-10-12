#define NGHTTP2_NO_SSIZE_T 1
#include <nghttp2/nghttp2.h>

// nghttp2 1.61.0 introduced the limit of CONTINUATION frames (CVE-2024-28182)
// together with nghttp2_option_set_max_continuations(). Distributions backported
// both to older versions, so the declaration is checked, not the version. The
// test is not linked against nghttp2, so the function is only named in an
// unevaluated context.

int main()
{
	using SetMaxContinuations = decltype(&nghttp2_option_set_max_continuations);
	SetMaxContinuations Function = nullptr;

	return Function ? 1 : 0;
}
