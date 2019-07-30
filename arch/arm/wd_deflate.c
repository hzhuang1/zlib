#include "zbuild.h"
#include "zutil.h"
#include "deflate.h"
#include "wd_deflate.h"
#include "wd_detail.h"

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
    deflate_state *state = (deflate_state *)strm->state;
    struct wd_state *wd_state = GET_WD_STATE(state);
    struct hisi_param *param = &wd_state->param;

    if (!param->hw_avail || !wd_can_deflate(strm))
        return 0;
    /* return 0 for unsupportd case. return 1 for hardware handling case. */
    return 1;
}

void ZLIB_INTERNAL wd_deflate_reset(PREFIX3(streamp) strm, uInt size)
{
    deflate_state *state = (deflate_state *)strm->state;
    struct wd_state *wd_state = GET_WD_STATE(state);
    struct hisi_param *param = &wd_state->param;
    struct hisi_qm_priv *priv;
    int ret;

    if (state->wrap & 2) {
        param->alg_type = HW_ZLIB;
        wd_state->q.capa.alg = "zlib";
    } else {
        param->alg_type = HW_GZIP;
        wd_state->q.capa.alg = "gzip";
    }
    wd_state->q.capa.latency = 0;   /*todo..*/
    wd_state->q.capa.throughput = 0;
    priv = (struct hisi_qm_priv *)(char *)&wd_state->q.capa.priv;
    priv->sqe_size = sizeof(struct hisi_zip_sqe);
    priv->op_type = param->op_type = HW_DEFLATE;
    ret = wd_request_queue(&wd_state->q);
    if (ret) {
        fprintf(stderr, "Fail to request wd queue without hwacc!\n");
        param->hw_avail = 0;
	return;
    }
    param->hw_avail = 1;
}
