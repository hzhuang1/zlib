#ifndef WD_DETAIL_H
#define WD_DETAIL_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include "wd.h"
#include "zip_usr_if.h"
#include "smm.h"

#if 0
struct hw_ctl {
	struct wd_queue *q;
	unsigned empty_in : 1;		/* IN buffer is empty */
	unsigned empty_out : 1;		/* OUT buffer is empty */
	unsigned full_in : 1;		/* IN buffer is ready */
	unsigned pending_out : 1;	/* pending data for OUT buffer */
	unsigned stream_pos : 1;	/* STREAM_NEW or STREAM_OLD */
	unsigned stream_end : 1;
	int alg_type;
	int op_type;
	int avail_in;			/* number of bytes availiable in IN buffer */
	int avail_out;
	int inlen;			/* data cached in IN buffer */
	int outlen;			/* data cached in OUT buffer */
	void *in;
	void *out;
	void *in_pa;
	void *out_pa;
	void *next_in;
	void *next_out;
	void *ctx_buf;
	int ctx_dw0;
	int ctx_dw1;
	int ctx_dw2;
	void *ss_buf;
};
#endif

#define ASIZE                          (2 * 512 * 1024)    /* 512KB */
#define HW_CTX_SIZE                    (512 * 1024)
#define STREAM_CHUNK                   (512 * 1024)
#define MIN_STREAM_CHUNK               512

struct hisi_param {
    int alg_type;
    int op_type;
    void *in;
    void *out;
    void *in_pa;
    void *out_pa;
    void *ctx_buf;
    int ctx_dw0;
    int ctx_dw1;
    int ctx_dw2;
    void *ss_buf;

    int avail_in;
    int avail_out;
    int inlen;
    int outlen;
    void *next_in;
    void *next_out;

    unsigned hw_avail : 1;             /* HW accelerator is available */
    unsigned empty_in : 1;             /* IN buffer is empty */
    unsigned empty_out : 1;            /* OUT buffer is empty */
    unsigned full_in : 1;              /* IN buffer is ready */
    unsigned pending_out : 1;          /* pending data for OUT buffer */
    unsigned stream_pos : 1;           /* STREAM_NEW or STREAM_OLD */
    unsigned stream_end : 1;
};

struct hisi_qm_priv {
    __u16 sqe_size;
    __u16 op_type;
};

/*
   Extension of inflate_state and deflate_state. Must be doubleword-aligned.
*/
struct wd_state {
    struct wd_queue q;
    struct hisi_param param;
};

#define ALIGN_UP(p, size) (__typeof__(p))(((uintptr_t)(p) + ((size) - 1)) & ~((size) - 1))

#define GET_WD_STATE(state) ((struct wd_state *)((char *)(state) + ALIGN_UP(sizeof(*state), 8)))

#endif
