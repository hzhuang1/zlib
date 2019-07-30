#include "zbuild.h"
#include "zutil.h"
#include "deflate.h"
#include "wd_deflate.h"

static inline int wd_are_params_ok(int level,
				   int strategy)
{
    return level && (strategy == Z_FIXED || strategy == Z_DEFAULT_STRATEGY);
}


int ZLIB_INTERNAL wd_deflate_params(PREFIX3(streamp) strm,
                                    int level,
                                    int strategy)
{
    int can_deflate = wd_are_params_ok(level, strategy);

    if (can_deflate)
	return Z_OK;

    return Z_STREAM_ERROR;
}

int ZLIB_INTERNAL wd_can_deflate(PREFIX3(streamp) strm)
{
    deflate_state *state = (deflate_state *)strm->state;

    /* Unsupported compression settings */
    if (!wd_are_params_ok(state->level, state->strategy))
        return 0;
    return 1;
}

int ZLIB_INTERNAL wd_deflate(PREFIX3(streamp) strm, int flush, block_state *result)
{
    if (!wd_can_deflate(strm))
        return 0;
    /* return 0 for unsupportd case. return 1 for hardware handling case. */
    return 0;
}
