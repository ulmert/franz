/*
    BSD 3-Clause License

    Copyright (c) 2023, Jacob Ulmert
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright notice, this
      list of conditions and the following disclaimer.

    * Redistributions in binary form must reproduce the above copyright notice,
      this list of conditions and the following disclaimer in the documentation
      and/or other materials provided with the distribution.

    * Neither the name of the copyright holder nor the names of its
      contributors may be used to endorse or promote products derived from
      this software without specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
    AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
    IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
    DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
    FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
    DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
    SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
    CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
    OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
    OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "userosc.h"

float param_lvl1;
float param_lvl2;
float param_lvl3;
float param_lvl4;

float ct0,ct1;
float t0;
float ctm;
float f0, f1;
float d0;
float pending_d0;
float pending_f0;
float pending_ctm;
float an;
float pol;
float pending_pol;
float pending_at0;

float pending_f1;
float at0;

bool setParams;
bool noteOn;

#define FT (1.f / 48000.f)
#define GAIN 0.5

#define FOLD
#define FOLDTHR 1.f

#ifdef FOLD
#define LVL_GAIN_MAX 1.0
#else
#define LVL_GAIN_MAX 0.8
#endif

#define NRAND 256
float randTbl[NRAND];
uint16_t randIdx;

#define NRAND_RESET (256-64)
float *pRand;

void OSC_INIT(uint32_t platform, uint32_t api)
{
  (void)platform;
  (void)api;

  randIdx = NRAND;
  while(randIdx) {
    randIdx--;
    randTbl[randIdx] = ((osc_white() + 1.0) * 0.5);
  }
  pRand = &randTbl[randIdx];

  pending_d0 = 50.f;
  d0 = pending_d0;

  pending_ctm = 1.f;
  ctm = pending_ctm;

  pending_f1 = 200.f;
  f1 = pending_f1;

  pending_pol = 1.f;
  pol = pending_pol;

  pending_at0 = 1.f;
  at0 = pending_at0;

  pending_f0 = 200.f;
  f0 = pending_f0;
  
  an = 0.f;

  ct1 = 0.f;

  t0 = 100.f;

  param_lvl1 = 1.f;
  param_lvl2 = 1.f;
  param_lvl3 = 1.f;
  param_lvl4 = 1.f;

  setParams = false;
  noteOn = false;

}

void OSC_CYCLE(const user_osc_param_t * const params, int32_t *yn, const uint32_t frames)
{
  randTbl[randIdx++] = ((osc_white() + 1.0) * 0.5);
  if (randIdx == NRAND) { randIdx = 0;}

  if (pRand >= &randTbl[NRAND_RESET]) {
    pRand = &randTbl[0];
  }

  if (noteOn) {
    pending_f0 = osc_notehzf((params->pitch)>>8) * 0.5;
    if ((*pRand++) > 0) {
      int32_t p = 2 ^ (uint32_t)((*pRand++) * 4);
      if (p == 0) {p = 1;}
      if ((*pRand++) < 0) {
        pol = pol * -1;
      }
      pending_f1 = pending_f0 * p;
    }

    pending_at0 = (*pRand++);
    if (pending_at0 > 0.5) {
      pending_f0 *= 2.f;
    }

    setParams = true;         
    noteOn = false;
  }

  const float a0 = (1.f - (t0 / d0));

  float a1 =  1.f - (t0 / (d0 * 0.5));
  if (a1 < 0.f) {a1 = 0.f;} else if (a1 > 1.f) {a1 = 1.f;} else {a1 = (a1 * a1) * param_lvl1;}

  float a2 = (1.f - (t0 / (d0 * at0)));
  if (a2 < 0.f) {a2 = 0.f;} else if (a2 > 1.f) {a2 = 1.f;} else {a2 = (a2 * a2) * param_lvl2;}

  float a3 = 1.f - (t0 / ((d0 * at0) * an));
  if (a3 < 0.f) {a3 = 0.f;} else if (a3 > 1.f) {a3 = 1.f;} else {a3 = (a3 * a3) * param_lvl3;}


  q31_t * __restrict y = (q31_t *)yn;
  const q31_t * y_e = y + frames;

  for (; y != y_e; ) {

    float sample = osc_sinf(ct0) * param_lvl1;

    ct1 += FT * f1;
    if (ct1 > 1.f) {
      ct1 = ct1 - 1.f;
      f1 = pending_f1;
      pol = pending_pol;
      at0 = pending_at0;
    }

    ct0 += FT * f0;
    if (ct0 > 1.f) {
      ct0 = ct0 - 1.f;
      if (setParams) {
        d0 = pending_d0;
        d0 = d0 * (*pRand++) + 10.f;

        ctm = pending_ctm;
        t0 = 0;
        f0 = pending_f0;
        ctm = ctm * (*pRand++);
       
        an = (*pRand++);
        setParams = false;
      }
      if (t0 >= d0) {
        t0 = d0;
      }
    }
   
    if (t0 <= d0) {
      sample = sample + (sample * osc_sinf(ct0 * ctm * a0)* a1);

#ifdef FOLD
      if (sample > FOLDTHR) {
        sample = sample - (sample - FOLDTHR);
      } else if (sample < -FOLDTHR) {
        sample = sample + (sample + FOLDTHR);
      }
#endif
     
      sample = sample + (sample + pol * osc_sinf(ct1)) * a2;
     
      sample = sample * a0;

      sample += (osc_white() * an * a3) * param_lvl4;

      sample = (sample - pol * osc_sinf(ct1 * 4.2) * a3);

      *(y++) = f32_to_q31(sample * GAIN);

      t0 += FT * 50.f;

    } else {
      *(y++) = f32_to_q31(0);
    }
  }
}

void OSC_NOTEON(const user_osc_param_t * const params) 
{
  noteOn = true;
}

void OSC_NOTEOFF(const user_osc_param_t * const params)
{
  
}

void OSC_PARAM(uint16_t index, uint16_t value)
{ 
  float valf = param_val_to_f32(value);
  switch (index) {
    case k_user_osc_param_id1: 
      param_lvl1 = LVL_GAIN_MAX * (1.f - ((float)value / 10.f));
      break;
  
    case k_user_osc_param_id2:
      param_lvl2 = LVL_GAIN_MAX * (1.f - ((float)value / 10.f));
      break;
    
    case k_user_osc_param_id3:
      param_lvl3 = LVL_GAIN_MAX * (1.f - ((float)value / 10.f));
      break;

    case k_user_osc_param_id4:
      param_lvl4 = LVL_GAIN_MAX * (1.f - ((float)value / 10.f));
      break;

    case k_user_osc_param_shape:
      pending_ctm = valf * 24.f; 
      break;

    case k_user_osc_param_shiftshape:
      pending_d0 = 3.f + valf * 50.f;
      break;

    default:
      break;
  }  
}