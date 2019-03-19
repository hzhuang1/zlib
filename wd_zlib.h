#ifndef __WD_ZLIB_H
#define __WD_ZLIB_H

#include "zutil.h"
#include "wd.h"
#include "zip_usr_if.h"
#include "smm.h"


struct hw_ctl {
	struct wd_queue *q;
	int alg_type;
	int op_type;
	int stream_pos;
	void *next_in;
	void *next_out;
	void *ctx_buf;
	int ctx_dw0;
	int ctx_dw1;
	int ctx_dw2;
	void *next_in_pa;   /* next input byte */
	void *temp_in_pa;   /* temp input byte */
	void *next_out_pa;  /* next output byte should be put there */
	void *ss_buf;
	int isize;
	int checksum;
};

extern int hisi_deflateInit2_(z_stream *zstrm, int level, int method,
		     int windowBits, int memLevel, int strategy,
		     const char *version, int stream_size);
extern int hisi_deflate(z_stream *zstrm, int flush);
extern int hisi_deflateEnd(z_stream *zstrm);


extern int hisi_inflateInit2_(z_stream *zstrm, int windowBits,
		     const char *version, int stream_size);
extern int hisi_inflate(z_stream *zstrm, int flush);
extern int hisi_inflateEnd(z_stream *zstrm);

#endif
