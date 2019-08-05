
#include "zbuild.h"
#include "wd_common.h"
#include "wd_detail.h"

void ZLIB_INTERNAL *wd_alloc_state(PREFIX3(streamp) strm, uInt items, uInt size)
{
    return ZALLOC(strm, ALIGN_UP(items * size, 8) + sizeof(struct wd_state), sizeof(unsigned char));
}

void ZLIB_INTERNAL wd_copy_state(void *dst, const void *src, uInt size)
{
    memcpy(dst, src, ALIGN_UP(size, 8) + sizeof(struct wd_state));
}
