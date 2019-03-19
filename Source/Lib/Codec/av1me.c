/*
 * Copyright (c) 2016, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <limits.h>
#include <math.h>
#include <stdio.h>
#include "EbDefinitions.h"
#if ICOPY
#include "EbCodingUnit.h"
#include "av1me.h"
#include "EbPictureControlSet.h"
#include "EbSequenceControlSet.h"
#include "EbComputeSAD.h"
#include "aom_dsp_rtcd.h"


int av1_is_dv_valid(const MV dv,
    const MacroBlockD *xd, int mi_row, int mi_col,
    block_size bsize, int mib_size_log2);

void clamp_mv(
    MV *mv,
    int32_t min_col,
    int32_t max_col,
    int32_t min_row,
    int32_t max_row);

typedef struct dist_wtd_comp_params {
    int use_dist_wtd_comp_avg;
    int fwd_offset;
    int bck_offset;
} DIST_WTD_COMP_PARAMS;
typedef unsigned int(*aom_sad_fn_t)(const uint8_t *a, int a_stride,
    const uint8_t *b, int b_stride);

typedef unsigned int(*aom_sad_avg_fn_t)(const uint8_t *a, int a_stride,
    const uint8_t *b, int b_stride,
    const uint8_t *second_pred);

typedef void(*aom_copy32xn_fn_t)(const uint8_t *a, int a_stride, uint8_t *b,
    int b_stride, int n);

typedef void(*aom_sad_multi_d_fn_t)(const uint8_t *a, int a_stride,
    const uint8_t *const b_array[],
    int b_stride, unsigned int *sad_array);

typedef unsigned int(*aom_variance_fn_t)(const uint8_t *a, int a_stride,
    const uint8_t *b, int b_stride,
    unsigned int *sse);

typedef unsigned int(*aom_subpixvariance_fn_t)(const uint8_t *a, int a_stride,
    int xoffset, int yoffset,
    const uint8_t *b, int b_stride,
    unsigned int *sse);

typedef unsigned int(*aom_subp_avg_variance_fn_t)(
    const uint8_t *a, int a_stride, int xoffset, int yoffset, const uint8_t *b,
    int b_stride, unsigned int *sse, const uint8_t *second_pred);

typedef unsigned int(*aom_dist_wtd_sad_avg_fn_t)(
    const uint8_t *a, int a_stride, const uint8_t *b, int b_stride,
    const uint8_t *second_pred, const DIST_WTD_COMP_PARAMS *jcp_param);

typedef unsigned int(*aom_dist_wtd_subp_avg_variance_fn_t)(
    const uint8_t *a, int a_stride, int xoffset, int yoffset, const uint8_t *b,
    int b_stride, unsigned int *sse, const uint8_t *second_pred,
    const DIST_WTD_COMP_PARAMS *jcp_param);

typedef unsigned int(*aom_masked_sad_fn_t)(const uint8_t *src, int src_stride,
    const uint8_t *ref, int ref_stride,
    const uint8_t *second_pred,
    const uint8_t *msk, int msk_stride,
    int invert_mask);
typedef unsigned int(*aom_masked_subpixvariance_fn_t)(
    const uint8_t *src, int src_stride, int xoffset, int yoffset,
    const uint8_t *ref, int ref_stride, const uint8_t *second_pred,
    const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
typedef unsigned int(*aom_obmc_sad_fn_t)(const uint8_t *pred, int pred_stride,
    const int32_t *wsrc,
    const int32_t *msk);
typedef unsigned int(*aom_obmc_variance_fn_t)(const uint8_t *pred,
    int pred_stride,
    const int32_t *wsrc,
    const int32_t *msk,
    unsigned int *sse);
typedef unsigned int(*aom_obmc_subpixvariance_fn_t)(
    const uint8_t *pred, int pred_stride, int xoffset, int yoffset,
    const int32_t *wsrc, const int32_t *msk, unsigned int *sse);

typedef struct aom_variance_vtable {
    aom_sad_fn_t sdf;                   
    aom_variance_fn_t vf; 
    aom_sad_multi_d_fn_t sdx4df;         
} aom_variance_fn_ptr_t;

int av1_refining_search_sad(IntraBcContext  *x, MV *ref_mv, int error_per_bit,
    int search_range,
    const aom_variance_fn_ptr_t *fn_ptr,
    const MV *center_mv);

#if !(AOM_SAD_PORTING)

/* Sum the difference between every corresponding element of the buffers. */
static INLINE unsigned int sad(const uint8_t *a, int a_stride, const uint8_t *b,
    int b_stride, int width, int height) {
    int y, x;
    unsigned int sad = 0;

    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++) {
            sad += abs(a[x] - b[x]);
        }
        a += a_stride;
        b += b_stride;

    }
    return sad;

}
#if 1
 #if FIX_SAD
 #define sadMxh(m)                                                          \
  unsigned int aom_sad##m##xh_c(const uint8_t *a, int a_stride,            \
                                const uint8_t *b, int b_stride, int width, \
                                int height) {                              \
    return NxMSadKernelSubSampled_funcPtrArray[ASM_AVX2][width >> 3]((uint8_t *)a, a_stride, (uint8_t *)b, b_stride, height,width);  \
  }
#define sadMxNx4D(m, n)                                                    \
  void aom_sad##m##x##n##x4d_c(const uint8_t *src, int src_stride,         \
                               const uint8_t *const ref_array[],           \
                               int ref_stride, uint32_t *sad_array) {      \
    int i;                                                                 \
    for (i = 0; i < 4; ++i) {                                              \
      sad_array[i] =                                                       \
          NxMSadKernelSubSampled_funcPtrArray[ASM_AVX2][m >> 3]((uint8_t *)src, src_stride, (uint8_t *)(ref_array[i]), ref_stride, n, m);   \
    }                                                                      \
  }
#define sadMxN(m, n)                                                          \
  unsigned int aom_sad##m##x##n##_c(const uint8_t *src, int src_stride,       \
                                    const uint8_t *ref, int ref_stride) {     \
return NxMSadKernelSubSampled_funcPtrArray[ASM_AVX2][m >> 3]((uint8_t *)src, src_stride, (uint8_t *)ref, ref_stride, n, m);  \
}
#else
#define sadMxh(m)                                                          \
  unsigned int aom_sad##m##xh_c(const uint8_t *a, int a_stride,            \
                                const uint8_t *b, int b_stride, int width, \
                                int height) {                              \
    return NxMSadKernelSubSampled_funcPtrArray[ASM_AVX2][width >> 3]((uint8_t *)a, a_stride, (uint8_t *)b, b_stride, width, height);  \
  }
