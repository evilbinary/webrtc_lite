/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <assert.h>
#include <math.h>
#include <stdio.h>

#include "./vp10_rtcd.h"

#include "vpx_dsp/vpx_dsp_common.h"
#include "vpx_mem/vpx_mem.h"
#include "vpx_ports/bitops.h"
#include "vpx_ports/mem.h"
#include "vpx_ports/system_state.h"

#include "vp10/common/common.h"
#include "vp10/common/entropy.h"
#include "vp10/common/entropymode.h"
#include "vp10/common/mvref_common.h"
#include "vp10/common/pred_common.h"
#include "vp10/common/quant_common.h"
#include "vp10/common/reconinter.h"
#include "vp10/common/reconintra.h"
#include "vp10/common/seg_common.h"

#include "vp10/encoder/cost.h"
#include "vp10/encoder/encodemb.h"
#include "vp10/encoder/encodemv.h"
#include "vp10/encoder/encoder.h"
#include "vp10/encoder/mcomp.h"
#include "vp10/encoder/quantize.h"
#include "vp10/encoder/ratectrl.h"
#include "vp10/encoder/rd.h"
#include "vp10/encoder/tokenize.h"

#define RD_THRESH_POW      1.25
#define RD_MULT_EPB_RATIO  64

// Factor to weigh the rate for switchable interp filters.
#define SWITCHABLE_INTERP_RATE_FACTOR 1

void vp10_rd_cost_reset(RD_COST *rd_cost) {
  rd_cost->rate = INT_MAX;
  rd_cost->dist = INT64_MAX;
  rd_cost->rdcost = INT64_MAX;
}

void vp10_rd_cost_init(RD_COST *rd_cost) {
  rd_cost->rate = 0;
  rd_cost->dist = 0;
  rd_cost->rdcost = 0;
}

// The baseline rd thresholds for breaking out of the rd loop for
// certain modes are assumed to be based on 8x8 blocks.
// This table is used to correct for block size.
// The factors here are << 2 (2 = x0.5, 32 = x8 etc).
static const uint8_t rd_thresh_block_size_factor[BLOCK_SIZES] = {
  2, 3, 3, 4, 6, 6, 8, 12, 12, 16, 24, 24, 32
};

static void fill_mode_costs(VP10_COMP *cpi) {
  const FRAME_CONTEXT *const fc = cpi->common.fc;
  int i, j;

  for (i = 0; i < INTRA_MODES; ++i)
    for (j = 0; j < INTRA_MODES; ++j)
      vp10_cost_tokens(cpi->y_mode_costs[i][j], vp10_kf_y_mode_prob[i][j],
                      vp10_intra_mode_tree);

  vp10_cost_tokens(cpi->mbmode_cost, fc->y_mode_prob[1], vp10_intra_mode_tree);
  for (i = 0; i < INTRA_MODES; ++i)
    vp10_cost_tokens(cpi->intra_uv_mode_cost[i],
                     fc->uv_mode_prob[i], vp10_intra_mode_tree);

  for (i = 0; i < SWITCHABLE_FILTER_CONTEXTS; ++i)
    vp10_cost_tokens(cpi->switchable_interp_costs[i],
                    fc->switchable_interp_prob[i], vp10_switchable_interp_tree);

  for (i = TX_4X4; i < EXT_TX_SIZES; ++i) {
    for (j = 0; j < TX_TYPES; ++j)
      vp10_cost_tokens(cpi->intra_tx_type_costs[i][j],
                       fc->intra_ext_tx_prob[i][j],
                       vp10_ext_tx_tree);
  }
  for (i = TX_4X4; i < EXT_TX_SIZES; ++i) {
    vp10_cost_tokens(cpi->inter_tx_type_costs[i],
                     fc->inter_ext_tx_prob[i],
                     vp10_ext_tx_tree);
  }
}

