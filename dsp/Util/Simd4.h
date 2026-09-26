#pragma once

// Minimal 4 x float SIMD helper: SSE2 on x86-64 (always available there), NEON on arm64 (Apple Silicon),
// scalar fallback elsewhere. Only what the DSP needs.

#if defined(_M_X64) || defined(__x86_64__) || defined(__SSE2__)
 #include <emmintrin.h>
 #define AUGUR_SIMD_SSE 1
#elif defined(__aarch64__) || defined(_M_ARM64)
 #include <arm_neon.h>
 #define AUGUR_SIMD_NEON 1
#endif

#include <cstring>

namespace augur::simd
{

#if defined(AUGUR_SIMD_SSE)

struct F4
{
    __m128 v;
};
inline F4 load (const float* p) noexcept { return { _mm_loadu_ps (p) }; }
inline void store (float* p, F4 a) noexcept { _mm_storeu_ps (p, a.v); }
inline F4 splat (float x) noexcept { return { _mm_set1_ps (x) }; }
inline F4 operator+ (F4 a, F4 b) noexcept { return { _mm_add_ps (a.v, b.v) }; }
inline F4 operator- (F4 a, F4 b) noexcept { return { _mm_sub_ps (a.v, b.v) }; }
inline F4 operator* (F4 a, F4 b) noexcept { return { _mm_mul_ps (a.v, b.v) }; }
inline F4 operator/ (F4 a, F4 b) noexcept { return { _mm_div_ps (a.v, b.v) }; }
inline F4 abs (F4 a) noexcept { return { _mm_andnot_ps (_mm_set1_ps (-0.0f), a.v) }; }
// mask ? a : b, with mask = x > limit
inline F4 selectGreater (F4 x, F4 limit, F4 a, F4 b) noexcept
{
    const __m128 m = _mm_cmpgt_ps (x.v, limit.v);
    return { _mm_or_ps (_mm_and_ps (m, a.v), _mm_andnot_ps (m, b.v)) };
}
inline F4 min (F4 a, F4 b) noexcept { return { _mm_min_ps (a.v, b.v) }; }
inline F4 max (F4 a, F4 b) noexcept { return { _mm_max_ps (a.v, b.v) }; }
inline F4 set (float a, float b, float c, float d) noexcept { return { _mm_setr_ps (a, b, c, d) }; }
inline float get (F4 a, int lane) noexcept
{
    alignas (16) float t[4];
    _mm_store_ps (t, a.v);
    return t[lane];
}
// 2^x for x already clamped to [-126, 126]: floor via truncation + correction, exponent bits.
inline F4 exp2Parts (F4 x, F4& fraction) noexcept
{
    __m128i i = _mm_cvttps_epi32 (x.v);
    __m128 fi = _mm_cvtepi32_ps (i);
    const __m128 gt = _mm_cmpgt_ps (fi, x.v); // truncated up (negative non-integers)
    fi = _mm_sub_ps (fi, _mm_and_ps (gt, _mm_set1_ps (1.0f)));
    i = _mm_cvttps_epi32 (fi);
    fraction = { _mm_sub_ps (x.v, fi) };
    return { _mm_castsi128_ps (_mm_slli_epi32 (_mm_add_epi32 (i, _mm_set1_epi32 (127)), 23)) };
}

#elif defined(AUGUR_SIMD_NEON)

struct F4
{
    float32x4_t v;
};
inline F4 load (const float* p) noexcept { return { vld1q_f32 (p) }; }
inline void store (float* p, F4 a) noexcept { vst1q_f32 (p, a.v); }
inline F4 splat (float x) noexcept { return { vdupq_n_f32 (x) }; }
inline F4 operator+ (F4 a, F4 b) noexcept { return { vaddq_f32 (a.v, b.v) }; }
inline F4 operator- (F4 a, F4 b) noexcept { return { vsubq_f32 (a.v, b.v) }; }
inline F4 operator* (F4 a, F4 b) noexcept { return { vmulq_f32 (a.v, b.v) }; }
inline F4 operator/ (F4 a, F4 b) noexcept { return { vdivq_f32 (a.v, b.v) }; }
inline F4 abs (F4 a) noexcept { return { vabsq_f32 (a.v) }; }
inline F4 selectGreater (F4 x, F4 limit, F4 a, F4 b) noexcept { return { vbslq_f32 (vcgtq_f32 (x.v, limit.v), a.v, b.v) }; }
inline F4 min (F4 a, F4 b) noexcept { return { vminq_f32 (a.v, b.v) }; }
inline F4 max (F4 a, F4 b) noexcept { return { vmaxq_f32 (a.v, b.v) }; }
inline F4 set (float a, float b, float c, float d) noexcept
{
    const float t[4] = { a, b, c, d };
    return { vld1q_f32 (t) };
}
inline float get (F4 a, int lane) noexcept
{
    float t[4];
    vst1q_f32 (t, a.v);
    return t[lane];
}
inline F4 exp2Parts (F4 x, F4& fraction) noexcept
{
    const float32x4_t fi = vrndmq_f32 (x.v); // floor
    fraction = { vsubq_f32 (x.v, fi) };
    const int32x4_t i = vcvtq_s32_f32 (fi);
    return { vreinterpretq_f32_s32 (vshlq_n_s32 (vaddq_s32 (i, vdupq_n_s32 (127)), 23)) };
}

#else

struct F4
{
    float v[4];
};
inline F4 load (const float* p) noexcept { return { { p[0], p[1], p[2], p[3] } }; }
inline void store (float* p, F4 a) noexcept { for (int i = 0; i < 4; ++i) p[i] = a.v[i]; }
inline F4 splat (float x) noexcept { return { { x, x, x, x } }; }
#define AUGUR_F4_OP(op) \
    inline F4 operator op (F4 a, F4 b) noexcept { return { { a.v[0] op b.v[0], a.v[1] op b.v[1], a.v[2] op b.v[2], a.v[3] op b.v[3] } }; }
AUGUR_F4_OP (+)
AUGUR_F4_OP (-)
AUGUR_F4_OP (*)
AUGUR_F4_OP (/)
#undef AUGUR_F4_OP
inline F4 abs (F4 a) noexcept
{
    F4 r;
    for (int i = 0; i < 4; ++i) r.v[i] = a.v[i] < 0.0f ? -a.v[i] : a.v[i];
    return r;
}
inline F4 selectGreater (F4 x, F4 limit, F4 a, F4 b) noexcept
{
    F4 r;
    for (int i = 0; i < 4; ++i) r.v[i] = x.v[i] > limit.v[i] ? a.v[i] : b.v[i];
    return r;
}
inline F4 min (F4 a, F4 b) noexcept
{
    F4 r;
    for (int i = 0; i < 4; ++i) r.v[i] = a.v[i] < b.v[i] ? a.v[i] : b.v[i];
    return r;
}
inline F4 max (F4 a, F4 b) noexcept
{
    F4 r;
    for (int i = 0; i < 4; ++i) r.v[i] = a.v[i] > b.v[i] ? a.v[i] : b.v[i];
    return r;
}
inline F4 set (float a, float b, float c, float d) noexcept { return { { a, b, c, d } }; }
inline float get (F4 a, int lane) noexcept { return a.v[lane]; }
inline F4 exp2Parts (F4 x, F4& fraction) noexcept
{
    F4 r;
    for (int k = 0; k < 4; ++k)
    {
        int i = static_cast<int> (x.v[k]);
        i -= static_cast<float> (i) > x.v[k] ? 1 : 0;
        fraction.v[k] = x.v[k] - static_cast<float> (i);
        const int bits = (i + 127) << 23;
        std::memcpy (&r.v[k], &bits, sizeof (float));
    }
    return r;
}

#endif

// tanh(x) / x for four values at once (same Padé as fastmath::tanhRatio).
inline F4 tanhRatio (F4 x) noexcept
{
    const F4 ax = abs (x);
    const F4 x2 = x * x;
    const F4 num = splat (135135.0f) + x2 * (splat (17325.0f) + x2 * (splat (378.0f) + x2));
    const F4 den = splat (135135.0f) + x2 * (splat (62370.0f) + x2 * (splat (3150.0f) + splat (28.0f) * x2));
    const F4 one = splat (1.0f);
    return selectGreater (ax, splat (4.97f), one / (ax + splat (1.0e-30f)), num / den);
}

// The same approximations as fastmath::tanh / tan / exp2, four lanes at a time.
inline F4 tanh (F4 x) noexcept
{
    x = min (max (x, splat (-4.97f)), splat (4.97f));
    const F4 x2 = x * x;
    const F4 num = x * (splat (135135.0f) + x2 * (splat (17325.0f) + x2 * (splat (378.0f) + x2)));
    const F4 den = splat (135135.0f) + x2 * (splat (62370.0f) + x2 * (splat (3150.0f) + splat (28.0f) * x2));
    return min (max (num / den, splat (-1.0f)), splat (1.0f));
}

inline F4 tan (F4 x) noexcept
{
    x = min (max (x, splat (0.0f)), splat (1.45f));
    const F4 x2 = x * x;
    const F4 num = x * (splat (135135.0f) - x2 * (splat (17325.0f) - x2 * (splat (378.0f) - x2)));
    const F4 den = splat (135135.0f) - x2 * (splat (62370.0f) - x2 * (splat (3150.0f) - splat (28.0f) * x2));
    return num / den;
}

inline F4 exp2 (F4 x) noexcept
{
    x = min (max (x, splat (-126.0f)), splat (126.0f));
    F4 frac;
    const F4 scale = exp2Parts (x, frac);
    const F4 g = frac - splat (0.5f);
    const F4 p = splat (1.0f) + g * (splat (0.69314718f) + g * (splat (0.24022651f) + g * (splat (0.05550411f)
                 + g * (splat (0.00961813f) + g * (splat (0.00133336f) + g * splat (0.00015404f))))));
    return p * scale * splat (1.41421356f);
}

} // namespace augur::simd