#define sadMxNx4D(m, n)                                                    \
  void aom_sad##m##x##n##x4d_c(const uint8_t *src, int src_stride,         \
                               const uint8_t *const ref_array[],           \
                               int ref_stride, uint32_t *sad_array) {      \
    int i;                                                                 \
    for (i = 0; i < 4; ++i) {                                              \
      sad_array[i] =                                                       \
          NxMSadKernelSubSampled_funcPtrArray[ASM_AVX2][m >> 3]((uint8_t *)src, src_stride, (uint8_t *)(ref_array[i]), ref_stride, m, n);   \
    }                                                                      \
  }
#define sadMxN(m, n)                                                          \
  unsigned int aom_sad##m##x##n##_c(const uint8_t *src, int src_stride,       \
                                    const uint8_t *ref, int ref_stride) {     \
return NxMSadKernelSubSampled_funcPtrArray[ASM_AVX2][m >> 3]((uint8_t *)src, src_stride, (uint8_t *)ref, ref_stride, m, n);  \
}
#endif
#else
#define sadMxh(m)                                                          \
  unsigned int aom_sad##m##xh_c(const uint8_t *a, int a_stride,            \
                                const uint8_t *b, int b_stride, int width, \
                                int height) {                              \
    return sad(a, a_stride, b, b_stride, width, height);                   \
  }

#define sadMxN(m, n)                                                          \
  unsigned int aom_sad##m##x##n##_c(const uint8_t *src, int src_stride,       \
                                    const uint8_t *ref, int ref_stride) {     \
   return sad(src, src_stride, ref, ref_stride, m, n);    \
  }                                                                           \


// Calculate sad against 4 reference locations and store each in sad_array
#define sadMxNx4D(m, n)                                                    \
  void aom_sad##m##x##n##x4d_c(const uint8_t *src, int src_stride,         \
                               const uint8_t *const ref_array[],           \
                               int ref_stride, uint32_t *sad_array) {      \
    int i;                                                                 \
    for (i = 0; i < 4; ++i) {                                              \
      sad_array[i] =                                                       \
          aom_sad##m##x##n##_c(src, src_stride, ref_array[i], ref_stride); \
    }                                                                      \
  }
#endif
// 128x128
sadMxN(128, 128);
sadMxNx4D(128, 128);
// 128x64
sadMxN(128, 64);
sadMxNx4D(128, 64);
// 64x128
sadMxN(64, 128);
sadMxNx4D(64, 128);
// 64x64
sadMxN(64, 64);
sadMxNx4D(64, 64);
// 64x32
sadMxN(64, 32);
sadMxNx4D(64, 32);
// 32x64
sadMxN(32, 64);
sadMxNx4D(32, 64);
// 32x32
sadMxN(32, 32);
sadMxNx4D(32, 32);
// 32x16
sadMxN(32, 16);
sadMxNx4D(32, 16);
// 16x32
sadMxN(16, 32);
sadMxNx4D(16, 32);
// 16x16
sadMxN(16, 16);
sadMxNx4D(16, 16);
// 16x8
sadMxN(16, 8);
sadMxNx4D(16, 8);
// 8x16
sadMxN(8, 16);
sadMxNx4D(8, 16);
// 8x8
sadMxN(8, 8);
sadMxNx4D(8, 8);
// 8x4
sadMxN(8, 4);
sadMxNx4D(8, 4);
// 4x8
sadMxN(4, 8);
sadMxNx4D(4, 8);
// 4x4
sadMxN(4, 4);
sadMxNx4D(4, 4);

sadMxh(128);
sadMxh(64);
sadMxh(32);
sadMxh(16);
sadMxh(8);
sadMxh(4);

sadMxN(4, 16);
sadMxNx4D(4, 16);
sadMxN(16, 4);
sadMxNx4D(16, 4);
sadMxN(8, 32);
sadMxNx4D(8, 32);
sadMxN(32, 8);
sadMxNx4D(32, 8);
sadMxN(16, 64);
sadMxNx4D(16, 64);
sadMxN(64, 16);
sadMxNx4D(64, 16);

static void variance(const uint8_t *a, int a_stride, const uint8_t *b,
    int b_stride, int w, int h, uint32_t *sse, int *sum) {
    int i, j;

    *sum = 0;
    *sse = 0;

    for (i = 0; i < h; ++i) {
        for (j = 0; j < w; ++j) {
            const int diff = a[j] - b[j];
            *sum += diff;
            *sse += diff * diff;
        }

        a += a_stride;
        b += b_stride;
    }
}

#define VAR(W, H)                                                    \
  uint32_t aom_variance##W##x##H##_c(const uint8_t *a, int a_stride, \
                                     const uint8_t *b, int b_stride, \
                                     uint32_t *sse) {                \
    int sum;                                                         \
    variance(a, a_stride, b, b_stride, W, H, sse, &sum);             \
    return *sse - (uint32_t)(((int64_t)sum * sum) / (W * H));        \
  }

VAR(128, 128)
VAR(128, 64)
VAR(64, 128)
VAR(64, 64)
VAR(64, 32)
VAR(32, 64)
VAR(32, 32)
VAR(32, 16)
VAR(16, 32)
VAR(16, 16)
VAR(16, 8)
VAR(8, 16)
VAR(8, 8)
VAR(8, 4)
VAR(4, 8)
VAR(4, 4)
VAR(4, 2)
VAR(2, 4)
VAR(2, 2)
VAR(4, 16)
VAR(16, 4)
VAR(8, 32)
VAR(32, 8)
VAR(16, 64)
VAR(64, 16)

#endif //!(AOM_SAD_PORTING)

aom_variance_fn_ptr_t mefn_ptr[BlockSizeS_ALL];