static void fill_token_costs(vp10_coeff_cost *c,
                             vp10_coeff_probs_model (*p)[PLANE_TYPES]) {
  int i, j, k, l;
  TX_SIZE t;
  for (t = TX_4X4; t <= TX_32X32; ++t)
    for (i = 0; i < PLANE_TYPES; ++i)
      for (j = 0; j < REF_TYPES; ++j)
        for (k = 0; k < COEF_BANDS; ++k)
          for (l = 0; l < BAND_COEFF_CONTEXTS(k); ++l) {
            vpx_prob probs[ENTROPY_NODES];
            vp10_model_to_full_probs(p[t][i][j][k][l], probs);
            vp10_cost_tokens((int *)c[t][i][j][k][0][l], probs,
                            vp10_coef_tree);
            vp10_cost_tokens_skip((int *)c[t][i][j][k][1][l], probs,
                                 vp10_coef_tree);
            assert(c[t][i][j][k][0][l][EOB_TOKEN] ==
                   c[t][i][j][k][1][l][EOB_TOKEN]);
          }
}

// Values are now correlated to quantizer.
static int sad_per_bit16lut_8[QINDEX_RANGE];
static int sad_per_bit4lut_8[QINDEX_RANGE];

#if CONFIG_VP9_HIGHBITDEPTH
static int sad_per_bit16lut_10[QINDEX_RANGE];
static int sad_per_bit4lut_10[QINDEX_RANGE];
static int sad_per_bit16lut_12[QINDEX_RANGE];
static int sad_per_bit4lut_12[QINDEX_RANGE];
#endif

static void init_me_luts_bd(int *bit16lut, int *bit4lut, int range,
                            vpx_bit_depth_t bit_depth) {
  int i;
  // Initialize the sad lut tables using a formulaic calculation for now.
  // This is to make it easier to resolve the impact of experimental changes
  // to the quantizer tables.
  for (i = 0; i < range; i++) {
    const double q = vp10_convert_qindex_to_q(i, bit_depth);
    bit16lut[i] = (int)(0.0418 * q + 2.4107);
    bit4lut[i] = (int)(0.063 * q + 2.742);
  }
}

void vp10_init_me_luts(void) {
  init_me_luts_bd(sad_per_bit16lut_8, sad_per_bit4lut_8, QINDEX_RANGE,
                  VPX_BITS_8);
#if CONFIG_VP9_HIGHBITDEPTH
  init_me_luts_bd(sad_per_bit16lut_10, sad_per_bit4lut_10, QINDEX_RANGE,
                  VPX_BITS_10);
  init_me_luts_bd(sad_per_bit16lut_12, sad_per_bit4lut_12, QINDEX_RANGE,
                  VPX_BITS_12);
#endif
}

static const int rd_boost_factor[16] = {
  64, 32, 32, 32, 24, 16, 12, 12,
  8, 8, 4, 4, 2, 2, 1, 0
};
static const int rd_frame_type_factor[FRAME_UPDATE_TYPES] = {
  128, 144, 128, 128, 144
};

int vp10_compute_rd_mult(const VP10_COMP *cpi, int qindex) {
  const int64_t q = vp10_dc_quant(qindex, 0, cpi->common.bit_depth);
#if CONFIG_VP9_HIGHBITDEPTH
  int64_t rdmult = 0;
  switch (cpi->common.bit_depth) {
    case VPX_BITS_8:
      rdmult = 88 * q * q / 24;
      break;
    case VPX_BITS_10:
      rdmult = ROUND_POWER_OF_TWO(88 * q * q / 24, 4);
      break;
    case VPX_BITS_12:
      rdmult = ROUND_POWER_OF_TWO(88 * q * q / 24, 8);
      break;
    default:
      assert(0 && "bit_depth should be VPX_BITS_8, VPX_BITS_10 or VPX_BITS_12");
      return -1;
  }
#else
  int64_t rdmult = 88 * q * q / 24;
#endif  // CONFIG_VP9_HIGHBITDEPTH
  if (cpi->oxcf.pass == 2 && (cpi->common.frame_type != KEY_FRAME)) {
    const GF_GROUP *const gf_group = &cpi->twopass.gf_group;
    const FRAME_UPDATE_TYPE frame_type = gf_group->update_type[gf_group->index];
    const int boost_index = VPXMIN(15, (cpi->rc.gfu_boost / 100));

    rdmult = (rdmult * rd_frame_type_factor[frame_type]) >> 7;
    rdmult += ((rdmult * rd_boost_factor[boost_index]) >> 7);
  }
  if (rdmult < 1)
    rdmult = 1;
  return (int)rdmult;
}

