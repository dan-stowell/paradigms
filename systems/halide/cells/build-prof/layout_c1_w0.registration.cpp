
// MACHINE GENERATED -- DO NOT EDIT

extern "C" {
struct halide_filter_metadata_t;
void halide_register_argv_and_metadata(
    int (*filter_argv_call)(void **),
    const struct halide_filter_metadata_t *filter_metadata,
    const char * const *extra_key_value_pairs
);
}

extern "C" {
extern int layout_c1_w0_argv(void **args);
extern const struct halide_filter_metadata_t *layout_c1_w0_metadata();
}

#ifdef HALIDE_REGISTER_EXTRA_KEY_VALUE_PAIRS_FUNC
extern "C" const char * const *HALIDE_REGISTER_EXTRA_KEY_VALUE_PAIRS_FUNC();
#endif  // HALIDE_REGISTER_EXTRA_KEY_VALUE_PAIRS_FUNC

namespace halide_nsreg_layout_c1_w0 {
namespace {
struct Registerer {
    Registerer() {
#ifdef HALIDE_REGISTER_EXTRA_KEY_VALUE_PAIRS_FUNC
        halide_register_argv_and_metadata(::layout_c1_w0_argv, ::layout_c1_w0_metadata(), HALIDE_REGISTER_EXTRA_KEY_VALUE_PAIRS_FUNC());
#else
        halide_register_argv_and_metadata(::layout_c1_w0_argv, ::layout_c1_w0_metadata(), nullptr);
#endif  // HALIDE_REGISTER_EXTRA_KEY_VALUE_PAIRS_FUNC
    }
};
static Registerer registerer;
}  // namespace
}  // halide_nsreg_layout_c1_w0