void init_fn_ptr(void)
{
#define BFP0(BT, SDF, VF, SDX4DF)                            \
  mefn_ptr[BT].sdf = SDF;                                    \
  mefn_ptr[BT].vf = VF;                                      \
  mefn_ptr[BT].sdx4df = SDX4DF;

#if AOM_SAD_PORTING
        BFP0(BLOCK_4X16, aom_sad4x16, aom_variance4x16, aom_sad4x16x4d)
        BFP0(BLOCK_16X4, aom_sad16x4, aom_variance16x4, aom_sad16x4x4d)
        BFP0(BLOCK_8X32, aom_sad8x32, aom_variance8x32, aom_sad8x32x4d)
        BFP0(BLOCK_32X8, aom_sad32x8, aom_variance32x8, aom_sad32x8x4d)
        BFP0(BLOCK_16X64, aom_sad16x64, aom_variance16x64, aom_sad16x64x4d)
        BFP0(BLOCK_64X16, aom_sad64x16, aom_variance64x16, aom_sad64x16x4d)
        BFP0(BLOCK_128X128, aom_sad128x128, aom_variance128x128, aom_sad128x128x4d)
        BFP0(BLOCK_128X64, aom_sad128x64, aom_variance128x64, aom_sad128x64x4d)
        BFP0(BLOCK_64X128, aom_sad64x128, aom_variance64x128, aom_sad64x128x4d)
        BFP0(BLOCK_32X16, aom_sad32x16, aom_variance32x16, aom_sad32x16x4d)
        BFP0(BLOCK_16X32, aom_sad16x32, aom_variance16x32, aom_sad16x32x4d)
        BFP0(BLOCK_64X32, aom_sad64x32, aom_variance64x32, aom_sad64x32x4d)
        BFP0(BLOCK_32X64, aom_sad32x64, aom_variance32x64, aom_sad32x64x4d)
        BFP0(BLOCK_32X32, aom_sad32x32, aom_variance32x32, aom_sad32x32x4d)
        BFP0(BLOCK_64X64, aom_sad64x64, aom_variance64x64, aom_sad64x64x4d)
        BFP0(BLOCK_16X16, aom_sad16x16, aom_variance16x16, aom_sad16x16x4d)
        BFP0(BLOCK_16X8, aom_sad16x8, aom_variance16x8, aom_sad16x8x4d)
        BFP0(BLOCK_8X16, aom_sad8x16, aom_variance8x16, aom_sad8x16x4d)
        BFP0(BLOCK_8X8, aom_sad8x8, aom_variance8x8, aom_sad8x8x4d)
        BFP0(BLOCK_8X4, aom_sad8x4, aom_variance8x4, aom_sad8x4x4d)
        BFP0(BLOCK_4X8, aom_sad4x8, aom_variance4x8, aom_sad4x8x4d)
        BFP0(BLOCK_4X4, aom_sad4x4, aom_variance4x4, aom_sad4x4x4d)
#else
       BFP0(BLOCK_4X16, aom_sad4x16_c, aom_variance4x16_c, aom_sad4x16x4d_c)

       BFP0(BLOCK_16X4, aom_sad16x4_c, aom_variance16x4_c, aom_sad16x4x4d_c)

       BFP0(BLOCK_8X32, aom_sad8x32_c, aom_variance8x32_c, aom_sad8x32x4d_c)

       BFP0(BLOCK_32X8, aom_sad32x8_c, aom_variance32x8_c, aom_sad32x8x4d_c)

       BFP0(BLOCK_16X64, aom_sad16x64_c, aom_variance16x64_c, aom_sad16x64x4d_c)

       BFP0(BLOCK_64X16, aom_sad64x16_c, aom_variance64x16_c, aom_sad64x16x4d_c)

       BFP0(BLOCK_128X128, aom_sad128x128_c, aom_variance128x128_c, aom_sad128x128x4d_c)

       BFP0(BLOCK_128X64, aom_sad128x64_c, aom_variance128x64_c, aom_sad128x64x4d_c)

       BFP0(BLOCK_64X128, aom_sad64x128_c, aom_variance64x128_c, aom_sad64x128x4d_c)

       BFP0(BLOCK_32X16, aom_sad32x16_c, aom_variance32x16_c, aom_sad32x16x4d_c)

       BFP0(BLOCK_16X32, aom_sad16x32_c, aom_variance16x32_c, aom_sad16x32x4d_c)

       BFP0(BLOCK_64X32, aom_sad64x32_c, aom_variance64x32_c, aom_sad64x32x4d_c)

       BFP0(BLOCK_32X64, aom_sad32x64_c, aom_variance32x64_c, aom_sad32x64x4d_c)

       BFP0(BLOCK_32X32, aom_sad32x32_c, aom_variance32x32_c, aom_sad32x32x4d_c)

       BFP0(BLOCK_64X64, aom_sad64x64_c, aom_variance64x64_c, aom_sad64x64x4d_c)

       BFP0(BLOCK_16X16, aom_sad16x16_c, aom_variance16x16_c, aom_sad16x16x4d_c)

       BFP0(BLOCK_16X8, aom_sad16x8_c, aom_variance16x8_c, aom_sad16x8x4d_c)

       BFP0(BLOCK_8X16, aom_sad8x16_c, aom_variance8x16_c, aom_sad8x16x4d_c)

       BFP0(BLOCK_8X8, aom_sad8x8_c, aom_variance8x8_c, aom_sad8x8x4d_c)

       BFP0(BLOCK_8X4, aom_sad8x4_c, aom_variance8x4_c, aom_sad8x4x4d_c)

       BFP0(BLOCK_4X8, aom_sad4x8_c, aom_variance4x8_c, aom_sad4x8x4d_c)

       BFP0(BLOCK_4X4, aom_sad4x4_c, aom_variance4x4_c, aom_sad4x4x4d_c)

#endif
}


// #define NEW_DIAMOND_SEARCH

static INLINE const uint8_t *get_buf_from_mv(const struct buf_2d *buf,
                                             const MV *mv) {
  return &buf->buf[mv->row * buf->stride + mv->col];
}

