#include "../../kstringview.h"

namespace dekaf2 {
namespace detail {

size_t kFindFirstOfNoSSE(
        KStringView haystack,
        KStringView needles,
        bool bNot);

size_t kFindLastOfNoSSE(
        KStringView haystack,
        KStringView needles,
        bool bNot);

size_t kFindFirstOfSSE(
        const KStringView haystack,
        const KStringView needles);

size_t kFindFirstNotOfSSE(
        const KStringView haystack,
        const KStringView needles);


size_t kFindLastOfSSE(
        const KStringView haystack,
        const KStringView needles);

size_t kFindLastNotOfSSE(
        const KStringView haystack,
        const KStringView needles);

} // end of namespace detail
} // end of namespace dekaf2
