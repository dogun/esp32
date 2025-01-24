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
static inline void _biquads_x(int32_t* src, int32_t* dst, int len);

static void _mk_biquad(float dbgain, float cf, float q, t_biquad *b);

static size_t eq_len_r;
static size_t eq_len_l;
static t_biquad r_biquads[MAX_EQ_COUNT];
static t_biquad l_biquads[MAX_EQ_COUNT];

static inline void _biquads_x(int32_t* src, int32_t* dst, int len) {
	int i, di;

	for (i = 0; i < len; i = i + 2) {
		int32_t data_l = (src[i] << 1) >> 8;
		int32_t data_r = (src[i + 1] << 1) >> 8;

		float l_s = (float)data_l * PREAMPF;
		float l_f = l_s;

		for (di = 0; di < eq_len_l; ++di) {
			l_f = _apply_biquads_one(l_s, &(l_biquads[di]));
			l_s = l_f;
		}
		
		float r_s = (float)data_r * PREAMPF;
		float r_f = r_s;

		for (di = 0; di < eq_len_r; ++di) {
			r_f = _apply_biquads_one(r_s, &(r_biquads[di]));
			r_s = r_f;
		}
		
		data_l = (int32_t)l_f;
		data_r = (int32_t)r_f;

		dst[i] = (data_l << 7) & 0x7FFFFFFF;
		dst[i+1] = (data_r << 7) & 0x7FFFFFFF;
	}
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


static void load_eq() {
	//int i = 0;
	//_mk_biquad(-2.8, 510, 2.588, &l_biquads[i++]);
	//_mk_biquad(-6.2, 1728, 1.281, &l_biquads[i++]);
	//_mk_biquad(-7.2, 4540, 1.143, &l_biquads[i++]);
	//eq_len_l = i;
	
	//_mk_biquad(6, 1000, 1, &r_biquads[0]);
	//eq_len_r = 1;
}
#endif
