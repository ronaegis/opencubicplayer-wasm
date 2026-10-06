#ifndef FLAC_ASSERT_SHIM_H
#define FLAC_ASSERT_SHIM_H

#include <assert.h>

#undef FLAC__ASSERT
#undef FLAC__ASSERT_DECLARATION
#define FLAC__ASSERT(x) ((void)0)
#define FLAC__ASSERT_DECLARATION(x) x

#endif