void av1_set_mv_search_range(MvLimits *mv_limits, const MV *mv) {
  int col_min = (mv->col >> 3) - MAX_FULL_PEL_VAL + (mv->col & 7 ? 1 : 0);
  int row_min = (mv->row >> 3) - MAX_FULL_PEL_VAL + (mv->row & 7 ? 1 : 0);
  int col_max = (mv->col >> 3) + MAX_FULL_PEL_VAL;
  int row_max = (mv->row >> 3) + MAX_FULL_PEL_VAL;

  col_min = AOMMAX(col_min, (MV_LOW >> 3) + 1);
  row_min = AOMMAX(row_min, (MV_LOW >> 3) + 1);
  col_max = AOMMIN(col_max, (MV_UPP >> 3) - 1);
  row_max = AOMMIN(row_max, (MV_UPP >> 3) - 1);

  // Get intersection of UMV window and valid MV window to reduce # of checks
  // in diamond search.
  if (mv_limits->col_min < col_min) mv_limits->col_min = col_min;
  if (mv_limits->col_max > col_max) mv_limits->col_max = col_max;
  if (mv_limits->row_min < row_min) mv_limits->row_min = row_min;
  if (mv_limits->row_max > row_max) mv_limits->row_max = row_max;
}



MV_JOINT_TYPE av1_get_mv_joint(const MV *mv);

static INLINE int mv_cost(const MV *mv, const int *joint_cost,
                          int *const comp_cost[2]) {
  return joint_cost[av1_get_mv_joint(mv)] + comp_cost[0][mv->row] +
         comp_cost[1][mv->col];
}


#define PIXEL_TRANSFORM_ERROR_SCALE 4
static int mv_err_cost(const MV *mv, const MV *ref, const int *mvjcost,
                       int *mvcost[2], int error_per_bit) {
  if (mvcost) {
    const MV diff = { mv->row - ref->row, mv->col - ref->col };
    return (int)ROUND_POWER_OF_TWO_64(
        (int64_t)mv_cost(&diff, mvjcost, mvcost) * error_per_bit,
        RDDIV_BITS + AV1_PROB_COST_SHIFT - RD_EPB_SHIFT +
            PIXEL_TRANSFORM_ERROR_SCALE);
  }
  return 0;
}

static int mvsad_err_cost(const IntraBcContext *x, const MV *mv, const MV *ref,
                          int sad_per_bit) {
  const MV diff = { (mv->row - ref->row) * 8, (mv->col - ref->col) * 8 };
  return ROUND_POWER_OF_TWO(
      (unsigned)mv_cost(&diff, x->nmv_vec_cost, x->mv_cost_stack) * sad_per_bit,
      AV1_PROB_COST_SHIFT);
}

void av1_init3smotion_compensation(search_site_config *cfg, int stride) {
  int len, ss_count = 1;

  cfg->ss[0].mv.col = cfg->ss[0].mv.row = 0;
  cfg->ss[0].offset = 0;

  for (len = MAX_FIRST_STEP; len > 0; len /= 2) {
    // Generate offsets for 8 search sites per step.
    const MV ss_mvs[8] = { { -len, 0 },   { len, 0 },     { 0, -len },
                           { 0, len },    { -len, -len }, { -len, len },
                           { len, -len }, { len, len } };
    int i;
    for (i = 0; i < 8; ++i) {
      search_site *const ss = &cfg->ss[ss_count++];
      ss->mv = ss_mvs[i];
      ss->offset = ss->mv.row * stride + ss->mv.col;
    }
  }

  cfg->ss_count = ss_count;
  cfg->searches_per_step = 8;
}

static INLINE int check_bounds(const MvLimits *mv_limits, int row, int col,
                               int range) {
  return ((row - range) >= mv_limits->row_min) &
         ((row + range) <= mv_limits->row_max) &
         ((col - range) >= mv_limits->col_min) &
         ((col + range) <= mv_limits->col_max);
}

static INLINE int is_mv_in(const MvLimits *mv_limits, const MV *mv) {
  return (mv->col >= mv_limits->col_min) && (mv->col <= mv_limits->col_max) &&
         (mv->row >= mv_limits->row_min) && (mv->row <= mv_limits->row_max);
}

#define CHECK_BETTER                                                      \
  {                                                                       \
    if (thissad < bestsad) {                                              \
      if (use_mvcost)                                                     \
        thissad += mvsad_err_cost(x, &this_mv, &fcenter_mv, sad_per_bit); \
      if (thissad < bestsad) {                                            \
        bestsad = thissad;                                                \
        best_site = i;                                                    \
      }                                                                   \
    }                                                                     \
  }

#define MAX_PATTERN_SCALES 11
#define MAX_PATTERN_CANDIDATES 8  // max number of canddiates per scale
#define PATTERN_CANDIDATES_REF 3  // number of refinement candidates

int av1_get_mvpred_var(const IntraBcContext *x, const MV *best_mv,
                       const MV *center_mv, const aom_variance_fn_ptr_t *vfp,
                       int use_mvcost) {
   
  const struct buf_2d *const what = &x->plane[0].src;
  const struct buf_2d *const in_what = &x->xdplane[0].pre[0];
  const MV mv = { best_mv->row * 8, best_mv->col * 8 };
  unsigned int unused;

  return vfp->vf(what->buf, what->stride, get_buf_from_mv(in_what, best_mv),
                 in_what->stride, &unused) +
         (use_mvcost ? mv_err_cost(&mv, center_mv, x->nmv_vec_cost,
                                   x->mv_cost_stack, x->errorperbit)
                     : 0);
}