static int compute_rd_thresh_factor(int qindex, vpx_bit_depth_t bit_depth) {
  double q;
#if CONFIG_VP9_HIGHBITDEPTH
  switch (bit_depth) {
    case VPX_BITS_8:
      q = vp10_dc_quant(qindex, 0, VPX_BITS_8) / 4.0;
      break;
    case VPX_BITS_10:
      q = vp10_dc_quant(qindex, 0, VPX_BITS_10) / 16.0;
      break;
    case VPX_BITS_12:
      q = vp10_dc_quant(qindex, 0, VPX_BITS_12) / 64.0;
      break;
    default:
      assert(0 && "bit_depth should be VPX_BITS_8, VPX_BITS_10 or VPX_BITS_12");
      return -1;
  }
#else
  (void) bit_depth;
  q = vp10_dc_quant(qindex, 0, VPX_BITS_8) / 4.0;
#endif  // CONFIG_VP9_HIGHBITDEPTH
  // TODO(debargha): Adjust the function below.
  return VPXMAX((int)(pow(q, RD_THRESH_POW) * 5.12), 8);
}

void vp10_initialize_me_consts(VP10_COMP *cpi, MACROBLOCK *x, int qindex) {
#if CONFIG_VP9_HIGHBITDEPTH
  switch (cpi->common.bit_depth) {
    case VPX_BITS_8:
      x->sadperbit16 = sad_per_bit16lut_8[qindex];
      x->sadperbit4 = sad_per_bit4lut_8[qindex];
      break;
    case VPX_BITS_10:
      x->sadperbit16 = sad_per_bit16lut_10[qindex];
      x->sadperbit4 = sad_per_bit4lut_10[qindex];
      break;
    case VPX_BITS_12:
      x->sadperbit16 = sad_per_bit16lut_12[qindex];
      x->sadperbit4 = sad_per_bit4lut_12[qindex];
      break;
    default:
      assert(0 && "bit_depth should be VPX_BITS_8, VPX_BITS_10 or VPX_BITS_12");
  }
#else
  (void)cpi;
  x->sadperbit16 = sad_per_bit16lut_8[qindex];
  x->sadperbit4 = sad_per_bit4lut_8[qindex];
#endif  // CONFIG_VP9_HIGHBITDEPTH
}

static void set_block_thresholds(const VP10_COMMON *cm, RD_OPT *rd) {
  int i, bsize, segment_id;

  for (segment_id = 0; segment_id < MAX_SEGMENTS; ++segment_id) {
    const int qindex =
        clamp(vp10_get_qindex(&cm->seg, segment_id, cm->base_qindex) +
              cm->y_dc_delta_q, 0, MAXQ);
    const int q = compute_rd_thresh_factor(qindex, cm->bit_depth);

    for (bsize = 0; bsize < BLOCK_SIZES; ++bsize) {
      // Threshold here seems unnecessarily harsh but fine given actual
      // range of values used for cpi->sf.thresh_mult[].
      const int t = q * rd_thresh_block_size_factor[bsize];
      const int thresh_max = INT_MAX / t;

      if (bsize >= BLOCK_8X8) {
        for (i = 0; i < MAX_MODES; ++i)
          rd->threshes[segment_id][bsize][i] =
              rd->thresh_mult[i] < thresh_max
                  ? rd->thresh_mult[i] * t / 4
                  : INT_MAX;
      } else {
        for (i = 0; i < MAX_REFS; ++i)
          rd->threshes[segment_id][bsize][i] =
              rd->thresh_mult_sub8x8[i] < thresh_max
                  ? rd->thresh_mult_sub8x8[i] * t / 4
                  : INT_MAX;
      }
    }
  }
}

