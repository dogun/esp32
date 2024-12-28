#ifndef MAIN_EQ__H_
#define MAIN_EQ__H_

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <assert.h>
#include <stdint.h>
#include <math.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "esp_log.h"
#include "config.h"

#define MAX_EQ_COUNT 32
#define PREAMPF powf(10.0f, -1 / 20.0f)

#define EQ_TAG "eq"

typedef struct t_biquad t_biquad;
struct t_biquad {
	float a0, a1, a2, a3, a4;
	float x1, x2, y1, y2;
	float cf, q, gain;
};

static inline float _apply_biquads_one(float s, t_biquad* b);
static inline void _biquads_x(int32_t* src, int32_t* dst, int len, t_biquad* biquad, int start, int eqline);
static inline void _apply_biquads_r(int32_t* src, int32_t* dst, int len);
static inline void _apply_biquads_l(int32_t* src, int32_t* dst, int len);

static void _mk_biquad(float dbgain, float cf, float q, t_biquad *b);

int eq_len_r;
int eq_len_l;
static t_biquad r_biquads[MAX_EQ_COUNT];
static t_biquad l_biquads[MAX_EQ_COUNT];

static inline void _biquads_x(int32_t* src, int32_t* dst, int len, t_biquad* biquad, int start, int eqline) {
	int i, di;

	for (i = start; i < len; i += 2) {
		int32_t rl = src[i];

		rl = rl >> 8;

		float l_s = (float)rl * PREAMPF;
		float l_f = l_s;

		for (di = 0; di < eqline; ++di) {
			l_f = _apply_biquads_one(l_s, &(biquad[di]));
			l_s = l_f;
		}
		rl = (int32_t)l_f;
		rl = rl << 8;

		dst[i] = rl;
	}
}

static inline void _apply_biquads_l(int32_t* src, int32_t* dst, int len) {
	_biquads_x(src, dst, len, l_biquads, L_CODE, eq_len_l);
}

static inline void _apply_biquads_r(int32_t* src, int32_t* dst, int len) {
	_biquads_x(src, dst, len, r_biquads, R_CODE, eq_len_r);
}

static inline float _apply_biquads_one(float s, t_biquad* b) {
	float f =
		s * b->a0 \
		+ b->a1 * b->x1 \
		+ b->a2 * b->x2 \
		- b->a3 * b->y1 \
		- b->a4 * b->y2;
	b->x2 = b->x1;
	b->x1 = s;
	b->y2 = b->y1;
	b->y1 = f;
	return f;
}

/* biquad functions */

/* Create a Peaking EQ Filter.
 * See 'Audio EQ Cookbook' for more information
 */
static void _mk_biquad(float dbgain, float cf, float q, t_biquad* b) {
  if(b == NULL) {
	  ESP_LOGE(EQ_TAG, "biquad NULL");
	  exit(1);
  }

  ESP_LOGW(EQ_TAG, "make biquad: %f %f %f", cf, dbgain, q);
  float A = powf(10.0, dbgain / 40.0);
  float omega = 2.0 * 3.14159265358979323846 * cf / RATE;
  float sn = sinf(omega);
  float cs = cosf(omega);
  //float alpha = sn * sinh(M_LN2 / 2.0f * bw * omega / sn);  //use band width
  float alpha = sn / (2 * q);

  float alpha_m_A = alpha * A;
  float alpha_d_A = alpha / A;

  float b0 = 1.0 + alpha_m_A;
  float b1 = -2.0 * cs;
  float b2 = 1.0 - alpha_m_A;
  float a0 = 1.0 + alpha_d_A;
  float a1 = b1;
  float a2 = 1.0 - alpha_d_A;

  b->a0 = b0 / a0;
  b->a1 = b1 / a0;
  b->a2 = b2 / a0;
  b->a3 = a1 / a0;
  b->a4 = a2 / a0;

  b->x1 = 0.0;
  b->x2 = 0.0;
  b->y1 = 0.0;
  b->y2 = 0.0;

  b->cf = cf;
  b->q = q;
  b->gain = dbgain;
}

#endif