// Exhuastive motion search around a given centre position with a given
// step size.
static int exhuastive_mesh_search(IntraBcContext  *x, MV *ref_mv, MV *best_mv,
                                  int range, int step, int sad_per_bit,
                                  const aom_variance_fn_ptr_t *fn_ptr,
                                  const MV *center_mv) {
   
  const struct buf_2d *const what = &x->plane[0].src;
  const struct buf_2d *const in_what = &x->xdplane[0].pre[0];
  MV fcenter_mv = { center_mv->row, center_mv->col };
  unsigned int best_sad = INT_MAX;
  int r, c, i;
  int start_col, end_col, start_row, end_row;
  int col_step = (step > 1) ? step : 4;

  assert(step >= 1);

  clamp_mv(&fcenter_mv, x->mv_limits.col_min, x->mv_limits.col_max,
           x->mv_limits.row_min, x->mv_limits.row_max);
  *best_mv = fcenter_mv;
  best_sad =
      fn_ptr->sdf(what->buf, what->stride,
                  get_buf_from_mv(in_what, &fcenter_mv), in_what->stride) +
      mvsad_err_cost(x, &fcenter_mv, ref_mv, sad_per_bit);
  start_row = AOMMAX(-range, x->mv_limits.row_min - fcenter_mv.row);
  start_col = AOMMAX(-range, x->mv_limits.col_min - fcenter_mv.col);
  end_row = AOMMIN(range, x->mv_limits.row_max - fcenter_mv.row);
  end_col = AOMMIN(range, x->mv_limits.col_max - fcenter_mv.col);

  for (r = start_row; r <= end_row; r += step) {
    for (c = start_col; c <= end_col; c += col_step) {
      // Step > 1 means we are not checking every location in this pass.
      if (step > 1) {
        const MV mv = { fcenter_mv.row + r, fcenter_mv.col + c };
        unsigned int sad =
            fn_ptr->sdf(what->buf, what->stride, get_buf_from_mv(in_what, &mv),
                        in_what->stride);
        if (sad < best_sad) {
          sad += mvsad_err_cost(x, &mv, ref_mv, sad_per_bit);
          if (sad < best_sad) {
            best_sad = sad;
            x->second_best_mv.as_mv = *best_mv;
            *best_mv = mv;
          }
        }
      } else {
        // 4 sads in a single call if we are checking every location
        if (c + 3 <= end_col) {
          unsigned int sads[4];
          const uint8_t *addrs[4];
          for (i = 0; i < 4; ++i) {
            const MV mv = { fcenter_mv.row + r, fcenter_mv.col + c + i };
            addrs[i] = get_buf_from_mv(in_what, &mv);
          }
          fn_ptr->sdx4df(what->buf, what->stride, addrs, in_what->stride, sads);

          for (i = 0; i < 4; ++i) {
            if (sads[i] < best_sad) {
              const MV mv = { fcenter_mv.row + r, fcenter_mv.col + c + i };
              const unsigned int sad =
                  sads[i] + mvsad_err_cost(x, &mv, ref_mv, sad_per_bit);
              if (sad < best_sad) {
                best_sad = sad;
                x->second_best_mv.as_mv = *best_mv;
                *best_mv = mv;
              }
            }
          }
        } else {
          for (i = 0; i < end_col - c; ++i) {
            const MV mv = { fcenter_mv.row + r, fcenter_mv.col + c + i };
            unsigned int sad =
                fn_ptr->sdf(what->buf, what->stride,
                            get_buf_from_mv(in_what, &mv), in_what->stride);
            if (sad < best_sad) {
              sad += mvsad_err_cost(x, &mv, ref_mv, sad_per_bit);
              if (sad < best_sad) {
                best_sad = sad;
                x->second_best_mv.as_mv = *best_mv;
                *best_mv = mv;
              }
            }
          }
        }
      }
    }
  }

  return best_sad;
}


int av1_diamond_search_sad_c(IntraBcContext  *x, const search_site_config *cfg,
                             MV *ref_mv, MV *best_mv, int search_param,
                             int sad_per_bit, int *num00,
                             const aom_variance_fn_ptr_t *fn_ptr,
                             const MV *center_mv) {
  int i, j, step;

 
  uint8_t *what = x->plane[0].src.buf;
  const int what_stride = x->plane[0].src.stride;
  const uint8_t *in_what;
  const int in_what_stride = x->xdplane[0].pre[0].stride;
  const uint8_t *best_address;

  unsigned int bestsad = INT_MAX;
  int best_site = 0;
  int last_site = 0;

  int ref_row;
  int ref_col;

  // search_param determines the length of the initial step and hence the number
  // of iterations.
  // 0 = initial step (MAX_FIRST_STEP) pel
  // 1 = (MAX_FIRST_STEP/2) pel,
  // 2 = (MAX_FIRST_STEP/4) pel...
  const search_site *ss = &cfg->ss[search_param * cfg->searches_per_step];
  const int tot_steps = (cfg->ss_count / cfg->searches_per_step) - search_param;

  const MV fcenter_mv = { center_mv->row >> 3, center_mv->col >> 3 };
  clamp_mv(ref_mv, x->mv_limits.col_min, x->mv_limits.col_max,
           x->mv_limits.row_min, x->mv_limits.row_max);
  ref_row = ref_mv->row;
  ref_col = ref_mv->col;
  *num00 = 0;
  best_mv->row = ref_row;
  best_mv->col = ref_col;

  // Work out the start point for the search
  in_what = x->xdplane[0].pre[0].buf + ref_row * in_what_stride + ref_col;
  best_address = in_what;

  // Check the starting position
  bestsad = fn_ptr->sdf(what, what_stride, in_what, in_what_stride) +
            mvsad_err_cost(x, best_mv, &fcenter_mv, sad_per_bit);

  i = 1;

  for (step = 0; step < tot_steps; step++) {
    int all_in = 1, t;

    // All_in is true if every one of the points we are checking are within
    // the bounds of the image.
    all_in &= ((best_mv->row + ss[i].mv.row) > x->mv_limits.row_min);
    all_in &= ((best_mv->row + ss[i + 1].mv.row) < x->mv_limits.row_max);
    all_in &= ((best_mv->col + ss[i + 2].mv.col) > x->mv_limits.col_min);
    all_in &= ((best_mv->col + ss[i + 3].mv.col) < x->mv_limits.col_max);

    // If all the pixels are within the bounds we don't check whether the
    // search point is valid in this loop,  otherwise we check each point
    // for validity..
    if (all_in) {
      unsigned int sad_array[4];

      for (j = 0; j < cfg->searches_per_step; j += 4) {
        unsigned char const *block_offset[4];

        for (t = 0; t < 4; t++)
          block_offset[t] = ss[i + t].offset + best_address;

        fn_ptr->sdx4df(what, what_stride, block_offset, in_what_stride,
                       sad_array);

        for (t = 0; t < 4; t++, i++) {
          if (sad_array[t] < bestsad) {
            const MV this_mv = { best_mv->row + ss[i].mv.row,
                                 best_mv->col + ss[i].mv.col };
            sad_array[t] +=
                mvsad_err_cost(x, &this_mv, &fcenter_mv, sad_per_bit);
            if (sad_array[t] < bestsad) {
              bestsad = sad_array[t];
              best_site = i;
            }
          }
        }
      }
    } else {
      for (j = 0; j < cfg->searches_per_step; j++) {
        // Trap illegal vectors
        const MV this_mv = { best_mv->row + ss[i].mv.row,
                             best_mv->col + ss[i].mv.col };

        if (is_mv_in(&x->mv_limits, &this_mv)) {
          const uint8_t *const check_here = ss[i].offset + best_address;
          unsigned int thissad =
              fn_ptr->sdf(what, what_stride, check_here, in_what_stride);

          if (thissad < bestsad) {
            thissad += mvsad_err_cost(x, &this_mv, &fcenter_mv, sad_per_bit);
            if (thissad < bestsad) {
              bestsad = thissad;
              best_site = i;
            }
          }
        }
        i++;
      }
    }
    if (best_site != last_site) {
      x->second_best_mv.as_mv = *best_mv;
      best_mv->row += ss[best_site].mv.row;
      best_mv->col += ss[best_site].mv.col;
      best_address += ss[best_site].offset;
      last_site = best_site;
#if defined(NEW_DIAMOND_SEARCH)
      while (1) {
        const MV this_mv = { best_mv->row + ss[best_site].mv.row,
                             best_mv->col + ss[best_site].mv.col };
        if (is_mv_in(&x->mv_limits, &this_mv)) {
          const uint8_t *const check_here = ss[best_site].offset + best_address;
          unsigned int thissad =
              fn_ptr->sdf(what, what_stride, check_here, in_what_stride);
          if (thissad < bestsad) {
            thissad += mvsad_err_cost(x, &this_mv, &fcenter_mv, sad_per_bit);
            if (thissad < bestsad) {
              bestsad = thissad;
              best_mv->row += ss[best_site].mv.row;
              best_mv->col += ss[best_site].mv.col;
              best_address += ss[best_site].offset;
              continue;
            }
          }
        }
        break;
      }
#endif
    } else if (best_address == in_what) {
      (*num00)++;
    }
  }
  return bestsad;
}