void vp10_initialize_rd_consts(VP10_COMP *cpi) {
  VP10_COMMON *const cm = &cpi->common;
  MACROBLOCK *const x = &cpi->td.mb;
  RD_OPT *const rd = &cpi->rd;
  int i;

  vpx_clear_system_state();

  rd->RDDIV = RDDIV_BITS;  // In bits (to multiply D by 128).
  rd->RDMULT = vp10_compute_rd_mult(cpi, cm->base_qindex + cm->y_dc_delta_q);

  x->errorperbit = rd->RDMULT / RD_MULT_EPB_RATIO;
  x->errorperbit += (x->errorperbit == 0);

  x->select_tx_size = (cpi->sf.tx_size_search_method == USE_LARGESTALL &&
                       cm->frame_type != KEY_FRAME) ? 0 : 1;

  set_block_thresholds(cm, rd);

  fill_token_costs(x->token_costs, cm->fc->coef_probs);

  if (cpi->sf.partition_search_type != VAR_BASED_PARTITION ||
      cm->frame_type == KEY_FRAME) {
    for (i = 0; i < PARTITION_CONTEXTS; ++i)
      vp10_cost_tokens(cpi->partition_cost[i], cm->fc->partition_prob[i],
                      vp10_partition_tree);
  }

  fill_mode_costs(cpi);

  if (!frame_is_intra_only(cm)) {
    vp10_build_nmv_cost_table(x->nmvjointcost,
                             cm->allow_high_precision_mv ? x->nmvcost_hp
                                                         : x->nmvcost,
                             &cm->fc->nmvc, cm->allow_high_precision_mv);

    for (i = 0; i < INTER_MODE_CONTEXTS; ++i)
      vp10_cost_tokens((int *)cpi->inter_mode_cost[i],
                      cm->fc->inter_mode_probs[i], vp10_inter_mode_tree);
  }
}

