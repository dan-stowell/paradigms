#include "layout_c0_w0.h"
#include "layout_c0_w1.h"
#include "layout_c1_w0.h"
#include "layout_c1_w1.h"
#include "layout_c2_w0.h"
#include "layout_c2_w1.h"
#include "layout_c3_w0.h"
#include "layout_c3_w1.h"
#include "reloc_s0.h"
#include "reloc_s1.h"
#include "reloc_s2.h"
static struct { const char *name; decltype(&layout_c0_w0) fn; } layouts[] = {
    {"layout_c0_w0", layout_c0_w0},
    {"layout_c0_w1", layout_c0_w1},
    {"layout_c1_w0", layout_c1_w0},
    {"layout_c1_w1", layout_c1_w1},
    {"layout_c2_w0", layout_c2_w0},
    {"layout_c2_w1", layout_c2_w1},
    {"layout_c3_w0", layout_c3_w0},
    {"layout_c3_w1", layout_c3_w1},
};
static const int nLayouts = sizeof layouts / sizeof layouts[0];
static struct { const char *name; decltype(&reloc_s0) fn; } relocs[] = {
    {"reloc_s0", reloc_s0},
    {"reloc_s1", reloc_s1},
    {"reloc_s2", reloc_s2},
};
static const int nRelocs = sizeof relocs / sizeof relocs[0];