/* do_refine: If last step (1-away) of n-step search doesn't pick the center
              point as the best match, we will do a final 1-away diamond
              refining search  */
static int full_pixel_diamond(PictureControlSet_t *pcs, IntraBcContext /*MACROBLOCK*/ *x,
                              MV *mvp_full, int step_param, int sadpb,
                              int further_steps, int do_refine, int *cost_list,
                              const aom_variance_fn_ptr_t *fn_ptr,
                              const MV *ref_mv) {
  MV temp_mv;
  int thissme, n, num00 = 0;
  (void)cost_list;
  /*int bestsme = cpi->diamond_search_sad(x, &cpi->ss_cfg, mvp_full, &temp_mv,
                                        step_param, sadpb, &n, fn_ptr, ref_mv);*/
  int bestsme = av1_diamond_search_sad_c(x, &pcs->ss_cfg, mvp_full, &temp_mv,
      step_param, sadpb, &n, fn_ptr, ref_mv);

  if (bestsme < INT_MAX)
    bestsme = av1_get_mvpred_var(x, &temp_mv, ref_mv, fn_ptr, 1);
  x->best_mv.as_mv = temp_mv;

  // If there won't be more n-step search, check to see if refining search is
  // needed.
  if (n > further_steps) do_refine = 0;

  while (n < further_steps) {
    ++n;

    if (num00) {
      num00--;
    } else {
      /*thissme = cpi->diamond_search_sad(x, &cpi->ss_cfg, mvp_full, &temp_mv,
                                        step_param + n, sadpb, &num00, fn_ptr,
                                        ref_mv);*/
      thissme = av1_diamond_search_sad_c(x, &pcs->ss_cfg, mvp_full, &temp_mv,
          step_param + n, sadpb, &num00, fn_ptr,
          ref_mv);

      if (thissme < INT_MAX)
        thissme = av1_get_mvpred_var(x, &temp_mv, ref_mv, fn_ptr, 1);

      // check to see if refining search is needed.
      if (num00 > further_steps - n) do_refine = 0;

      if (thissme < bestsme) {
        bestsme = thissme;
        x->best_mv.as_mv = temp_mv;
      }
    }
  }

  // final 1-away diamond refining search
  if (do_refine) {
    const int search_range = 8;
    MV best_mv = x->best_mv.as_mv;
    thissme = av1_refining_search_sad(x, &best_mv, sadpb, search_range, fn_ptr,
                                      ref_mv);
    if (thissme < INT_MAX)
      thissme = av1_get_mvpred_var(x, &best_mv, ref_mv, fn_ptr, 1);
    if (thissme < bestsme) {
      bestsme = thissme;
      x->best_mv.as_mv = best_mv;
    }
  }

  // Return cost list.
 /* if (cost_list) {
    calc_int_cost_list(x, ref_mv, sadpb, fn_ptr, &x->best_mv.as_mv, cost_list);
  }*/
  return bestsme;
}

