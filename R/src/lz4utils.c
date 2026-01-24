/* lz4utils.c - use LZ4 Frame API to read/write */
// LZ4frame API example : compress a file
// Based on sample code from Zbigniew Jędrzejewski-Szmek

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "lz4frame.h"

#define BUF_SIZE 512*1024
#define LZ4_HEADER_SIZE 19
#define LZ4_FOOTER_SIZE 4

size_t compress_buffer_to_lz4_file(unsigned char *data, int src_size, FILE *out)
{
  LZ4F_errorCode_t r;
  LZ4F_compressionContext_t ctx;
  char *src, *buf = NULL;
  size_t size, n, k, count_in = 0, count_out, offset = 0, frame_size;
  size_t remaining;

  LZ4F_preferences_t lz4_preferences;

  memset(&lz4_preferences, 0, sizeof(LZ4F_preferences_t));

  r = LZ4F_createCompressionContext(&ctx, LZ4F_VERSION);
  if (LZ4F_isError(r)) {
    return 0;
  }
  r = 0;

  /* Add in source size so we can decompress into single memory block */
  lz4_preferences.frameInfo.contentSize = src_size;

  frame_size = LZ4F_compressBound(BUF_SIZE, &lz4_preferences);
  size =  frame_size + LZ4_HEADER_SIZE + LZ4_FOOTER_SIZE;
  buf = malloc(size);
  if (!buf) goto cleanup;
  
  
  n = offset = count_out = LZ4F_compressBegin(ctx, buf, size, &lz4_preferences);
  if (LZ4F_isError(n)) goto cleanup;
  
  k = 0;
  src = data;
  while(count_in < (size_t) src_size) {
    remaining = src_size-count_in;
    if (remaining > BUF_SIZE) k = BUF_SIZE;
    else k = remaining;

    n = LZ4F_compressUpdate(ctx, buf + offset, size - offset, src, k, NULL);
    if (LZ4F_isError(n)) {
      goto cleanup;
    }

    offset += n;
    count_out += n;
    count_in += k;
    src+=BUF_SIZE;

    if (size - offset < frame_size + LZ4_FOOTER_SIZE) {
      k = fwrite(buf, 1, offset, out);
      if (k < offset) {
	goto cleanup;
      }
      offset = 0;
    }
  }

  n = LZ4F_compressEnd(ctx, buf + offset, size - offset, NULL);
  if (LZ4F_isError(n)) {
    goto cleanup;
  }

  offset += n;
  count_out += n;
  k = fwrite(buf, 1, offset, out);
  if (k < offset) {
    goto cleanup;
  }

  r = count_out;

 cleanup:
  if (ctx)
    LZ4F_freeCompressionContext(ctx);
  free(buf);
  return r;
}

static size_t get_block_size(const LZ4F_frameInfo_t* info)
{
  switch (info->blockSizeID) {
  case LZ4F_default:
  case LZ4F_max64KB:  return 1 << 16;
  case LZ4F_max256KB: return 1 << 18;
  case LZ4F_max1MB:   return 1 << 20;
  case LZ4F_max4MB:   return 1 << 22;
  default:
    return 0;
  }
}

int decompress_lz4_file_to_buffer(FILE *in, int *size, unsigned char **data)
{
  unsigned char* const src = malloc(BUF_SIZE);
  unsigned char* dst = NULL, *cur_dst;
  unsigned char *srcPtr, *srcEnd;
  size_t dstCapacity = 0;
  LZ4F_decompressionContext_t dctx;
  size_t ret, nbytes = 0;
  int status = 0;

  /* Initialization */
  if (!src) {
    goto cleanup;
  }
  ret = LZ4F_createDecompressionContext(&dctx, 100);
  if (LZ4F_isError(ret)) {
    goto cleanup;
  }


  /* Decompression */
  ret = 1;
  while (ret != 0) {
    /* Load more input */
    size_t srcSize = fread(src, 1, BUF_SIZE, in);
    srcPtr = src;
    srcEnd = srcPtr + srcSize;
    if (srcSize == 0 || ferror(in)) {
      goto cleanup;
    }
    /* Allocate destination buffer if it isn't already */
    if (!dst) {
      LZ4F_frameInfo_t info;

      ret = LZ4F_getFrameInfo(dctx, &info, src, &srcSize);
      if (LZ4F_isError(ret)) {
	goto cleanup;
      }

      /* for now, insiste content size was specified or bail */
      if (!info.contentSize) goto cleanup;
      dstCapacity = info.contentSize;
      nbytes = 0;
      dst = malloc((int) dstCapacity);
      if (!dst) { goto cleanup; }
      cur_dst = dst;
      srcPtr += srcSize;
      srcSize = srcEnd - srcPtr;
    }
    /* Decompress:
     * Continue while there is more input to read and the frame isn't over.
     * If srcPtr == srcEnd then we know that there is no more output left in the
     * internal buffer left to flush.
     */
    while (srcPtr != srcEnd && ret != 0) {
      /* INVARIANT: Any data left in dst has already been written */
      size_t dstSize = dstCapacity-nbytes;
      ret = LZ4F_decompress(dctx, cur_dst, &dstSize, srcPtr, &srcSize,
			    /* LZ4F_decompressOptions_t */ NULL);

      if (LZ4F_isError(ret)) {
	goto cleanup;
      }
      nbytes+=dstSize;
      cur_dst+=dstSize;

      /* Update input */
      srcPtr += srcSize;
      srcSize = srcEnd - srcPtr;
    }
  }
  /* Check that there isn't trailing input data after the frame.
   * It is valid to have multiple frames in the same file, but this example
   * doesn't support it.
   */
  ret = fread(src, 1, 1, in);
  if (ret != 0 || !feof(in)) {
    goto cleanup;
  }

  if (data) *data = dst;
  if (size) *size = nbytes;
  status = 1;
  
 cleanup:
  free(src);
  if (!data || (*data != dst)) free(dst);
  LZ4F_freeDecompressionContext(dctx);   /* note : free works on NULL */

  return status;
}