static void model_rd_norm(int xsq_q10, int *r_q10, int *d_q10) {
  // NOTE: The tables below must be of the same size.

  // The functions described below are sampled at the four most significant
  // bits of x^2 + 8 / 256.

  // Normalized rate:
  // This table models the rate for a Laplacian source with given variance
  // when quantized with a uniform quantizer with given stepsize. The
  // closed form expression is:
  // Rn(x) = H(sqrt(r)) + sqrt(r)*[1 + H(r)/(1 - r)],
  // where r = exp(-sqrt(2) * x) and x = qpstep / sqrt(variance),
  // and H(x) is the binary entropy function.
  static const int rate_tab_q10[] = {
    65536,  6086,  5574,  5275,  5063,  4899,  4764,  4651,
     4553,  4389,  4255,  4142,  4044,  3958,  3881,  3811,
     3748,  3635,  3538,  3453,  3376,  3307,  3244,  3186,
     3133,  3037,  2952,  2877,  2809,  2747,  2690,  2638,
     2589,  2501,  2423,  2353,  2290,  2232,  2179,  2130,
     2084,  2001,  1928,  1862,  1802,  1748,  1698,  1651,
     1608,  1530,  1460,  1398,  1342,  1290,  1243,  1199,
     1159,  1086,  1021,   963,   911,   864,   821,   781,
      745,   680,   623,   574,   530,   490,   455,   424,
      395,   345,   304,   269,   239,   213,   190,   171,
      154,   126,   104,    87,    73,    61,    52,    44,
       38,    28,    21,    16,    12,    10,     8,     6,
        5,     3,     2,     1,     1,     1,     0,     0,
  };
  // Normalized distortion:
  // This table models the normalized distortion for a Laplacian source
  // with given variance when quantized with a uniform quantizer
  // with given stepsize. The closed form expression is:
  // Dn(x) = 1 - 1/sqrt(2) * x / sinh(x/sqrt(2))
  // where x = qpstep / sqrt(variance).
  // Note the actual distortion is Dn * variance.
  static const int dist_tab_q10[] = {
       0,     0,     1,     1,     1,     2,     2,     2,
       3,     3,     4,     5,     5,     6,     7,     7,
       8,     9,    11,    12,    13,    15,    16,    17,
      18,    21,    24,    26,    29,    31,    34,    36,
      39,    44,    49,    54,    59,    64,    69,    73,
      78,    88,    97,   106,   115,   124,   133,   142,
     151,   167,   184,   200,   215,   231,   245,   260,
     274,   301,   327,   351,   375,   397,   418,   439,
     458,   495,   528,   559,   587,   613,   637,   659,
     680,   717,   749,   777,   801,   823,   842,   859,
     874,   899,   919,   936,   949,   960,   969,   977,
     983,   994,  1001,  1006,  1010,  1013,  1015,  1017,
    1018,  1020,  1022,  1022,  1023,  1023,  1023,  1024,
  };
  static const int xsq_iq_q10[] = {
         0,      4,      8,     12,     16,     20,     24,     28,
        32,     40,     48,     56,     64,     72,     80,     88,
        96,    112,    128,    144,    160,    176,    192,    208,
       224,    256,    288,    320,    352,    384,    416,    448,
       480,    544,    608,    672,    736,    800,    864,    928,
       992,   1120,   1248,   1376,   1504,   1632,   1760,   1888,
      2016,   2272,   2528,   2784,   3040,   3296,   3552,   3808,
      4064,   4576,   5088,   5600,   6112,   6624,   7136,   7648,
      8160,   9184,  10208,  11232,  12256,  13280,  14304,  15328,
     16352,  18400,  20448,  22496,  24544,  26592,  28640,  30688,
     32736,  36832,  40928,  45024,  49120,  53216,  57312,  61408,
     65504,  73696,  81888,  90080,  98272, 106464, 114656, 122848,
    131040, 147424, 163808, 180192, 196576, 212960, 229344, 245728,
  };
  const int tmp = (xsq_q10 >> 2) + 8;
  const int k = get_msb(tmp) - 3;
  const int xq = (k << 3) + ((tmp >> k) & 0x7);
  const int one_q10 = 1 << 10;
  const int a_q10 = ((xsq_q10 - xsq_iq_q10[xq]) << 10) >> (2 + k);
  const int b_q10 = one_q10 - a_q10;
  *r_q10 = (rate_tab_q10[xq] * b_q10 + rate_tab_q10[xq + 1] * a_q10) >> 10;
  *d_q10 = (dist_tab_q10[xq] * b_q10 + dist_tab_q10[xq + 1] * a_q10) >> 10;
}

void vp10_model_rd_from_var_lapndz(unsigned int var, unsigned int n_log2,
                                  unsigned int qstep, int *rate,
                                  int64_t *dist) {
  // This function models the rate and distortion for a Laplacian
  // source with given variance when quantized with a uniform quantizer
  // with given stepsize. The closed form expressions are in:
  // Hang and Chen, "Source Model for transform video coder and its
  // application - Part I: Fundamental Theory", IEEE Trans. Circ.
  // Sys. for Video Tech., April 1997.
  if (var == 0) {
    *rate = 0;
    *dist = 0;
  } else {
    int d_q10, r_q10;
    static const uint32_t MAX_XSQ_Q10 = 245727;
    const uint64_t xsq_q10_64 =
        (((uint64_t)qstep * qstep << (n_log2 + 10)) + (var >> 1)) / var;
    const int xsq_q10 = (int)VPXMIN(xsq_q10_64, MAX_XSQ_Q10);
    model_rd_norm(xsq_q10, &r_q10, &d_q10);
    *rate = ((r_q10 << n_log2) + 2) >> 2;
    *dist = (var * (int64_t)d_q10 + 512) >> 10;
  }
}