#define MIN_RANGE 7
#define MAX_RANGE 256
#define MIN_INTERVAL 1
// Runs an limited range exhaustive mesh search using a pattern set
// according to the encode speed profile.
static int full_pixel_exhaustive(PictureControlSet_t *pcs, IntraBcContext  *x,
                                 const MV *centre_mv_full, int sadpb,
                                 int *cost_list,
                                 const aom_variance_fn_ptr_t *fn_ptr,
                                 const MV *ref_mv, MV *dst_mv) {
    UNUSED(cost_list);
    const SPEED_FEATURES *const sf = &pcs->sf;// cpi->sf;
  MV temp_mv = { centre_mv_full->row, centre_mv_full->col };
  MV f_ref_mv = { ref_mv->row >> 3, ref_mv->col >> 3 };
  int bestsme;
  int i;
  int interval = sf->mesh_patterns[0].interval;
  int range = sf->mesh_patterns[0].range;
  int baseline_interval_divisor;

  // Keep track of number of exhaustive calls (this frame in this thread).
  //CHKN if (x->ex_search_count_ptr != NULL) ++(*x->ex_search_count_ptr);

  // Trap illegal values for interval and range for this function.
  if ((range < MIN_RANGE) || (range > MAX_RANGE) || (interval < MIN_INTERVAL) ||
      (interval > range))
    return INT_MAX;

  baseline_interval_divisor = range / interval;

  // Check size of proposed first range against magnitude of the centre
  // value used as a starting point.
  range = AOMMAX(range, (5 * AOMMAX(abs(temp_mv.row), abs(temp_mv.col))) / 4);
  range = AOMMIN(range, MAX_RANGE);
  interval = AOMMAX(interval, range / baseline_interval_divisor);

  // initial search
  bestsme = exhuastive_mesh_search(x, &f_ref_mv, &temp_mv, range, interval,
                                   sadpb, fn_ptr, &temp_mv);

  if ((interval > MIN_INTERVAL) && (range > MIN_RANGE)) {
    // Progressive searches with range and step size decreasing each time
    // till we reach a step size of 1. Then break out.
    for (i = 1; i < MAX_MESH_STEP; ++i) {
      // First pass with coarser step and longer range
      bestsme = exhuastive_mesh_search(
          x, &f_ref_mv, &temp_mv, sf->mesh_patterns[i].range,
          sf->mesh_patterns[i].interval, sadpb, fn_ptr, &temp_mv);

      if (sf->mesh_patterns[i].interval == 1) break;
    }
  }

  if (bestsme < INT_MAX)
    bestsme = av1_get_mvpred_var(x, &temp_mv, ref_mv, fn_ptr, 1);
  *dst_mv = temp_mv;

  // Return cost list.
 /* if (cost_list) {
    calc_int_cost_list(x, ref_mv, sadpb, fn_ptr, dst_mv, cost_list);
  }*/
  return bestsme;
}


int av1_refining_search_sad(IntraBcContext  *x, MV *ref_mv, int error_per_bit,
                            int search_range,
                            const aom_variance_fn_ptr_t *fn_ptr,
                            const MV *center_mv) {
  
  const MV neighbors[4] = { { -1, 0 }, { 0, -1 }, { 0, 1 }, { 1, 0 } };
  const struct buf_2d *const what = &x->plane[0].src;
  const struct buf_2d *const in_what = &x->xdplane[0].pre[0];
  const MV fcenter_mv = { center_mv->row >> 3, center_mv->col >> 3 };
  const uint8_t *best_address = get_buf_from_mv(in_what, ref_mv);
  unsigned int best_sad =
      fn_ptr->sdf(what->buf, what->stride, best_address, in_what->stride) +
      mvsad_err_cost(x, ref_mv, &fcenter_mv, error_per_bit);
  int i, j;

  for (i = 0; i < search_range; i++) {
    int best_site = -1;
    const int all_in = ((ref_mv->row - 1) > x->mv_limits.row_min) &
                       ((ref_mv->row + 1) < x->mv_limits.row_max) &
                       ((ref_mv->col - 1) > x->mv_limits.col_min) &
                       ((ref_mv->col + 1) < x->mv_limits.col_max);

    if (all_in) {
      unsigned int sads[4];
      const uint8_t *const positions[4] = { best_address - in_what->stride,
                                            best_address - 1, best_address + 1,
                                            best_address + in_what->stride };

      fn_ptr->sdx4df(what->buf, what->stride, positions, in_what->stride, sads);

      for (j = 0; j < 4; ++j) {
        if (sads[j] < best_sad) {
          const MV mv = { ref_mv->row + neighbors[j].row,
                          ref_mv->col + neighbors[j].col };
          sads[j] += mvsad_err_cost(x, &mv, &fcenter_mv, error_per_bit);
          if (sads[j] < best_sad) {
            best_sad = sads[j];
            best_site = j;
          }
        }
      }
    } else {
      for (j = 0; j < 4; ++j) {
        const MV mv = { ref_mv->row + neighbors[j].row,
                        ref_mv->col + neighbors[j].col };

        if (is_mv_in(&x->mv_limits, &mv)) {
          unsigned int sad =
              fn_ptr->sdf(what->buf, what->stride,
                          get_buf_from_mv(in_what, &mv), in_what->stride);
          if (sad < best_sad) {
            sad += mvsad_err_cost(x, &mv, &fcenter_mv, error_per_bit);
            if (sad < best_sad) {
              best_sad = sad;
              best_site = j;
            }
          }
        }
      }
    }

    if (best_site == -1) {
      break;
    } else {
      x->second_best_mv.as_mv = *ref_mv;
      ref_mv->row += neighbors[best_site].row;
      ref_mv->col += neighbors[best_site].col;
      best_address = get_buf_from_mv(in_what, ref_mv);
    }
  }

  return best_sad;
}

