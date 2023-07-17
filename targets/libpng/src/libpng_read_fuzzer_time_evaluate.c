#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>

#include "time.h"



#define PNG_INTERNAL
#include "png.h"

#define PNG_CLEANUP \
  if(png_handler.png_ptr) \
  { \
    if (png_handler.row_ptr) \
      png_free(png_handler.png_ptr, png_handler.row_ptr); \
    if (png_handler.end_info_ptr) \
      png_destroy_read_struct(&png_handler.png_ptr, &png_handler.info_ptr,\
        &png_handler.end_info_ptr); \
    else if (png_handler.info_ptr) \
      png_destroy_read_struct(&png_handler.png_ptr, &png_handler.info_ptr,\
        NULL); \
    else \
      png_destroy_read_struct(&png_handler.png_ptr, NULL, NULL); \
    png_handler.png_ptr = NULL; \
    png_handler.row_ptr = NULL; \
    png_handler.info_ptr = NULL; \
    png_handler.end_info_ptr = NULL; \
  } \
  if (png_handler.buf_state) {\
    free(png_handler.buf_state); \
    png_handler.buf_state = NULL; \
  }


typedef struct {
  const uint8_t* data;
  size_t bytes_left;
}  BufState;


struct PngObjectHandler {
  png_infop info_ptr;
  png_structp png_ptr;
  png_infop end_info_ptr;
  png_voidp row_ptr;
  BufState* buf_state;
};

void user_read_data(png_structp png_ptr, png_bytep data, size_t length) {
  BufState* buf_state = (BufState *)png_get_io_ptr(png_ptr);
  if (length > buf_state->bytes_left) {
    png_error(png_ptr, "read error");
  }
  memcpy(data, buf_state->data, length);
  buf_state->bytes_left -= length;
  buf_state->data += length;
}



void* limited_malloc(png_structp a, png_alloc_size_t size) {
  // libpng may allocate large amounts of memory that the fuzzer reports as
  // an error. In order to silence these errors, make libpng fail when trying
  // to allocate a large amount. This allocator used to be in the Chromium
  // version of this fuzzer.
  // This number is chosen to match the default png_user_chunk_malloc_max.
  if (size > 8000000)
    return NULL;

  return malloc(size);
}

void default_free(png_structp a, png_voidp ptr) {
  return free(ptr);
}



extern void __sym_asan_print_profile();
static const int kPngHeaderSize = 8;

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (size < kPngHeaderSize) {
    return 0;
  }
  if (png_sig_cmp(data, 0, kPngHeaderSize)) {
    return 0;
  }
  struct PngObjectHandler png_handler;
  png_handler.png_ptr = NULL;
  png_handler.row_ptr = NULL;
  png_handler.info_ptr = NULL;
  png_handler.end_info_ptr = NULL;
  png_handler.png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
  if (!png_handler.png_ptr) {
    return 0;
  }

  png_handler.info_ptr = png_create_info_struct(png_handler.png_ptr);
  if (!png_handler.info_ptr) {
    PNG_CLEANUP
    return 0;
  }

  png_handler.end_info_ptr = png_create_info_struct(png_handler.png_ptr);
  if (!png_handler.end_info_ptr) {
    PNG_CLEANUP
    return 0;
  }

  png_set_mem_fn(png_handler.png_ptr, NULL, limited_malloc, default_free);

  png_set_crc_action(png_handler.png_ptr, PNG_CRC_QUIET_USE, PNG_CRC_QUIET_USE);
#ifdef PNG_IGNORE_ADLER32
  png_set_option(png_handler.png_ptr, PNG_IGNORE_ADLER32, PNG_OPTION_ON);
#endif


  // Setting up reading from buffer.
  png_handler.buf_state = (BufState *) malloc(sizeof(BufState));
  png_handler.buf_state->data = data + kPngHeaderSize;
  png_handler.buf_state->bytes_left = size - kPngHeaderSize;
  png_set_read_fn(png_handler.png_ptr, png_handler.buf_state, user_read_data);
  png_set_sig_bytes(png_handler.png_ptr, kPngHeaderSize);

  if (setjmp(png_jmpbuf(png_handler.png_ptr))) {
    PNG_CLEANUP
    return 0;
  }

  // Reading.
  png_read_info(png_handler.png_ptr, png_handler.info_ptr);

  // reset error handler to put png_deleter into scope.
  if (setjmp(png_jmpbuf(png_handler.png_ptr))) {
    PNG_CLEANUP
    return 0;
  }


  png_uint_32 width, height;
  int bit_depth, color_type, interlace_type, compression_type;
  int filter_type;

  if (!png_get_IHDR(png_handler.png_ptr, png_handler.info_ptr, &width,
                    &height, &bit_depth, &color_type, &interlace_type,
                    &compression_type, &filter_type)) {
    PNG_CLEANUP
    return 0;
  }

  // This is going to be too slow.
  if (width && height > 100000000 / width) {
    PNG_CLEANUP
    return 0;
  }

  printf("Set several transforms that browsers typically use:\n");
  // Set several transforms that browsers typically use:
  png_set_gray_to_rgb(png_handler.png_ptr);
  png_set_expand(png_handler.png_ptr);
  png_set_packing(png_handler.png_ptr);
  png_set_scale_16(png_handler.png_ptr);
  png_set_tRNS_to_alpha(png_handler.png_ptr);

  int passes = png_set_interlace_handling(png_handler.png_ptr);
  printf("png_read_update_info\n");
  png_read_update_info(png_handler.png_ptr, png_handler.info_ptr);

  png_handler.row_ptr = png_malloc(
      png_handler.png_ptr, png_get_rowbytes(png_handler.png_ptr,
                                            png_handler.info_ptr));
  printf("png_read_row:%d, %d\n", passes, height);
  clock_t  start, stop;
  double duration;
  for (int pass = 0; pass < passes; ++pass) {
    for (png_uint_32 y = 0; y < height; ++y) {
      start = clock();
      png_read_row(png_handler.png_ptr,
                   (png_bytep)png_handler.row_ptr, NULL);
      stop = clock();
      duration = ((double)(stop - start))/ CLOCKS_PER_SEC;
      printf("png_read_row end: %d, %f\n", y, duration);
      __sym_asan_print_profile();
    }
  }

  png_read_end(png_handler.png_ptr, png_handler.end_info_ptr);



  PNG_CLEANUP
  return 0;
}