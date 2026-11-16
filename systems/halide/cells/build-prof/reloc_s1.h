#ifndef HALIDE_____reloc_s1_h
#define HALIDE_____reloc_s1_h
#include <stdint.h>

// Forward declarations of the types used in the interface
// to the Halide pipeline.
//
// For the definitions of these structs, include HalideRuntime.h

// Halide's representation of a multi-dimensional array.
// Halide::Runtime::Buffer is a more user-friendly wrapper
// around this. Its declaration is in HalideBuffer.h
struct halide_buffer_t;

// Metadata describing the arguments to the generated function.
// Used to construct calls to the _argv version of the function.
struct halide_filter_metadata_t;

#ifndef HALIDE_MUST_USE_RESULT
#ifdef __has_attribute
#if __has_attribute(nodiscard)
#define HALIDE_MUST_USE_RESULT [[nodiscard]]
#elif __has_attribute(warn_unused_result)
#define HALIDE_MUST_USE_RESULT __attribute__((warn_unused_result))
#else
#define HALIDE_MUST_USE_RESULT
#endif
#else
#define HALIDE_MUST_USE_RESULT
#endif
#endif

#ifndef HALIDE_FUNCTION_ATTRS
#define HALIDE_FUNCTION_ATTRS
#endif



#ifdef __cplusplus
extern "C" {
#endif

HALIDE_FUNCTION_ATTRS
int reloc_s1(struct halide_buffer_t *_bucket_buffer, struct halide_buffer_t *_addr_buffer, struct halide_buffer_t *_doff_buffer, struct halide_buffer_t *_dbin_buffer, struct halide_buffer_t *_rstart_buffer, struct halide_buffer_t *_roff_buffer, struct halide_buffer_t *_rsiz_buffer, struct halide_buffer_t *_rkind_buffer, struct halide_buffer_t *_radd_buffer, struct halide_buffer_t *_rtgt_buffer, struct halide_buffer_t *_addrAll_buffer, struct halide_buffer_t *_sect_buffer, uint32_t _imageBase, uint32_t _unreach, uint32_t _mapinit, struct halide_buffer_t *_image_buffer, struct halide_buffer_t *_nerr_buffer);

HALIDE_FUNCTION_ATTRS
int reloc_s1_argv(void **args);

HALIDE_FUNCTION_ATTRS
const struct halide_filter_metadata_t *reloc_s1_metadata();

#ifdef __cplusplus
}  // extern "C"
#endif

#endif