int av1_full_pixel_search(PictureControlSet_t *pcs, IntraBcContext  *x, block_size bsize,
                          MV *mvp_full, int step_param, int method,
                          int run_mesh_search, int error_per_bit,
                          int *cost_list, const MV *ref_mv, int var_max, int rd,
                          int x_pos, int y_pos, int intra) {
    UNUSED (run_mesh_search);
    UNUSED (var_max);
    UNUSED (rd);

#if IBC_MODES
    int32_t ibc_shift = 0;
    if (pcs->parent_pcs_ptr->ibc_mode > 0)
        ibc_shift = 1;
#endif

#if IBC_EARLY_0
    SPEED_FEATURES * sf = &pcs->sf;
    sf->exhaustive_searches_thresh = (1 << 25);
#else
  const SPEED_FEATURES *const sf = &pcs->sf;
#endif
  const aom_variance_fn_ptr_t *fn_ptr = &mefn_ptr[bsize];
  int var = 0;

  if (cost_list) {
    cost_list[0] = INT_MAX;
    cost_list[1] = INT_MAX;
    cost_list[2] = INT_MAX;
    cost_list[3] = INT_MAX;
    cost_list[4] = INT_MAX;
  }

  // Keep track of number of searches (this frame in this thread).
  //if (x->m_search_count_ptr != NULL) ++(*x->m_search_count_ptr);

  switch (method) {
    case FAST_DIAMOND:
      //var = fast_dia_search(x, mvp_full, step_param, error_per_bit, 0,
      //                      cost_list, fn_ptr, 1, ref_mv);
      break;
    case FAST_HEX:
      //var = fast_hex_search(x, mvp_full, step_param, error_per_bit, 0,
      //                      cost_list, fn_ptr, 1, ref_mv);
      break;
    case HEX:
      //var = av1_hex_search(x, mvp_full, step_param, error_per_bit, 1, cost_list,
      //                     fn_ptr, 1, ref_mv);
      break;
    case SQUARE:
      //var = square_search(x, mvp_full, step_param, error_per_bit, 1, cost_list,
      //                    fn_ptr, 1, ref_mv);
      break;
    case BIGDIA:
      //var = bigdia_search(x, mvp_full, step_param, error_per_bit, 1, cost_list,
      //                    fn_ptr, 1, ref_mv);
      break;
    case NSTEP:
      var = full_pixel_diamond(pcs, x, mvp_full, step_param, error_per_bit,
                               MAX_MVSEARCH_STEPS - 1 - step_param, 1,
                               cost_list, fn_ptr, ref_mv);

#if IBC_EARLY_0
      if (x->is_exhaustive_allowed)
      {
          int exhuastive_thr = sf->exhaustive_searches_thresh;
          exhuastive_thr >>=
              10 - (mi_size_wide_log2[bsize] + mi_size_high_log2[bsize]);

#if IBC_MODES
          exhuastive_thr = exhuastive_thr << ibc_shift;
#endif

          if (var > exhuastive_thr)
          {
              int var_ex;
              MV tmp_mv_ex;
              var_ex =
                  full_pixel_exhaustive(pcs, x, &x->best_mv.as_mv, error_per_bit,
                      cost_list, fn_ptr, ref_mv, &tmp_mv_ex);

              if (var_ex < var) {
                  var = var_ex;
                  x->best_mv.as_mv = tmp_mv_ex;
              }
          }
      }
#else

      // Should we allow a follow on exhaustive search?
      if(1)// (is_exhaustive_allowed(cpi, x))   
      {
        //int exhuastive_thr = sf->exhaustive_searches_thresh;
        //exhuastive_thr >>=
        //    10 - (mi_size_wide_log2[bsize] + mi_size_high_log2[bsize]);

        // Threshold variance for an exhaustive full search.
        //if (var > exhuastive_thr) 
        {
          int var_ex;
          MV tmp_mv_ex;
          var_ex =
              full_pixel_exhaustive(pcs, x, &x->best_mv.as_mv, error_per_bit,
                                    cost_list, fn_ptr, ref_mv, &tmp_mv_ex);

          if (var_ex < var) {
            var = var_ex;
            x->best_mv.as_mv = tmp_mv_ex;
          }
        }
      }
#endif
      break;
    default: assert(0 && "Invalid search method.");
  }

#if !IBC_EARLY_0 

  // Should we allow a follow on exhaustive search?
  if (!run_mesh_search) {
    if (method == NSTEP) {
      //if (is_exhaustive_allowed(cpi, x))
      {
        int exhuastive_thr = sf->exhaustive_searches_thresh;
        exhuastive_thr >>=
            10 - (mi_size_wide_log2[bsize] + mi_size_high_log2[bsize]);
        // Threshold variance for an exhaustive full search.
        if (var > exhuastive_thr) run_mesh_search = 1;
      }
    }
  }

  //if (run_mesh_search)
  if(1)
  {
    int var_ex;
    MV tmp_mv_ex;
    var_ex = full_pixel_exhaustive(pcs, x, &x->best_mv.as_mv, error_per_bit,
                                   cost_list, fn_ptr, ref_mv, &tmp_mv_ex);
    if (var_ex < var) {
      var = var_ex;
      x->best_mv.as_mv = tmp_mv_ex;
    }
  }

  if (method != NSTEP && rd && var < var_max)
    var = av1_get_mvpred_var(x, &x->best_mv.as_mv, ref_mv, fn_ptr, 1);

#endif

  do {
    //CHKN if (!intra || !av1_use_hash_me(&cpi->common)) break;

    // already single ME
    // get block size and original buffer of current block
    const int block_height = block_size_high[bsize];
    const int block_width = block_size_wide[bsize];
    if (block_height == block_width && x_pos >= 0 && y_pos >= 0) {
      if (block_width == 4 || block_width == 8 || block_width == 16 ||
          block_width == 32 || block_width == 64 || block_width == 128) {
        uint8_t *what = x->plane[0].src.buf;
        const int what_stride = x->plane[0].src.stride;
        uint32_t hash_value1, hash_value2;
        MV best_hash_mv;
        int best_hash_cost = INT_MAX;

        // for the hashMap
        hash_table *ref_frame_hash = &pcs->hash_table;

        av1_get_block_hash_value(what, what_stride, block_width, &hash_value1,
                                 &hash_value2, 0, pcs, x);

        const int count = av1_hash_table_count(ref_frame_hash, hash_value1);
        // for intra, at least one matching can be found, itself.
        if (count <= (intra ? 1 : 0)) {
          break;
        }
 
        Iterator iterator =
            av1_hash_get_first_iterator(ref_frame_hash, hash_value1);
        for (int i = 0; i < count; i++, iterator_increment(&iterator)) {
          block_hash ref_block_hash = *(block_hash *)(iterator_get(&iterator));
          if (hash_value2 == ref_block_hash.hash_value2) {
            // For intra, make sure the prediction is from valid area.
            if (intra) {
              const int mi_col = x_pos / MI_SIZE;
              const int mi_row = y_pos / MI_SIZE;
              const MV dv = { 8 * (ref_block_hash.y - y_pos),
                              8 * (ref_block_hash.x - x_pos) };
              if (!av1_is_dv_valid(dv, x->xd, mi_row, mi_col,
                                   bsize, pcs->parent_pcs_ptr->sequence_control_set_ptr->mib_size_log2))
                continue;
            }
            MV hash_mv;
            hash_mv.col = ref_block_hash.x - x_pos;
            hash_mv.row = ref_block_hash.y - y_pos;
            if (!is_mv_in(&x->mv_limits, &hash_mv)) continue;
            const int refCost =
                av1_get_mvpred_var(x, &hash_mv, ref_mv, fn_ptr, 1);
            if (refCost < best_hash_cost) {
              best_hash_cost = refCost;
              best_hash_mv = hash_mv;
            }
          }
        }

        if (best_hash_cost < var) {
          x->second_best_mv = x->best_mv;
          x->best_mv.as_mv = best_hash_mv;
          var = best_hash_cost;
        }


      }
    }
  } while (0);


  return 0;//CHKN  var;
}

#endif