void vp10_get_entropy_contexts(BLOCK_SIZE bsize, TX_SIZE tx_size,
                              const struct macroblockd_plane *pd,
                              ENTROPY_CONTEXT t_above[16],
                              ENTROPY_CONTEXT t_left[16]) {
  const BLOCK_SIZE plane_bsize = get_plane_block_size(bsize, pd);
  const int num_4x4_w = num_4x4_blocks_wide_lookup[plane_bsize];
  const int num_4x4_h = num_4x4_blocks_high_lookup[plane_bsize];
  const ENTROPY_CONTEXT *const above = pd->above_context;
  const ENTROPY_CONTEXT *const left = pd->left_context;

  int i;
  switch (tx_size) {
    case TX_4X4:
      memcpy(t_above, above, sizeof(ENTROPY_CONTEXT) * num_4x4_w);
      memcpy(t_left, left, sizeof(ENTROPY_CONTEXT) * num_4x4_h);
      break;
    case TX_8X8:
      for (i = 0; i < num_4x4_w; i += 2)
        t_above[i] = !!*(const uint16_t *)&above[i];
      for (i = 0; i < num_4x4_h; i += 2)
        t_left[i] = !!*(const uint16_t *)&left[i];
      break;
    case TX_16X16:
      for (i = 0; i < num_4x4_w; i += 4)
        t_above[i] = !!*(const uint32_t *)&above[i];
      for (i = 0; i < num_4x4_h; i += 4)
        t_left[i] = !!*(const uint32_t *)&left[i];
      break;
    case TX_32X32:
      for (i = 0; i < num_4x4_w; i += 8)
        t_above[i] = !!*(const uint64_t *)&above[i];
      for (i = 0; i < num_4x4_h; i += 8)
        t_left[i] = !!*(const uint64_t *)&left[i];
      break;
    default:
      assert(0 && "Invalid transform size.");
      break;
  }
}

void vp10_mv_pred(VP10_COMP *cpi, MACROBLOCK *x,
                 uint8_t *ref_y_buffer, int ref_y_stride,
                 int ref_frame, BLOCK_SIZE block_size) {
  int i;
  int zero_seen = 0;
  int best_index = 0;
  int best_sad = INT_MAX;
  int this_sad = INT_MAX;
  int max_mv = 0;
  int near_same_nearest;
  uint8_t *src_y_ptr = x->plane[0].src.buf;
  uint8_t *ref_y_ptr;
  const int num_mv_refs = MAX_MV_REF_CANDIDATES +
                    (cpi->sf.adaptive_motion_search &&
                     block_size < x->max_partition_size);

  MV pred_mv[3];
  pred_mv[0] = x->mbmi_ext->ref_mvs[ref_frame][0].as_mv;
  pred_mv[1] = x->mbmi_ext->ref_mvs[ref_frame][1].as_mv;
  pred_mv[2] = x->pred_mv[ref_frame];
  assert(num_mv_refs <= (int)(sizeof(pred_mv) / sizeof(pred_mv[0])));

  near_same_nearest =
      x->mbmi_ext->ref_mvs[ref_frame][0].as_int ==
          x->mbmi_ext->ref_mvs[ref_frame][1].as_int;
  // Get the sad for each candidate reference mv.
  for (i = 0; i < num_mv_refs; ++i) {
    const MV *this_mv = &pred_mv[i];
    int fp_row, fp_col;

    if (i == 1 && near_same_nearest)
      continue;
    fp_row = (this_mv->row + 3 + (this_mv->row >= 0)) >> 3;
    fp_col = (this_mv->col + 3 + (this_mv->col >= 0)) >> 3;
    max_mv = VPXMAX(max_mv, VPXMAX(abs(this_mv->row), abs(this_mv->col)) >> 3);

    if (fp_row ==0 && fp_col == 0 && zero_seen)
      continue;
    zero_seen |= (fp_row ==0 && fp_col == 0);

    ref_y_ptr =&ref_y_buffer[ref_y_stride * fp_row + fp_col];
    // Find sad for current vector.
    this_sad = cpi->fn_ptr[block_size].sdf(src_y_ptr, x->plane[0].src.stride,
                                           ref_y_ptr, ref_y_stride);
    // Note if it is the best so far.
    if (this_sad < best_sad) {
      best_sad = this_sad;
      best_index = i;
    }
  }

  // Note the index of the mv that worked best in the reference list.
  x->mv_best_ref_index[ref_frame] = best_index;
  x->max_mv_context[ref_frame] = max_mv;
  x->pred_mv_sad[ref_frame] = best_sad;
}

