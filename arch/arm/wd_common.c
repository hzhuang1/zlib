
#include "zbuild.h"
#include "wd_common.h"
#include "wd_detail.h"

void ZLIB_INTERNAL *wd_alloc_state(PREFIX3(streamp) strm, uInt items, uInt size)
{
    return ZALLOC(strm, ALIGN_UP(items * size, 8) + sizeof(struct wd_state), sizeof(unsigned char));
}
