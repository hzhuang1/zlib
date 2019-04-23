// SPDX-License-Identifier: GPL-2.0+
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <unistd.h>
#include <assert.h>
#include "wd_zlib.h"

#define DEFLATE 0
#define INFLATE 1

#define ASIZE (2*512*1024)	/*512K*/

#define SYS_ERR_COND(cond, msg, ...) \
do { \
	if (cond) { \
		if (errno) \
			perror(msg); \
		else \
			fprintf(stderr, msg, ##__VA_ARGS__); \
		exit(EXIT_FAILURE); \
	} \
} while (0)

struct hisi_qm_priv {
	__u16 sqe_size;
	__u16 op_type;
};

#define ALG_ZLIB 0
#define ALG_GZIP 1

#define HW_CTX_SIZE (64*1024)

#define Z_OK            0
#define Z_STREAM_END    1
#define Z_STREAM_NULL   3
#define Z_STREAM_NO_FINSH   4
#define Z_ERRNO (-1)

#define STREAM_CHUNK_OUT (64*1024)

#define swab32(x) \
	((((x) & 0x000000ff) << 24) | \
	(((x) & 0x0000ff00) <<  8) | \
	(((x) & 0x00ff0000) >>  8) | \
	(((x) & 0xff000000) >> 24))

#define cpu_to_be32(x) swab32(x)

static int hw_init(z_stream *zstrm, int alg_type, int comp_optype);
static void hw_end(z_stream *zstrm);
static int hw_send_and_recv(z_stream *zstrm, int flush);

#define HZLIB_VERSION "1.2.11"
#define ZLIB_HEAD_SIZE 2

const static char zlib_head[ZLIB_HEAD_SIZE] = {0x78, 0x9c};
static int stream_chunk = 1024*64;

int hisi_deflateInit2_(z_stream *zstrm, int level, int method, int windowBits,
		       int memLevel, int strategy, const char *version,
		       int stream_size)
{
	int alg_type;
	int wrap = 0;

	if (windowBits < 0) { /* suppress zlib wrapper */
		wrap = 0;
		windowBits = -windowBits;
	}
	else if (windowBits > 15) {
		wrap = 2;		/* write gzip wrapper instead */
		windowBits -= 16;
	}
	if (wrap & 0x02)
		alg_type = ALG_GZIP;
	else
		alg_type = ALG_ZLIB;

	zstrm->total_in = 0;
	zstrm->total_out = 0;

	return hw_init(zstrm, alg_type, HW_DEFLATE);

}

int hisi_deflateInit_(z_stream *zstrm, int level,
		      const char *version, int stream_size)
{
	return hisi_deflateInit2_(zstrm, level, Z_DEFLATED, MAX_WBITS,
				  DEF_MEM_LEVEL, Z_DEFAULT_STRATEGY,
				  version, stream_size);
}

int hisi_flowctl(z_stream *zstrm, int flush)
{
	int ret = Z_OK;
	struct hw_ctl *hw_ctl = (struct hw_ctl *)zstrm->reserved;

	if (zstrm->avail_in) {
		if (hw_ctl->flowctl == 0) {
			ret = hw_send_and_recv(zstrm, flush);
			if (ret == Z_STREAM_NO_FINSH) {
				zstrm->avail_in = 0;
				return Z_OK;
			}
			if (ret == Z_STREAM_NULL) {
				zstrm->avail_in = 0;
				return Z_STREAM_END;
			}
			hw_ctl->next_out_temp = hw_ctl->next_out;
			hw_ctl->outlen = stream_chunk - hw_ctl->avail_out;
		}
		if (hw_ctl->outlen > zstrm->avail_out) {
			/* need to copy*/
			memcpy(zstrm->next_out, hw_ctl->next_out_temp,
			       zstrm->avail_out);
			hw_ctl->outlen -= zstrm->avail_out;
			hw_ctl->next_out_temp += zstrm->avail_out;
			zstrm->avail_out = 0;
			hw_ctl->flowctl = 1;
		} else if (hw_ctl->outlen > 0 &&
			   hw_ctl->outlen <= zstrm->avail_out) {
			/* need to copy*/
			memcpy(zstrm->next_out,
			       hw_ctl->next_out_temp,
			       hw_ctl->outlen);
			zstrm->avail_out -= hw_ctl->outlen;
			zstrm->avail_in = 0;
			hw_ctl->flowctl = 0;
		}
	} else {
		ret = Z_STREAM_END;
	}

	if (flush == Z_FINISH)
		ret = Z_STREAM_END;

	return ret;
}

int hisi_deflate(z_stream *zstrm, int flush)
{
	return hisi_flowctl(zstrm, flush);
}

int hisi_deflateEnd(z_stream *zstrm)
{
	hw_end(zstrm);
	return 0;
}

int hisi_inflateInit2_(z_stream *zstrm, int windowBits,
		       const char *version, int stream_size)
{
	int wrap, alg_type;

	/* extract wrap request from windowBits parameter */
	if (windowBits < 0) {
		wrap = 0;
		windowBits = -windowBits;
	} else {
		wrap = (windowBits >> 4) + 5;

	}
	if (wrap & 0x01)
		alg_type = ALG_ZLIB;
	if (wrap & 0x02)
		alg_type = ALG_GZIP;

	zstrm->total_in = 0;
	zstrm->total_out = 0;

	return hw_init(zstrm, alg_type, HW_INFLATE);
}

int hisi_inflateInit_(z_stream *zstrm, const char *version, int stream_size)
{
	return inflateInit2_(zstrm, DEF_WBITS, version, stream_size);
}

int hisi_inflate(z_stream *zstrm, int flush)
{
	return hisi_flowctl(zstrm, flush);
}

int hisi_inflateEnd(z_stream *zstrm)
{
	hw_end(zstrm);
	return 0;
}

static int hw_init(z_stream *zstrm, int alg_type, int comp_optype)
{
	int ret = -1;
	size_t ss_region_size;
	struct hisi_qm_priv *priv;
	struct hw_ctl *hw_ctl;

	hw_ctl = calloc(1, sizeof(struct hw_ctl));
	if (!hw_ctl) {
		fputs("alloc hw_ctl fail!\n", stderr);
		return -1;
	}
	hw_ctl->q = calloc(1, sizeof(struct wd_queue));
	if (!hw_ctl->q) {
		fputs("alloc hw_ctl->q fail!\n", stderr);
		goto hwctl_free;
	}

	switch (alg_type) {
	case 0:
		hw_ctl->alg_type = HW_ZLIB;
		hw_ctl->q->capa.alg = "zlib";
		break;
	case 1:
		hw_ctl->alg_type = HW_GZIP;
		hw_ctl->q->capa.alg = "gzip";
		break;
	default:
		hw_ctl->alg_type = HW_ZLIB;
		hw_ctl->q->capa.alg = "zlib";
	}
	hw_ctl->q->capa.latency = 0;   /*todo..*/
	hw_ctl->q->capa.throughput = 0;
	priv = (struct hisi_qm_priv *)hw_ctl->q->capa.priv;
	priv->sqe_size = sizeof(struct hisi_zip_sqe);
	priv->op_type = hw_ctl->op_type = comp_optype;
	ret = wd_request_queue(hw_ctl->q);
	if (ret) {
		fprintf(stderr, "wd request queue fail, no hwacc!\n");
		goto hwctl_q_free;
	}

	ss_region_size = 4096+ASIZE*2+HW_CTX_SIZE;
#ifdef CONFIG_IOMMU_SVA
		hw_ctl->ss_buf = malloc(ss_region_size);
#else
		hw_ctl->ss_buf = wd_reserve_memory(hw_ctl->q, ss_region_size);
#endif
	if (!hw_ctl->ss_buf) {
		fprintf(stderr, "fail to reserve %ld dmabuf\n", ss_region_size);
		ret = -ENOMEM;
		goto release_q;
	}

	ret = smm_init(hw_ctl->ss_buf, ss_region_size, 0xF);
	if (ret)
		goto buf_free;

	hw_ctl->next_in = NULL;
	hw_ctl->next_out = NULL;

	hw_ctl->next_in = smm_alloc(hw_ctl->ss_buf, ASIZE);
	hw_ctl->next_out = smm_alloc(hw_ctl->ss_buf, ASIZE);
	hw_ctl->ctx_buf = smm_alloc(hw_ctl->ss_buf, HW_CTX_SIZE);

	if (!hw_ctl->next_in || !hw_ctl->next_out) {
		dbg("not enough data ss_region memory for cache 1 (bs=%d)\n",
			ASIZE);
			goto buf_free;
	}

	if (hw_ctl->q->dev_flags & UACCE_DEV_NOIOMMU) {
		hw_ctl->next_in_pa = wd_get_pa_from_va(hw_ctl->q,
						       hw_ctl->next_in);
		hw_ctl->next_out_pa = wd_get_pa_from_va(hw_ctl->q,
							hw_ctl->next_out);
		hw_ctl->ctx_buf = wd_get_pa_from_va(hw_ctl->q, hw_ctl->ctx_buf);
	} else {
		hw_ctl->next_in_pa = hw_ctl->next_in;
		hw_ctl->next_out_pa = hw_ctl->next_out;
	}

	hw_ctl->temp_in_pa = hw_ctl->next_in_pa;
	hw_ctl->stream_pos = STREAM_NEW;
	zstrm->reserved = hw_ctl;

	return Z_OK;
buf_free:
#ifdef CONFIG_IOMMU_SVA
	if (hw_ctl->ss_buf)
		free(hw_ctl->ss_buf);
#endif
release_q:
	wd_release_queue(hw_ctl->q);
hwctl_q_free:
	free(hw_ctl->q);
hwctl_free:
	free(hw_ctl);

	return ret;
}

static void hw_end(z_stream *zstrm)
{
	struct hw_ctl *hw_ctl = (struct hw_ctl *)zstrm->reserved;

#ifdef CONFIG_IOMMU_SVA
	if (hw_ctl->ss_buf)
		free(hw_ctl->ss_buf);
#endif

	wd_release_queue(hw_ctl->q);
	free(hw_ctl->q);
	free(hw_ctl);
}

static unsigned int bit_reverse(register unsigned int x)
{
	x = (((x & 0xaaaaaaaa) >> 1) | ((x & 0x55555555) << 1));
	x = (((x & 0xcccccccc) >> 2) | ((x & 0x33333333) << 2));
	x = (((x & 0xf0f0f0f0) >> 4) | ((x & 0x0f0f0f0f) << 4));
	x = (((x & 0xff00ff00) >> 8) | ((x & 0x00ff00ff) << 8));

	return((x >> 16) | (x << 16));
}

/* output an empty store block */
static int append_store_block(z_stream *zstrm, int flush)
{
	char store_block[5] = {0x1, 0x00, 0x00, 0xff, 0xff};
	struct hw_ctl *hw_ctl = (struct hw_ctl *)zstrm->reserved;
	__u32 checksum = hw_ctl->checksum;
	__u32 isize = hw_ctl->isize;

	if (flush != Z_FINISH)
		return Z_STREAM_NO_FINSH;
	memcpy(zstrm->next_out, store_block, 5);
	zstrm->total_out += 5;
	zstrm->avail_out -= 5;
	if (hw_ctl->alg_type == HW_ZLIB) { /*if zlib, ADLER32*/
		checksum = (__u32) cpu_to_be32(checksum);
		memcpy(zstrm->next_out + 5, &checksum, 4);
		zstrm->total_out += 4;
		zstrm->avail_out -= 4;
	} else if (hw_ctl->alg_type == HW_GZIP) {  /*if gzip, CRC32 and ISIZE*/
		checksum = ~checksum;
		checksum = bit_reverse(checksum);
		memcpy(zstrm->next_out + 5, &checksum, 4);
		memcpy(zstrm->next_out + 9, &isize, 4);
		zstrm->total_out += 8;
		zstrm->avail_out -= 8;
	} else
		fprintf(stderr, "in append store block, wrong alg type %d.\n",
			hw_ctl->alg_type);

	return Z_STREAM_NULL;
}

static int hw_send_and_recv(z_stream *zstrm, int flush)
{
	struct hisi_zip_sqe *msg, *recv_msg;
	struct hw_ctl *hw_ctl = (struct hw_ctl *)zstrm->reserved;
	int ret = 0;
	__u32 status, type;
	__u64 stream_mode, stream_new, flush_type;

	if (zstrm->avail_in == 0)
		return append_store_block(zstrm, flush);

	msg = calloc(1, sizeof(*msg));
	if (!msg) {
		fputs("alloc msg fail!\n", stderr);
		goto msg_free;
	}
	hw_ctl->avail_in = zstrm->avail_in;
	hw_ctl->avail_out = stream_chunk;
	memcpy(hw_ctl->next_in, zstrm->next_in, zstrm->avail_in);/* need copy */
	stream_mode = STATEFUL;
	stream_new = hw_ctl->stream_pos;
	flush_type = (flush == Z_FINISH) ? HZ_FINISH : HZ_SYNC_FLUSH;
	if (hw_ctl->stream_pos == STREAM_NEW) {
		if (hw_ctl->op_type == HW_DEFLATE) {
			memcpy(zstrm->next_out, zlib_head, ZLIB_HEAD_SIZE);
			hw_ctl->total_out = ZLIB_HEAD_SIZE;
			zstrm->avail_out -= ZLIB_HEAD_SIZE;
			zstrm->next_out += ZLIB_HEAD_SIZE;
			zstrm->total_out += ZLIB_HEAD_SIZE;
		} else {
			hw_ctl->next_in_pa += ZLIB_HEAD_SIZE;
			hw_ctl->avail_in -= ZLIB_HEAD_SIZE;
			zstrm->total_in += ZLIB_HEAD_SIZE;
		}
		hw_ctl->stream_pos = STREAM_OLD;
	}
	msg->dw9 = hw_ctl->alg_type;
	msg->dw7 |= ((stream_new << 2 | stream_mode << 1 |
		    flush_type)) << STREAM_FLUSH_SHIFT;
	msg->source_addr_l = (__u64)hw_ctl->next_in_pa & 0xffffffff;
	msg->source_addr_h = (__u64)hw_ctl->next_in_pa >> 32;
	msg->dest_addr_l = (__u64)hw_ctl->next_out_pa & 0xffffffff;
	msg->dest_addr_h = (__u64)hw_ctl->next_out_pa >> 32;
	msg->input_data_length = hw_ctl->avail_in;
	msg->dest_avail_out = hw_ctl->avail_out;
	msg->stream_ctx_addr_l = (__u64)hw_ctl->ctx_buf & 0xffffffff;
	msg->stream_ctx_addr_h = (__u64)hw_ctl->ctx_buf >> 32;
	msg->ctx_dw0 = hw_ctl->ctx_dw0;
	msg->ctx_dw1 = hw_ctl->ctx_dw1;
	msg->ctx_dw2 = hw_ctl->ctx_dw2;
	msg->isize = hw_ctl->isize;
	msg->checksum = hw_ctl->checksum;

	ret = wd_send(hw_ctl->q, msg);
	if (ret == -EBUSY) {
		usleep(1);
		goto recv_again;
	}

	SYS_ERR_COND(ret, "send fail!\n");
recv_again:
	ret = wd_recv(hw_ctl->q, (void **)&recv_msg);
	if (ret == -EIO) {
		fputs(" wd_recv fail!\n", stderr);
		goto msg_free;
	/* synchronous mode, if get none, then get again */
	} else if (ret == -EAGAIN)
		goto recv_again;
	status = recv_msg->dw3 & 0xff;
	type = recv_msg->dw9 & 0xff;
	SYS_ERR_COND(status != 0 && status != 0x0d && status != 0x13,
		     "bad status (s=%d, t=%d)\n", status, type);
	hw_ctl->avail_out -= recv_msg->produced;
	hw_ctl->total_out += recv_msg->produced;
	hw_ctl->avail_in -= recv_msg->consumed;
	hw_ctl->ctx_dw0 = recv_msg->ctx_dw0;
	hw_ctl->ctx_dw1 = recv_msg->ctx_dw1;
	hw_ctl->ctx_dw2 = recv_msg->ctx_dw2;
	hw_ctl->isize = recv_msg->isize;
	hw_ctl->checksum = recv_msg->checksum;
	zstrm->total_in += recv_msg->consumed;
	zstrm->total_out += recv_msg->produced;
	if (hw_ctl->avail_in > 0) {
		/* zstrm->avail_out = 0; */
		hw_ctl->next_in_pa +=  recv_msg->consumed;
	}
	if (hw_ctl->avail_in == 0)
		hw_ctl->next_in_pa = hw_ctl->temp_in_pa;

	if (ret == 0 && flush == Z_FINISH)
		ret = Z_STREAM_END;
	else if (ret == 0 &&  (recv_msg->dw3 & 0x1ff) == 0x113)
		ret = Z_STREAM_END;    /* decomp_is_end  region */

msg_free:
	free(msg);
	return ret;
}