void vp10_setup_pred_block(const MACROBLOCKD *xd,
                          struct buf_2d dst[MAX_MB_PLANE],
                          const YV12_BUFFER_CONFIG *src,
                          int mi_row, int mi_col,
                          const struct scale_factors *scale,
                          const struct scale_factors *scale_uv) {
  int i;

  dst[0].buf = src->y_buffer;
  dst[0].stride = src->y_stride;
  dst[1].buf = src->u_buffer;
  dst[2].buf = src->v_buffer;
  dst[1].stride = dst[2].stride = src->uv_stride;

  for (i = 0; i < MAX_MB_PLANE; ++i) {
    setup_pred_plane(dst + i, dst[i].buf, dst[i].stride, mi_row, mi_col,
                     i ? scale_uv : scale,
                     xd->plane[i].subsampling_x, xd->plane[i].subsampling_y);
  }
}

int vp10_raster_block_offset(BLOCK_SIZE plane_bsize,
                            int raster_block, int stride) {
  const int bw = b_width_log2_lookup[plane_bsize];
  const int y = 4 * (raster_block >> bw);
  const int x = 4 * (raster_block & ((1 << bw) - 1));
  return y * stride + x;
}

int16_t* vp10_raster_block_offset_int16(BLOCK_SIZE plane_bsize,
                                       int raster_block, int16_t *base) {
  const int stride = 4 * num_4x4_blocks_wide_lookup[plane_bsize];
  return base + vp10_raster_block_offset(plane_bsize, raster_block, stride);
}

YV12_BUFFER_CONFIG *vp10_get_scaled_ref_frame(const VP10_COMP *cpi,
                                             int ref_frame) {
  const VP10_COMMON *const cm = &cpi->common;
  const int scaled_idx = cpi->scaled_ref_idx[ref_frame - 1];
  const int ref_idx = get_ref_frame_buf_idx(cpi, ref_frame);
  return
      (scaled_idx != ref_idx && scaled_idx != INVALID_IDX) ?
          &cm->buffer_pool->frame_bufs[scaled_idx].buf : NULL;
}

int vp10_get_switchable_rate(const VP10_COMP *cpi,
                             const MACROBLOCKD *const xd) {
  const MB_MODE_INFO *const mbmi = &xd->mi[0]->mbmi;
  const int ctx = vp10_get_pred_context_switchable_interp(xd);
  return SWITCHABLE_INTERP_RATE_FACTOR *
             cpi->switchable_interp_costs[ctx][mbmi->interp_filter];
}

