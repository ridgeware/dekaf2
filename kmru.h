
#pragma once

#include "bits/kconfiguration.h"

#ifdef DEKAF2_USE_KMRU_MULTI_INDEX
	#include "bits/kmru-multi-index.h"
#else
	#include "bits/kmru-std-vector.h"
#endif

