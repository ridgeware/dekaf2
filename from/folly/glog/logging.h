#pragma once

// This file is here to satisfy include directives from folly.
// In many cases folly includes it, but does not actually
// call any logging functions. So we avoid patching folly
// by providing this dummy header.

// In other cases it is used for debug assumes, which we will
// happily disable as we encounter them.

#define DCHECK_LT(a, b)
#define DCHECK_LE(a, b)
#define DCHECK_GT(a, b)
#define DCHECK_GE(a, b)
#define DCHECK_EQ(a, b)
#define DCHECK(a)