void vp10_set_rd_speed_thresholds(VP10_COMP *cpi) {
  int i;
  RD_OPT *const rd = &cpi->rd;
  SPEED_FEATURES *const sf = &cpi->sf;

  // Set baseline threshold values.
  for (i = 0; i < MAX_MODES; ++i)
    rd->thresh_mult[i] = cpi->oxcf.mode == BEST ? -500 : 0;

  if (sf->adaptive_rd_thresh) {
    rd->thresh_mult[THR_NEARESTMV] = 300;
    rd->thresh_mult[THR_NEARESTG] = 300;
    rd->thresh_mult[THR_NEARESTA] = 300;
  } else {
    rd->thresh_mult[THR_NEARESTMV] = 0;
    rd->thresh_mult[THR_NEARESTG] = 0;
    rd->thresh_mult[THR_NEARESTA] = 0;
  }

  rd->thresh_mult[THR_DC] += 1000;

  rd->thresh_mult[THR_NEWMV] += 1000;
  rd->thresh_mult[THR_NEWA] += 1000;
  rd->thresh_mult[THR_NEWG] += 1000;

  rd->thresh_mult[THR_NEARMV] += 1000;
  rd->thresh_mult[THR_NEARA] += 1000;
  rd->thresh_mult[THR_COMP_NEARESTLA] += 1000;
  rd->thresh_mult[THR_COMP_NEARESTGA] += 1000;

  rd->thresh_mult[THR_TM] += 1000;

  rd->thresh_mult[THR_COMP_NEARLA] += 1500;
  rd->thresh_mult[THR_COMP_NEWLA] += 2000;
  rd->thresh_mult[THR_NEARG] += 1000;
  rd->thresh_mult[THR_COMP_NEARGA] += 1500;
  rd->thresh_mult[THR_COMP_NEWGA] += 2000;

  rd->thresh_mult[THR_ZEROMV] += 2000;
  rd->thresh_mult[THR_ZEROG] += 2000;
  rd->thresh_mult[THR_ZEROA] += 2000;
  rd->thresh_mult[THR_COMP_ZEROLA] += 2500;
  rd->thresh_mult[THR_COMP_ZEROGA] += 2500;

  rd->thresh_mult[THR_H_PRED] += 2000;
  rd->thresh_mult[THR_V_PRED] += 2000;
  rd->thresh_mult[THR_D45_PRED ] += 2500;
  rd->thresh_mult[THR_D135_PRED] += 2500;
  rd->thresh_mult[THR_D117_PRED] += 2500;
  rd->thresh_mult[THR_D153_PRED] += 2500;
  rd->thresh_mult[THR_D207_PRED] += 2500;
  rd->thresh_mult[THR_D63_PRED] += 2500;
}

void vp10_set_rd_speed_thresholds_sub8x8(VP10_COMP *cpi) {
  static const int thresh_mult[2][MAX_REFS] =
      {{2500, 2500, 2500, 4500, 4500, 2500},
       {2000, 2000, 2000, 4000, 4000, 2000}};
  RD_OPT *const rd = &cpi->rd;
  const int idx = cpi->oxcf.mode == BEST;
  memcpy(rd->thresh_mult_sub8x8, thresh_mult[idx], sizeof(thresh_mult[idx]));
}

void vp10_update_rd_thresh_fact(int (*factor_buf)[MAX_MODES], int rd_thresh,
                               int bsize, int best_mode_index) {
  if (rd_thresh > 0) {
    const int top_mode = bsize < BLOCK_8X8 ? MAX_REFS : MAX_MODES;
    int mode;
    for (mode = 0; mode < top_mode; ++mode) {
      const BLOCK_SIZE min_size = VPXMAX(bsize - 1, BLOCK_4X4);
      const BLOCK_SIZE max_size = VPXMIN(bsize + 2, BLOCK_64X64);
      BLOCK_SIZE bs;
      for (bs = min_size; bs <= max_size; ++bs) {
        int *const fact = &factor_buf[bs][mode];
        if (mode == best_mode_index) {
          *fact -= (*fact >> 4);
        } else {
          *fact = VPXMIN(*fact + RD_THRESH_INC, rd_thresh * RD_THRESH_MAX_FACT);
        }
      }
    }
  }
}

int vp10_get_intra_cost_penalty(int qindex, int qdelta,
                               vpx_bit_depth_t bit_depth) {
  const int q = vp10_dc_quant(qindex, qdelta, bit_depth);
#if CONFIG_VP9_HIGHBITDEPTH
  switch (bit_depth) {
    case VPX_BITS_8:
      return 20 * q;
    case VPX_BITS_10:
      return 5 * q;
    case VPX_BITS_12:
      return ROUND_POWER_OF_TWO(5 * q, 2);
    default:
      assert(0 && "bit_depth should be VPX_BITS_8, VPX_BITS_10 or VPX_BITS_12");
      return -1;
  }
#else
  return 20 * q;
#endif  // CONFIG_VP9_HIGHBITDEPTH
}

