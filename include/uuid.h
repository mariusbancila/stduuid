#ifndef STDUUID_H
#define STDUUID_H

#include <cstring>
#include <cstdint>
#include <string>
#include <sstream>
#include <iomanip>
#include <array>
#include <string_view>
#include <iterator>
#include <random>
#include <memory>
#include <functional>
#include <type_traits>
#include <optional>
#include <chrono>
#include <numeric>
#include <atomic>

// -----------------------------------------------------------------------------
// SIMD support
//
// The hot paths of the library - formatting a uuid as text, parsing text into a
// uuid, nil/equality checks and the SHA-1 compression used by the name based
// generator - are all byte/word crunching that maps very well onto SIMD.
//
// SIMD is used opportunistically:
//   * x86/x86-64 uses SSE2, SSSE3 and (when the processor has it) the SHA
//     extensions. On GCC/Clang the SSSE3/SHA code is compiled with target
//     attributes and selected at run time, so the library still works on old
//     CPUs and no special compiler flags are required.
//   * ARM/AArch64 uses NEON.
//   * everything else, and any build with STDUUID_DISABLE_SIMD defined, uses
//     the original portable scalar code.
//
// std::simd (C++26) is not shipped by any mainstream standard library yet, so
// the intrinsics below are the portable-in-practice way of writing this.  The
// scalar code stays as the reference implementation and as the fallback.
// -----------------------------------------------------------------------------
#if defined(STDUUID_DISABLE_SIMD)
#  define STDUUID_SIMD_ENABLED 0
#else
#  define STDUUID_SIMD_ENABLED 1
#endif

#if STDUUID_SIMD_ENABLED && (defined(_M_X64) || defined(__x86_64__) || defined(__i386__) || defined(_M_IX86))
#  define STDUUID_ARCH_X86 1
#endif

// MSVC's ARM64 intrinsics live in <arm64_neon.h> and use a slightly different
// setup, so the NEON path is limited to the GCC/Clang spelling.  ARM64 builds
// with MSVC simply use the portable scalar code.
#if STDUUID_SIMD_ENABLED && (defined(__ARM_NEON) || defined(__ARM_NEON__))
#  define STDUUID_ARCH_NEON 1
#endif

#if defined(STDUUID_ARCH_X86)
#  include <immintrin.h>
#endif

#if defined(STDUUID_ARCH_NEON)
#  include <arm_neon.h>
#endif

// std::is_constant_evaluated() is the C++20 spelling; __builtin_is_constant_evaluated()
// works in C++17 already with GCC and Clang.  The check is needed because the
// parsing/formatting members below are constexpr, but the SIMD helpers are not.
#if defined(__GNUC__) || defined(__clang__)
#  define STDUUID_LIKELY(x) __builtin_expect(!!(x), 1)
#else
#  define STDUUID_LIKELY(x) (x)
#endif

#if defined(__cpp_lib_is_constant_evaluated)
#  define STDUUID_IS_CONSTANT_EVALUATED() std::is_constant_evaluated()
#elif defined(__has_builtin)
#  if __has_builtin(__builtin_is_constant_evaluated)
#    define STDUUID_IS_CONSTANT_EVALUATED() __builtin_is_constant_evaluated()
#  else
#    define STDUUID_IS_CONSTANT_EVALUATED() false
#  endif
#else
#  define STDUUID_IS_CONSTANT_EVALUATED() false
#endif

#ifdef __cplusplus

#  if __has_include(<version>)
#    include <version>
#  endif

// The feature-test macro is deliberately tested first, and the __cplusplus value only as a fallback.
// A standard library can provide a complete <span> while the compiler still reports a pre-C++20 value for __cplusplus: 
// GCC 10 reports 201709L for -std=c++20 because the support was experimental, yet it ships <span> and defines __cpp_lib_span.
// Checking __cplusplus alone made those compilers fall back to gsl::span, so keep the order.
#  if defined(__cpp_lib_span) || ((__cplusplus >= 202002L || defined(_MSVC_LANG) && _MSVC_LANG >= 202002L) && __has_include(<span>))
#    include <span>
#    define STDUUID_USE_STD_SPAN
#  else
#    include <gsl/span>
#  endif

// Formatting support is optional and, like the span detection above, is keyed
// off the feature-test macro rather than the value of __cplusplus: <format>
// arrived well after C++20 itself in every standard library.
#  if defined(__cpp_lib_format) && __has_include(<format>)
#    include <format>
#    define STDUUID_USE_STD_FORMAT
#  endif

#endif

#ifdef _WIN32

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#ifdef UUID_SYSTEM_GENERATOR
#include <objbase.h>
#endif

#ifdef UUID_TIME_GENERATOR
#include <iphlpapi.h> 
#pragma comment(lib, "IPHLPAPI.lib")
#endif

#elif defined(__linux__) || defined(__unix__)

#ifdef UUID_SYSTEM_GENERATOR
#include <uuid/uuid.h>
#endif

#elif defined(__APPLE__)

#ifdef UUID_SYSTEM_GENERATOR
#include <CoreFoundation/CFUUID.h>
#endif

#endif

namespace uuids
{
#ifdef STDUUID_USE_STD_SPAN
   template <class ElementType, std::size_t Extent>
   using span = std::span<ElementType, Extent>;
#else
   template <class ElementType, std::ptrdiff_t Extent>
   using span = gsl::span<ElementType, Extent>;
#endif

   namespace detail
   {
      // -----------------------------------------------------------------------
      // SIMD primitives.  Everything in here works on raw bytes and is
      // self-contained, so it can be used by both the uuid class and sha1.
      // -----------------------------------------------------------------------
      namespace simd
      {
         // Result of a fast-path parse.  A fast path may decline a string
         // (parse_not_applicable), in which case the caller runs the fully
         // general scalar parser.
         enum parse_result
         {
            parse_not_applicable = 0,
            parse_invalid = 1,
            parse_ok = 2
         };

#if defined(STDUUID_ARCH_X86) && (defined(__GNUC__) || defined(__clang__))
#  define STDUUID_X86_RUNTIME_DISPATCH 1
#  define STDUUID_TARGET_SSSE3 __attribute__((target("ssse3")))
#  define STDUUID_TARGET_SHA1  __attribute__((target("sha,ssse3,sse4.1")))
#else
#  define STDUUID_TARGET_SSSE3
#  define STDUUID_TARGET_SHA1
#endif

#if defined(__SSSE3__) || (defined(_MSC_VER) && (defined(__AVX__) || defined(__AVX2__)))
#  define STDUUID_SSSE3_COMPILE_TIME 1
#endif

#if defined(STDUUID_SSSE3_COMPILE_TIME) || defined(STDUUID_X86_RUNTIME_DISPATCH)
#  define STDUUID_SSSE3_CODE 1
#endif

#if defined(STDUUID_X86_RUNTIME_DISPATCH) || (defined(__SHA__) && defined(__SSSE3__))
#  define STDUUID_SHA1_CODE 1
#endif

#if STDUUID_SIMD_ENABLED && (defined(__SSE2__) || defined(_M_X64) || (defined(_M_IX86_FP) && _M_IX86_FP >= 2))
#  define STDUUID_SSE2_CODE 1
#endif

         // --------------------------------------------------------------------
         // Portable reference implementations
         // --------------------------------------------------------------------
         [[nodiscard]] inline bool hex_digit(char c) noexcept
         {
            if (c >= '0' && c <= '9') return true;
            if (c >= 'a' && c <= 'f') return true;
            if (c >= 'A' && c <= 'F') return true;
            return false;
         }

         [[nodiscard]] inline uint8_t hex_value(char c) noexcept
         {
            if (c >= '0' && c <= '9') return static_cast<uint8_t>(c - '0');
            if (c >= 'a' && c <= 'f') return static_cast<uint8_t>(c - 'a' + 10);
            return static_cast<uint8_t>(c - 'A' + 10);
         }

         inline void encode36_scalar(uint8_t const* data, char* out) noexcept
         {
            static constexpr char lut[16] = {
               '0', '1', '2', '3', '4', '5', '6', '7',
               '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'
            };

            for (size_t i = 0; i < 16; ++i)
            {
               if (i == 4 || i == 6 || i == 8 || i == 10)
                  *out++ = '-';
               *out++ = lut[data[i] >> 4 & 0x0f];
               *out++ = lut[data[i] & 0x0f];
            }
         }

         [[nodiscard]] inline bool hex32_scalar(char const* src, uint8_t* out) noexcept
         {
            for (size_t i = 0; i < 16; ++i)
            {
               char hi = src[i * 2];
               char lo = src[i * 2 + 1];
               if (!hex_digit(hi) || !hex_digit(lo))
                  return false;
               out[i] = static_cast<uint8_t>((hex_value(hi) << 4) | hex_value(lo));
            }
            return true;
         }

         // --------------------------------------------------------------------
         // x86: SSSE3 hex conversion
         // --------------------------------------------------------------------
#if defined(STDUUID_SSSE3_CODE)
         STDUUID_TARGET_SSSE3
         inline __m128i hex_nibbles_ssse3(__m128i c, bool& ok) noexcept
         {
            // A character is valid if it is in ['0','9'] or, after folding the
            // case bit, in ['a','f'].  All comparisons are signed but every
            // value involved is ASCII and therefore below 0x80.
            __m128i is_digit = _mm_and_si128(
               _mm_cmpgt_epi8(c, _mm_set1_epi8('0' - 1)),
               _mm_cmpgt_epi8(_mm_set1_epi8('9' + 1), c));

            __m128i lower = _mm_or_si128(c, _mm_set1_epi8(0x20));
            __m128i is_alpha = _mm_and_si128(
               _mm_cmpgt_epi8(lower, _mm_set1_epi8('a' - 1)),
               _mm_cmpgt_epi8(_mm_set1_epi8('f' + 1), lower));

            ok = _mm_movemask_epi8(_mm_or_si128(is_digit, is_alpha)) == 0xFFFF;

            // nibble = (c & 0x0f) + 9 * (c >> 6 & 1).  Bit 6 is set for the
            // ASCII letters 'a'..'f' and 'A'..'F', clear for '0'..'9'.
            __m128i nib = _mm_and_si128(c, _mm_set1_epi8(0x0f));
            __m128i letter = _mm_and_si128(_mm_srli_epi16(c, 6), _mm_set1_epi8(1));
            nib = _mm_add_epi8(nib, _mm_add_epi8(_mm_slli_epi16(letter, 3), letter));
            return nib;
         }

         STDUUID_TARGET_SSSE3
         inline void encode36_ssse3(uint8_t const* data, char* out) noexcept
         {
            const __m128i lut = _mm_setr_epi8(
               '0', '1', '2', '3', '4', '5', '6', '7',
               '8', '9', 'a', 'b', 'c', 'd', 'e', 'f');
            const __m128i m0f = _mm_set1_epi8(0x0f);

            __m128i v = _mm_loadu_si128(reinterpret_cast<__m128i const*>(data));
            __m128i lo = _mm_and_si128(v, m0f);
            __m128i hi = _mm_and_si128(_mm_srli_epi16(v, 4), m0f);
            __m128i lo_ch = _mm_shuffle_epi8(lut, lo);
            __m128i hi_ch = _mm_shuffle_epi8(lut, hi);

            // hi/lo interleaved: [d0hi d0lo d1hi d1lo ... d7lo] and the same
            // for bytes 8..15.
            __m128i first = _mm_unpacklo_epi8(hi_ch, lo_ch);
            __m128i second = _mm_unpackhi_epi8(hi_ch, lo_ch);

            // Store the groups around the four dashes.  The stores are
            // deliberately unaligned: there is no reason for the caller's
            // buffer to be aligned.
            _mm_storel_epi64(reinterpret_cast<__m128i*>(out), first);
            uint32_t w = static_cast<uint32_t>(_mm_cvtsi128_si32(_mm_srli_si128(first, 8)));
            std::memcpy(out + 9, &w, 4);
            w = static_cast<uint32_t>(_mm_cvtsi128_si32(_mm_srli_si128(first, 12)));
            std::memcpy(out + 14, &w, 4);
            w = static_cast<uint32_t>(_mm_cvtsi128_si32(second));
            std::memcpy(out + 19, &w, 4);
            _mm_storel_epi64(reinterpret_cast<__m128i*>(out + 24), _mm_srli_si128(second, 4));
            w = static_cast<uint32_t>(_mm_cvtsi128_si32(_mm_srli_si128(second, 12)));
            std::memcpy(out + 32, &w, 4);

            out[8] = '-';
            out[13] = '-';
            out[18] = '-';
            out[23] = '-';
         }

         STDUUID_TARGET_SSSE3
         inline bool hex32_ssse3(char const* src, uint8_t* out) noexcept
         {
            bool ok0 = false;
            bool ok1 = false;
            __m128i n0 = hex_nibbles_ssse3(
               _mm_loadu_si128(reinterpret_cast<__m128i const*>(src)), ok0);
            __m128i n1 = hex_nibbles_ssse3(
               _mm_loadu_si128(reinterpret_cast<__m128i const*>(src + 16)), ok1);

            if (!ok0 || !ok1)
               return false;

            // Multiply adjacent (high, low) nibbles by (16, 1) and add them,
            // producing eight big-endian bytes per input vector.
            const __m128i weights = _mm_setr_epi8(
               16, 1, 16, 1, 16, 1, 16, 1,
               16, 1, 16, 1, 16, 1, 16, 1);
            __m128i p0 = _mm_maddubs_epi16(n0, weights);
            __m128i p1 = _mm_maddubs_epi16(n1, weights);
            _mm_storeu_si128(reinterpret_cast<__m128i*>(out), _mm_packus_epi16(p0, p1));
            return true;
         }
#endif

         // --------------------------------------------------------------------
         // ARM/AArch64: NEON hex conversion
         // --------------------------------------------------------------------
#if defined(STDUUID_ARCH_NEON)
         inline uint8x16_t hex_chars_neon(uint8x16_t nibbles) noexcept
         {
            uint8x16_t alpha = vcgtq_u8(nibbles, vdupq_n_u8(9));
            return vbslq_u8(
               alpha,
               vaddq_u8(nibbles, vdupq_n_u8('a' - 10)),
               vaddq_u8(nibbles, vdupq_n_u8('0')));
         }

         inline void store_pairs_neon(uint8x16_t nibbles, uint8_t* out) noexcept
         {
            // vuzp splits even and odd bytes; the low eight bytes of each half
            // are the high and low nibbles of the first eight result bytes.
            uint8x16x2_t d = vuzpq_u8(nibbles, nibbles);
            uint8x16_t packed = vorrq_u8(vshlq_n_u8(d.val[0], 4), d.val[1]);
            vst1_u8(out, vget_low_u8(packed));
         }

         inline bool hex16_valid_neon(uint8x16_t c) noexcept
         {
#if defined(__aarch64__) || defined(_M_ARM64)
            uint8x16_t lower = vorrq_u8(c, vdupq_n_u8(0x20));
            uint8x16_t is_digit = vandq_u8(
               vcgeq_u8(c, vdupq_n_u8('0')), vcleq_u8(c, vdupq_n_u8('9')));
            uint8x16_t is_alpha = vandq_u8(
               vcgeq_u8(lower, vdupq_n_u8('a')), vcleq_u8(lower, vdupq_n_u8('f')));
            return vminvq_u8(vorrq_u8(is_digit, is_alpha)) != 0;
#else
            uint8_t tmp[16];
            vst1q_u8(tmp, c);
            for (int i = 0; i < 16; ++i)
               if (!hex_digit(static_cast<char>(tmp[i])))
                  return false;
            return true;
#endif
         }

         inline uint8x16_t hex_nibbles_neon(uint8x16_t c) noexcept
         {
            uint8x16_t nib = vandq_u8(c, vdupq_n_u8(0x0f));
            uint8x16_t letter = vandq_u8(vshrq_n_u8(c, 6), vdupq_n_u8(1));
            return vaddq_u8(nib, vaddq_u8(vshlq_n_u8(letter, 3), letter));
         }

         inline void encode36_neon(uint8_t const* data, char* out) noexcept
         {
            uint8x16_t v = vld1q_u8(data);
            uint8x16_t lo = vandq_u8(v, vdupq_n_u8(0x0f));
            uint8x16_t hi = vandq_u8(vshrq_n_u8(v, 4), vdupq_n_u8(0x0f));

            uint8x16x2_t z = vzipq_u8(hex_chars_neon(hi), hex_chars_neon(lo));

            // z.val[0] is the 16 hex characters of data[0..7], z.val[1] the
            // characters of data[8..15].  Place them around the four dashes.
            uint8_t hex[32];
            vst1q_u8(hex, z.val[0]);
            vst1q_u8(hex + 16, z.val[1]);

            std::memcpy(out, hex, 8);
            std::memcpy(out + 9, hex + 8, 4);
            std::memcpy(out + 14, hex + 12, 4);
            std::memcpy(out + 19, hex + 16, 4);
            std::memcpy(out + 24, hex + 20, 12);

            out[8] = '-';
            out[13] = '-';
            out[18] = '-';
            out[23] = '-';
         }

         inline bool hex32_neon(char const* src, uint8_t* out) noexcept
         {
            uint8x16_t c0 = vld1q_u8(reinterpret_cast<uint8_t const*>(src));
            uint8x16_t c1 = vld1q_u8(reinterpret_cast<uint8_t const*>(src + 16));
            if (!hex16_valid_neon(c0) || !hex16_valid_neon(c1))
               return false;
            store_pairs_neon(hex_nibbles_neon(c0), out);
            store_pairs_neon(hex_nibbles_neon(c1), out + 8);
            return true;
         }
#endif

         // --------------------------------------------------------------------
         // Dispatch
         // --------------------------------------------------------------------
         [[nodiscard]] inline bool have_ssse3() noexcept
         {
#if defined(__SSSE3__) || defined(STDUUID_SSSE3_COMPILE_TIME)
            return true;
#elif defined(STDUUID_X86_RUNTIME_DISPATCH)
            return __builtin_cpu_supports("ssse3") != 0;
#else
            return false;
#endif
         }

         inline void encode36(uint8_t const* data, char* out) noexcept
         {
#if defined(STDUUID_SSSE3_CODE)
            if (have_ssse3())
            {
               encode36_ssse3(data, out);
               return;
            }
#endif
#if defined(STDUUID_ARCH_NEON)
            encode36_neon(data, out);
            return;
#endif
            encode36_scalar(data, out);
         }

         [[nodiscard]] inline bool hex32(char const* src, uint8_t* out) noexcept
         {
#if defined(STDUUID_SSSE3_CODE)
            if (have_ssse3())
               return hex32_ssse3(src, out);
#endif
#if defined(STDUUID_ARCH_NEON)
            return hex32_neon(src, out);
#endif
            return hex32_scalar(src, out);
         }

         // Gather the 32 hexadecimal digits of the canonical forms
         //   xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx      (36 characters)
         //   {xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx}    (38 characters)
         //   xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx          (32 characters)
         // into a contiguous buffer.  Returns false for any other layout; the
         // scalar parser then handles the (rare) unusual spellings, such as
         // dashes in different places.
         [[nodiscard]] inline bool gather_hex32(char const* str, size_t size, char* hex) noexcept
         {
            if (size == 32)
            {
               std::memcpy(hex, str, 32);
               return true;
            }

            if (size == 38)
            {
               if (str[0] != '{' || str[37] != '}')
                  return false;
               ++str;
            }
            else if (size != 36)
            {
               return false;
            }

            if (str[8] != '-' || str[13] != '-' || str[18] != '-' || str[23] != '-')
               return false;

            std::memcpy(hex, str, 8);
            std::memcpy(hex + 8, str + 9, 4);
            std::memcpy(hex + 12, str + 14, 4);
            std::memcpy(hex + 16, str + 19, 4);
            std::memcpy(hex + 20, str + 24, 12);
            return true;
         }

         [[nodiscard]] inline parse_result parse_canonical(char const* str, size_t size, uint8_t* out) noexcept
         {
            if (size != 32 && size != 36 && size != 38)
               return parse_not_applicable;

            char hex[32];
            if (!gather_hex32(str, size, hex))
               return parse_not_applicable;

            return hex32(hex, out) ? parse_ok : parse_invalid;
         }

         // --------------------------------------------------------------------
         // Byte comparisons
         // --------------------------------------------------------------------
         [[nodiscard]] inline bool is_zero16(uint8_t const* data) noexcept
         {
#if defined(STDUUID_SSE2_CODE)
            // Most uuids are not nil and differ in their very first byte.  A
            // scalar 64-bit test keeps that case as cheap as the early-exit
            // loop in the original code; the full 16-byte SIMD compare then
            // confirms the remaining bytes.
            uint64_t first;
            std::memcpy(&first, data, sizeof(first));
            if (STDUUID_LIKELY(first != 0))
               return false;

            __m128i v = _mm_loadu_si128(reinterpret_cast<__m128i const*>(data));
            return _mm_movemask_epi8(_mm_cmpeq_epi8(v, _mm_setzero_si128())) == 0xFFFF;
#elif defined(STDUUID_ARCH_NEON)
            uint64_t first;
            std::memcpy(&first, data, sizeof(first));
            if (STDUUID_LIKELY(first != 0))
               return false;

#  if defined(__aarch64__) || defined(_M_ARM64)
            return vmaxvq_u8(vld1q_u8(data)) == 0;
#  else
            uint8x16_t v = vld1q_u8(data);
            uint8x8_t lo = vorr_u8(vget_low_u8(v), vget_high_u8(v));
            return vget_lane_u64(vreinterpret_u64_u8(lo), 0) == 0;
#  endif
#else
            for (size_t i = 0; i < 16; ++i)
               if (data[i] != 0)
                  return false;
            return true;
#endif
         }

         [[nodiscard]] inline bool equal16(uint8_t const* lhs, uint8_t const* rhs) noexcept
         {
#if defined(STDUUID_SSE2_CODE) || defined(STDUUID_ARCH_NEON)
            // Two uuids that differ almost always differ in the first word, so
            // test that with a single 64-bit compare before doing the full
            // 16-byte vector comparison.  STDUUID_LIKELY keeps the branch from
            // being if-converted away when this helper is inlined.
            uint64_t a;
            uint64_t b;
            std::memcpy(&a, lhs, sizeof(a));
            std::memcpy(&b, rhs, sizeof(b));
            if (STDUUID_LIKELY(a != b))
               return false;
#endif

#if defined(STDUUID_SSE2_CODE)
            __m128i va = _mm_loadu_si128(reinterpret_cast<__m128i const*>(lhs));
            __m128i vb = _mm_loadu_si128(reinterpret_cast<__m128i const*>(rhs));
            return _mm_movemask_epi8(_mm_cmpeq_epi8(va, vb)) == 0xFFFF;
#elif defined(STDUUID_ARCH_NEON)
#  if defined(__aarch64__) || defined(_M_ARM64)
            return vminvq_u8(vceqq_u8(vld1q_u8(lhs), vld1q_u8(rhs))) != 0;
#  else
            uint8x16_t v = vceqq_u8(vld1q_u8(lhs), vld1q_u8(rhs));
            uint8x8_t lo = vand_u8(vget_low_u8(v), vget_high_u8(v));
            return vget_lane_u64(vreinterpret_u64_u8(lo), 0) == 0xFFFFFFFFFFFFFFFFULL;
#  endif
#else
            return std::memcmp(lhs, rhs, 16) == 0;
#endif
         }

         // --------------------------------------------------------------------
         // SHA-1 compression using the x86 SHA extensions (sha_ni)
         //
         // The instruction sequence follows the public domain example published
         // together with the Intel SHA extensions.
         // --------------------------------------------------------------------
#if defined(STDUUID_SHA1_CODE)
         STDUUID_TARGET_SHA1
         inline void sha1_transform_sha1ni(uint32_t state[5], uint8_t const* data) noexcept
         {
            __m128i abcd = _mm_loadu_si128(reinterpret_cast<__m128i const*>(state));
            __m128i e0 = _mm_set_epi32(static_cast<int>(state[4]), 0, 0, 0);
            abcd = _mm_shuffle_epi32(abcd, 0x1B);

            __m128i abcd_save = abcd;
            __m128i e0_save = e0;
            __m128i e1;
            __m128i msg0, msg1, msg2, msg3;

            const __m128i mask = _mm_set_epi64x(
               static_cast<long long>(0x0001020304050607ULL),
               static_cast<long long>(0x08090a0b0c0d0e0fULL));

            // Rounds 0-3
            msg0 = _mm_shuffle_epi8(
               _mm_loadu_si128(reinterpret_cast<__m128i const*>(data)), mask);
            e0 = _mm_add_epi32(e0, msg0);
            e1 = abcd;
            abcd = _mm_sha1rnds4_epu32(abcd, e0, 0);

            // Rounds 4-7
            msg1 = _mm_shuffle_epi8(
               _mm_loadu_si128(reinterpret_cast<__m128i const*>(data + 16)), mask);
            e1 = _mm_sha1nexte_epu32(e1, msg1);
            e0 = abcd;
            abcd = _mm_sha1rnds4_epu32(abcd, e1, 0);
            msg0 = _mm_sha1msg1_epu32(msg0, msg1);

            // Rounds 8-11
            msg2 = _mm_shuffle_epi8(
               _mm_loadu_si128(reinterpret_cast<__m128i const*>(data + 32)), mask);
            e0 = _mm_sha1nexte_epu32(e0, msg2);
            e1 = abcd;
            abcd = _mm_sha1rnds4_epu32(abcd, e0, 0);
            msg1 = _mm_sha1msg1_epu32(msg1, msg2);
            msg0 = _mm_xor_si128(msg0, msg2);

            // Rounds 12-15
            msg3 = _mm_shuffle_epi8(
               _mm_loadu_si128(reinterpret_cast<__m128i const*>(data + 48)), mask);
            e1 = _mm_sha1nexte_epu32(e1, msg3);
            e0 = abcd;
            msg0 = _mm_sha1msg2_epu32(msg0, msg3);
            abcd = _mm_sha1rnds4_epu32(abcd, e1, 0);
            msg2 = _mm_sha1msg1_epu32(msg2, msg3);
            msg1 = _mm_xor_si128(msg1, msg3);

            // Rounds 16-19
            e0 = _mm_sha1nexte_epu32(e0, msg0);
            e1 = abcd;
            msg1 = _mm_sha1msg2_epu32(msg1, msg0);
            abcd = _mm_sha1rnds4_epu32(abcd, e0, 0);
            msg3 = _mm_sha1msg1_epu32(msg3, msg0);
            msg2 = _mm_xor_si128(msg2, msg0);

            // Rounds 20-23
            e1 = _mm_sha1nexte_epu32(e1, msg1);
            e0 = abcd;
            msg2 = _mm_sha1msg2_epu32(msg2, msg1);
            abcd = _mm_sha1rnds4_epu32(abcd, e1, 1);
            msg0 = _mm_sha1msg1_epu32(msg0, msg1);
            msg3 = _mm_xor_si128(msg3, msg1);

            // Rounds 24-27
            e0 = _mm_sha1nexte_epu32(e0, msg2);
            e1 = abcd;
            msg3 = _mm_sha1msg2_epu32(msg3, msg2);
            abcd = _mm_sha1rnds4_epu32(abcd, e0, 1);
            msg1 = _mm_sha1msg1_epu32(msg1, msg2);
            msg0 = _mm_xor_si128(msg0, msg2);

            // Rounds 28-31
            e1 = _mm_sha1nexte_epu32(e1, msg3);
            e0 = abcd;
            msg0 = _mm_sha1msg2_epu32(msg0, msg3);
            abcd = _mm_sha1rnds4_epu32(abcd, e1, 1);
            msg2 = _mm_sha1msg1_epu32(msg2, msg3);
            msg1 = _mm_xor_si128(msg1, msg3);

            // Rounds 32-35
            e0 = _mm_sha1nexte_epu32(e0, msg0);
            e1 = abcd;
            msg1 = _mm_sha1msg2_epu32(msg1, msg0);
            abcd = _mm_sha1rnds4_epu32(abcd, e0, 1);
            msg3 = _mm_sha1msg1_epu32(msg3, msg0);
            msg2 = _mm_xor_si128(msg2, msg0);

            // Rounds 36-39
            e1 = _mm_sha1nexte_epu32(e1, msg1);
            e0 = abcd;
            msg2 = _mm_sha1msg2_epu32(msg2, msg1);
            abcd = _mm_sha1rnds4_epu32(abcd, e1, 1);
            msg0 = _mm_sha1msg1_epu32(msg0, msg1);
            msg3 = _mm_xor_si128(msg3, msg1);

            // Rounds 40-43
            e0 = _mm_sha1nexte_epu32(e0, msg2);
            e1 = abcd;
            msg3 = _mm_sha1msg2_epu32(msg3, msg2);
            abcd = _mm_sha1rnds4_epu32(abcd, e0, 2);
            msg1 = _mm_sha1msg1_epu32(msg1, msg2);
            msg0 = _mm_xor_si128(msg0, msg2);

            // Rounds 44-47
            e1 = _mm_sha1nexte_epu32(e1, msg3);
            e0 = abcd;
            msg0 = _mm_sha1msg2_epu32(msg0, msg3);
            abcd = _mm_sha1rnds4_epu32(abcd, e1, 2);
            msg2 = _mm_sha1msg1_epu32(msg2, msg3);
            msg1 = _mm_xor_si128(msg1, msg3);

            // Rounds 48-51
            e0 = _mm_sha1nexte_epu32(e0, msg0);
            e1 = abcd;
            msg1 = _mm_sha1msg2_epu32(msg1, msg0);
            abcd = _mm_sha1rnds4_epu32(abcd, e0, 2);
            msg3 = _mm_sha1msg1_epu32(msg3, msg0);
            msg2 = _mm_xor_si128(msg2, msg0);

            // Rounds 52-55
            e1 = _mm_sha1nexte_epu32(e1, msg1);
            e0 = abcd;
            msg2 = _mm_sha1msg2_epu32(msg2, msg1);
            abcd = _mm_sha1rnds4_epu32(abcd, e1, 2);
            msg0 = _mm_sha1msg1_epu32(msg0, msg1);
            msg3 = _mm_xor_si128(msg3, msg1);

            // Rounds 56-59
            e0 = _mm_sha1nexte_epu32(e0, msg2);
            e1 = abcd;
            msg3 = _mm_sha1msg2_epu32(msg3, msg2);
            abcd = _mm_sha1rnds4_epu32(abcd, e0, 2);
            msg1 = _mm_sha1msg1_epu32(msg1, msg2);
            msg0 = _mm_xor_si128(msg0, msg2);

            // Rounds 60-63
            e1 = _mm_sha1nexte_epu32(e1, msg3);
            e0 = abcd;
            msg0 = _mm_sha1msg2_epu32(msg0, msg3);
            abcd = _mm_sha1rnds4_epu32(abcd, e1, 3);
            msg2 = _mm_sha1msg1_epu32(msg2, msg3);
            msg1 = _mm_xor_si128(msg1, msg3);

            // Rounds 64-67
            e0 = _mm_sha1nexte_epu32(e0, msg0);
            e1 = abcd;
            msg1 = _mm_sha1msg2_epu32(msg1, msg0);
            abcd = _mm_sha1rnds4_epu32(abcd, e0, 3);
            msg3 = _mm_sha1msg1_epu32(msg3, msg0);
            msg2 = _mm_xor_si128(msg2, msg0);

            // Rounds 68-71
            e1 = _mm_sha1nexte_epu32(e1, msg1);
            e0 = abcd;
            msg2 = _mm_sha1msg2_epu32(msg2, msg1);
            abcd = _mm_sha1rnds4_epu32(abcd, e1, 3);
            msg3 = _mm_xor_si128(msg3, msg1);

            // Rounds 72-75
            e0 = _mm_sha1nexte_epu32(e0, msg2);
            e1 = abcd;
            msg3 = _mm_sha1msg2_epu32(msg3, msg2);
            abcd = _mm_sha1rnds4_epu32(abcd, e0, 3);

            // Rounds 76-79
            e1 = _mm_sha1nexte_epu32(e1, msg3);
            e0 = abcd;
            abcd = _mm_sha1rnds4_epu32(abcd, e1, 3);

            // Combine state
            e0 = _mm_sha1nexte_epu32(e0, e0_save);
            abcd = _mm_add_epi32(abcd, abcd_save);

            abcd = _mm_shuffle_epi32(abcd, 0x1B);
            _mm_storeu_si128(reinterpret_cast<__m128i*>(state), abcd);
            state[4] = static_cast<uint32_t>(_mm_cvtsi128_si32(_mm_srli_si128(e0, 12)));
         }
#endif

         [[nodiscard]] inline bool have_sha1() noexcept
         {
#if defined(STDUUID_SHA1_CODE)
#  if defined(__SHA__)
            return true;
#  else
            return __builtin_cpu_supports("sha") != 0 &&
                   __builtin_cpu_supports("ssse3") != 0 &&
                   __builtin_cpu_supports("sse4.1") != 0;
#  endif
#else
            return false;
#endif
         }

         inline bool sha1_try_transform(uint32_t state[5], uint8_t const* data) noexcept
         {
#if defined(STDUUID_SHA1_CODE)
            if (have_sha1())
            {
               sha1_transform_sha1ni(state, data);
               return true;
            }
#endif
            (void)state;
            (void)data;
            return false;
         }

#undef STDUUID_TARGET_SSSE3
#undef STDUUID_TARGET_SHA1
      } // namespace simd

      template <typename TChar>
      [[nodiscard]] constexpr inline unsigned char hex2char(TChar const ch) noexcept
      {
         if (ch >= static_cast<TChar>('0') && ch <= static_cast<TChar>('9'))
            return static_cast<unsigned char>(ch - static_cast<TChar>('0'));
         if (ch >= static_cast<TChar>('a') && ch <= static_cast<TChar>('f'))
            return static_cast<unsigned char>(10 + ch - static_cast<TChar>('a'));
         if (ch >= static_cast<TChar>('A') && ch <= static_cast<TChar>('F'))
            return static_cast<unsigned char>(10 + ch - static_cast<TChar>('A'));
         return 0;
      }

      template <typename TChar>
      [[nodiscard]] constexpr inline bool is_hex(TChar const ch) noexcept
      {
         return
            (ch >= static_cast<TChar>('0') && ch <= static_cast<TChar>('9')) ||
            (ch >= static_cast<TChar>('a') && ch <= static_cast<TChar>('f')) ||
            (ch >= static_cast<TChar>('A') && ch <= static_cast<TChar>('F'));
      }

      template <typename TChar>
      [[nodiscard]] constexpr std::basic_string_view<TChar> to_string_view(TChar const * str) noexcept
      {
         if (str) return str;
         return {};
      }

      template <typename StringType>
	  [[nodiscard]] 
      constexpr std::basic_string_view<
         typename StringType::value_type,
         typename StringType::traits_type>
      to_string_view(StringType const & str) noexcept
      {
         return str;
      }

      class sha1
      {
      public:
         using digest32_t = uint32_t[5];
         using digest8_t = uint8_t[20];

         static constexpr unsigned int block_bytes = 64;

         [[nodiscard]] inline static uint32_t left_rotate(uint32_t value, size_t const count) noexcept
         {
            return (value << count) ^ (value >> (32 - count));
         }

         sha1() { reset(); }

         void reset() noexcept
         {
            m_digest[0] = 0x67452301;
            m_digest[1] = 0xEFCDAB89;
            m_digest[2] = 0x98BADCFE;
            m_digest[3] = 0x10325476;
            m_digest[4] = 0xC3D2E1F0;
            m_blockByteIndex = 0;
            m_byteCount = 0;
         }

         void process_byte(uint8_t octet) 
         {
            this->m_block[this->m_blockByteIndex++] = octet;
            ++this->m_byteCount;
            if (m_blockByteIndex == block_bytes)
            {
               this->m_blockByteIndex = 0;
               process_block();
            }
         }

         void process_block(void const * const start, void const * const end) 
         {
            const uint8_t* begin = static_cast<const uint8_t*>(start);
            const uint8_t* finish = static_cast<const uint8_t*>(end);
            while (begin != finish) 
            {
               process_byte(*begin);
               begin++;
            }
         }

         void process_bytes(void const * const data, size_t const len)
         {
            const uint8_t* block = static_cast<const uint8_t*>(data);
            process_block(block, block + len);
         }

         uint32_t const * get_digest(digest32_t digest) 
         {
            size_t const bitCount = this->m_byteCount * 8;
            process_byte(0x80);
            if (this->m_blockByteIndex > 56) {
               while (m_blockByteIndex != 0) {
                  process_byte(0);
               }
               while (m_blockByteIndex < 56) {
                  process_byte(0);
               }
            }
            else {
               while (m_blockByteIndex < 56) {
                  process_byte(0);
               }
            }
            process_byte(0);
            process_byte(0);
            process_byte(0);
            process_byte(0);
            process_byte(static_cast<unsigned char>((bitCount >> 24) & 0xFF));
            process_byte(static_cast<unsigned char>((bitCount >> 16) & 0xFF));
            process_byte(static_cast<unsigned char>((bitCount >> 8) & 0xFF));
            process_byte(static_cast<unsigned char>((bitCount) & 0xFF));

            memcpy(digest, m_digest, 5 * sizeof(uint32_t));
            return digest;
         }

         uint8_t const * get_digest_bytes(digest8_t digest) 
         {
            digest32_t d32;
            get_digest(d32);
            size_t di = 0;
            digest[di++] = static_cast<uint8_t>(d32[0] >> 24);
            digest[di++] = static_cast<uint8_t>(d32[0] >> 16);
            digest[di++] = static_cast<uint8_t>(d32[0] >> 8);
            digest[di++] = static_cast<uint8_t>(d32[0] >> 0);

            digest[di++] = static_cast<uint8_t>(d32[1] >> 24);
            digest[di++] = static_cast<uint8_t>(d32[1] >> 16);
            digest[di++] = static_cast<uint8_t>(d32[1] >> 8);
            digest[di++] = static_cast<uint8_t>(d32[1] >> 0);

            digest[di++] = static_cast<uint8_t>(d32[2] >> 24);
            digest[di++] = static_cast<uint8_t>(d32[2] >> 16);
            digest[di++] = static_cast<uint8_t>(d32[2] >> 8);
            digest[di++] = static_cast<uint8_t>(d32[2] >> 0);

            digest[di++] = static_cast<uint8_t>(d32[3] >> 24);
            digest[di++] = static_cast<uint8_t>(d32[3] >> 16);
            digest[di++] = static_cast<uint8_t>(d32[3] >> 8);
            digest[di++] = static_cast<uint8_t>(d32[3] >> 0);

            digest[di++] = static_cast<uint8_t>(d32[4] >> 24);
            digest[di++] = static_cast<uint8_t>(d32[4] >> 16);
            digest[di++] = static_cast<uint8_t>(d32[4] >> 8);
            digest[di++] = static_cast<uint8_t>(d32[4] >> 0);

            return digest;
         }

      private:
         void process_block() 
         {
            // The SHA extensions do the whole compression function in a
            // handful of instructions.  They are selected at run time so the
            // library keeps working on processors without them.
            if (detail::simd::sha1_try_transform(m_digest, m_block))
               return;

            uint32_t w[80];
            for (size_t i = 0; i < 16; i++) {
               w[i] = static_cast<uint32_t>(m_block[i * 4 + 0] << 24);
               w[i] |= static_cast<uint32_t>(m_block[i * 4 + 1] << 16);
               w[i] |= static_cast<uint32_t>(m_block[i * 4 + 2] << 8);
               w[i] |= static_cast<uint32_t>(m_block[i * 4 + 3]);
            }
            for (size_t i = 16; i < 80; i++) {
               w[i] = left_rotate((w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16]), 1);
            }

            uint32_t a = m_digest[0];
            uint32_t b = m_digest[1];
            uint32_t c = m_digest[2];
            uint32_t d = m_digest[3];
            uint32_t e = m_digest[4];

            for (std::size_t i = 0; i < 80; ++i) 
            {
               uint32_t f = 0;
               uint32_t k = 0;

               if (i < 20) {
                  f = (b & c) | (~b & d);
                  k = 0x5A827999;
               }
               else if (i < 40) {
                  f = b ^ c ^ d;
                  k = 0x6ED9EBA1;
               }
               else if (i < 60) {
                  f = (b & c) | (b & d) | (c & d);
                  k = 0x8F1BBCDC;
               }
               else {
                  f = b ^ c ^ d;
                  k = 0xCA62C1D6;
               }
               uint32_t temp = left_rotate(a, 5) + f + e + k + w[i];
               e = d;
               d = c;
               c = left_rotate(b, 30);
               b = a;
               a = temp;
            }

            m_digest[0] += a;
            m_digest[1] += b;
            m_digest[2] += c;
            m_digest[3] += d;
            m_digest[4] += e;
         }

      private:
         digest32_t m_digest;
         uint8_t m_block[64];
         size_t m_blockByteIndex;
         size_t m_byteCount;
      };

      template <typename CharT>
      inline constexpr CharT empty_guid[37] = "00000000-0000-0000-0000-000000000000";

      template <>
      inline constexpr wchar_t empty_guid<wchar_t>[37] = L"00000000-0000-0000-0000-000000000000";

      template <typename CharT>
      inline constexpr CharT guid_encoder[17] = "0123456789abcdef";

      template <>
      inline constexpr wchar_t guid_encoder<wchar_t>[17] = L"0123456789abcdef";
   }

   // --------------------------------------------------------------------------------------------------------------------------
   // UUID format https://tools.ietf.org/html/rfc4122
   // --------------------------------------------------------------------------------------------------------------------------

   // --------------------------------------------------------------------------------------------------------------------------
   // Field	                     NDR Data Type	   Octet #	Note
   // --------------------------------------------------------------------------------------------------------------------------
   // time_low	                  unsigned long	   0 - 3	   The low field of the timestamp.
   // time_mid	                  unsigned short	   4 - 5	   The middle field of the timestamp.
   // time_hi_and_version	      unsigned short	   6 - 7	   The high field of the timestamp multiplexed with the version number.
   // clock_seq_hi_and_reserved	unsigned small	   8	      The high field of the clock sequence multiplexed with the variant.
   // clock_seq_low	            unsigned small	   9	      The low field of the clock sequence.
   // node	                     character	      10 - 15	The spatially unique node identifier.
   // --------------------------------------------------------------------------------------------------------------------------
   // 0                   1                   2                   3
   //  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
   // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   // |                          time_low                             |
   // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   // |       time_mid                |         time_hi_and_version   |
   // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   // |clk_seq_hi_res |  clk_seq_low  |         node (0-1)            |
   // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   // |                         node (2-5)                            |
   // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

   // --------------------------------------------------------------------------------------------------------------------------
   // enumerations
   // --------------------------------------------------------------------------------------------------------------------------

   // indicated by a bit pattern in octet 8, marked with N in xxxxxxxx-xxxx-xxxx-Nxxx-xxxxxxxxxxxx
   enum class uuid_variant
   {
      // NCS backward compatibility (with the obsolete Apollo Network Computing System 1.5 UUID format)
      // N bit pattern: 0xxx
      // > the first 6 octets of the UUID are a 48-bit timestamp (the number of 4 microsecond units of time since 1 Jan 1980 UTC);
      // > the next 2 octets are reserved;
      // > the next octet is the "address family"; 
      // > the final 7 octets are a 56-bit host ID in the form specified by the address family
      ncs,
      
      // RFC 4122/DCE 1.1 
      // N bit pattern: 10xx
      // > big-endian byte order
      rfc,
      
      // Microsoft Corporation backward compatibility
      // N bit pattern: 110x
      // > little endian byte order
      // > formely used in the Component Object Model (COM) library      
      microsoft,
      
      // reserved for possible future definition
      // N bit pattern: 111x      
      reserved
   };

   // indicated by a bit pattern in octet 6, marked with M in xxxxxxxx-xxxx-Mxxx-xxxx-xxxxxxxxxxxx   
   enum class uuid_version
   {
      none = 0, // only possible for nil or invalid uuids
      time_based = 1,  // The time-based version specified in RFC 4122
      dce_security = 2,  // DCE Security version, with embedded POSIX UIDs.
      name_based_md5 = 3,  // The name-based version specified in RFS 4122 with MD5 hashing
      random_number_based = 4,  // The randomly or pseudo-randomly generated version specified in RFS 4122
      name_based_sha1 = 5   // The name-based version specified in RFS 4122 with SHA1 hashing
   };

   // Forward declare uuid & to_string so that we can declare to_string as a friend later.
   class uuid;
   template <class CharT = char,
             class Traits = std::char_traits<CharT>,
             class Allocator = std::allocator<CharT>>
   std::basic_string<CharT, Traits, Allocator> to_string(uuid const &id);

   template <class CharT = char, class OutputIt>
   OutputIt to_string(uuid const &id, OutputIt out);

   // --------------------------------------------------------------------------------------------------------------------------
   // uuid class
   // --------------------------------------------------------------------------------------------------------------------------
   class uuid
   {
   public:
      using value_type = uint8_t;

      constexpr uuid() noexcept = default;

      uuid(value_type(&arr)[16]) noexcept
      {
         std::copy(std::cbegin(arr), std::cend(arr), std::begin(data));
      }

      constexpr uuid(std::array<value_type, 16> const & arr) noexcept : data{arr} {}

      explicit uuid(span<value_type, 16> bytes)
      {
         std::copy(std::cbegin(bytes), std::cend(bytes), std::begin(data));
      }
      
      template<typename ForwardIterator>
      explicit uuid(ForwardIterator first, ForwardIterator last)
      {
         if (std::distance(first, last) == 16)
            std::copy(first, last, std::begin(data));
      }
      
      [[nodiscard]] constexpr uuid_variant variant() const noexcept
      {
         if ((data[8] & 0x80) == 0x00)
            return uuid_variant::ncs;
         else if ((data[8] & 0xC0) == 0x80)
            return uuid_variant::rfc;
         else if ((data[8] & 0xE0) == 0xC0)
            return uuid_variant::microsoft;
         else
            return uuid_variant::reserved;
      }

      [[nodiscard]] constexpr uuid_version version() const noexcept
      {
         if ((data[6] & 0xF0) == 0x10)
            return uuid_version::time_based;
         else if ((data[6] & 0xF0) == 0x20)
            return uuid_version::dce_security;
         else if ((data[6] & 0xF0) == 0x30)
            return uuid_version::name_based_md5;
         else if ((data[6] & 0xF0) == 0x40)
            return uuid_version::random_number_based;
         else if ((data[6] & 0xF0) == 0x50)
            return uuid_version::name_based_sha1;
         else
            return uuid_version::none;
      }

      [[nodiscard]] constexpr bool is_nil() const noexcept
      {
         // Constant evaluation must not reach the (non-constexpr) SIMD helper;
         // at run time the whole check becomes a single 16-byte comparison.
         if (!STDUUID_IS_CONSTANT_EVALUATED())
            return detail::simd::is_zero16(data.data());

         for (size_t i = 0; i < data.size(); ++i) if (data[i] != 0) return false;
         return true;
      }

      void swap(uuid & other) noexcept
      {
         data.swap(other.data);
      }

      [[nodiscard]] inline span<std::byte const, 16> as_bytes() const
      {
         return span<std::byte const, 16>(reinterpret_cast<std::byte const*>(data.data()), 16);
      }

      template <typename StringType>
      [[nodiscard]] constexpr static bool is_valid_uuid(StringType const & in_str) noexcept
      {
         auto str = detail::to_string_view(in_str);

         // The canonical spellings (with or without braces/dashes) are the
         // overwhelmingly common case.  Validate them 16 bytes at a time and
         // leave everything else to the fully general scalar parser below.
         if (!STDUUID_IS_CONSTANT_EVALUATED())
         {
            if constexpr (std::is_same_v<std::remove_cv_t<typename decltype(str)::value_type>, char>)
            {
               uint8_t bytes[16] = { 0 };
               auto r = detail::simd::parse_canonical(str.data(), str.size(), bytes);
               if (r == detail::simd::parse_ok)
                  return true;
               if (r == detail::simd::parse_invalid)
                  return false;
            }
         }

         bool firstDigit = true;
         size_t hasBraces = 0;
         size_t index = 0;

         if (str.empty())
            return false;

         if (str.front() == '{')
            hasBraces = 1;
         if (hasBraces && str.back() != '}')
            return false;

         for (size_t i = hasBraces; i < str.size() - hasBraces; ++i)
         {
            if (str[i] == '-') continue;

            if (index >= 16 || !detail::is_hex(str[i]))
            {
               return false;
            }

            if (firstDigit)
            {
               firstDigit = false;
            }
            else
            {
               index++;
               firstDigit = true;
            }
         }

         if (index < 16)
         {
            return false;
         }

         return true;
      }

      template <typename StringType>
      [[nodiscard]] constexpr static std::optional<uuid> from_string(StringType const & in_str) noexcept
      {
         auto str = detail::to_string_view(in_str);

         std::array<uint8_t, 16> data{ { 0 } };

         // Fast path for the canonical spellings; fills the whole uuid at once.
         if (!STDUUID_IS_CONSTANT_EVALUATED())
         {
            if constexpr (std::is_same_v<std::remove_cv_t<typename decltype(str)::value_type>, char>)
            {
               auto r = detail::simd::parse_canonical(str.data(), str.size(), data.data());
               if (r == detail::simd::parse_ok)
                  return uuid{ data };
               if (r == detail::simd::parse_invalid)
                  return {};
            }
         }

         bool firstDigit = true;
         size_t hasBraces = 0;
         size_t index = 0;

         if (str.empty()) return {};

         if (str.front() == '{')
            hasBraces = 1;
         if (hasBraces && str.back() != '}')
            return {};

         for (size_t i = hasBraces; i < str.size() - hasBraces; ++i)
         {
            if (str[i] == '-') continue;

            if (index >= 16 || !detail::is_hex(str[i]))
            {
               return {};
            }

            if (firstDigit)
            {
               data[index] = static_cast<uint8_t>(detail::hex2char(str[i]) << 4);
               firstDigit = false;
            }
            else
            {
               data[index] = static_cast<uint8_t>(data[index] | detail::hex2char(str[i]));
               index++;
               firstDigit = true;
            }
         }

         if (index < 16)
         {
            return {};
         }

         return uuid{ data };
      }

   private:
      std::array<value_type, 16> data{ { 0 } };

      friend bool operator==(uuid const & lhs, uuid const & rhs) noexcept;
      friend bool operator<(uuid const & lhs, uuid const & rhs) noexcept;

      template <class Elem, class Traits>
      friend std::basic_ostream<Elem, Traits> & operator<<(std::basic_ostream<Elem, Traits> &s, uuid const & id);

      template<class CharT, class Traits, class Allocator>
      friend std::basic_string<CharT, Traits, Allocator> to_string(uuid const& id);

      template<class CharT, class OutputIt>
      friend OutputIt to_string(uuid const& id, OutputIt out);

      friend std::hash<uuid>;
   };

   // --------------------------------------------------------------------------------------------------------------------------
   // operators and non-member functions
   // --------------------------------------------------------------------------------------------------------------------------

   [[nodiscard]] inline bool operator== (uuid const& lhs, uuid const& rhs) noexcept
   {
      return detail::simd::equal16(lhs.data.data(), rhs.data.data());
   }

   [[nodiscard]] inline bool operator!= (uuid const& lhs, uuid const& rhs) noexcept
   {
      return !(lhs == rhs);
   }

   [[nodiscard]] inline bool operator< (uuid const& lhs, uuid const& rhs) noexcept
   {
      return lhs.data < rhs.data;
   }

   // Writes the 36 characters of the canonical textual representation to an output iterator, without allocating a string of its own. 
   // This is what the overload below is implemented in terms of, so both always agree.
   template <class CharT, class OutputIt>
   inline OutputIt to_string(uuid const & id, OutputIt out)
   {
      // When the caller hands us a plain char* (operator<< does, and so does
      // anyone formatting into a stack buffer) the whole textual form is
      // written with one SIMD hex conversion.  Generic output iterators keep
      // the portable character-at-a-time loop.
      if constexpr (std::is_same_v<CharT, char> && std::is_same_v<OutputIt, char*>)
      {
         detail::simd::encode36(id.data.data(), out);
         return out + 36;
      }
      else
      {
         for (size_t index = 0; index < 16; ++index)
         {
            if (index == 4 || index == 6 || index == 8 || index == 10)
            {
               *out++ = static_cast<CharT>('-');
            }
            *out++ = detail::guid_encoder<CharT>[id.data[index] >> 4 & 0x0f];
            *out++ = detail::guid_encoder<CharT>[id.data[index] & 0x0f];
         }

         return out;
      }
   }

   template <class CharT,
             class Traits,
             class Allocator>
   [[nodiscard]] inline std::basic_string<CharT, Traits, Allocator> to_string(uuid const & id)
   {
      std::basic_string<CharT, Traits, Allocator> uustr{detail::empty_guid<CharT>};

      if constexpr (std::is_same_v<CharT, char>)
      {
         detail::simd::encode36(id.data.data(), uustr.data());
      }
      else
      {
         to_string<CharT>(id, uustr.begin());
      }

      return uustr;
   }

   template <class Elem, class Traits>
   std::basic_ostream<Elem, Traits>& operator<<(std::basic_ostream<Elem, Traits>& s, uuid const& id)
   {
       Elem buffer[36];
       to_string<Elem>(id, buffer);

       s << std::basic_string_view<Elem, Traits>(buffer, 36);
       return s;
   }

   inline void swap(uuids::uuid & lhs, uuids::uuid & rhs) noexcept
   {
      lhs.swap(rhs);   
   }

   // --------------------------------------------------------------------------------------------------------------------------
   // namespace IDs that could be used for generating name-based uuids
   // --------------------------------------------------------------------------------------------------------------------------

   // Name string is a fully-qualified domain name
   static uuid uuid_namespace_dns{ {0x6b, 0xa7, 0xb8, 0x10, 0x9d, 0xad, 0x11, 0xd1, 0x80, 0xb4, 0x00, 0xc0, 0x4f, 0xd4, 0x30, 0xc8} };

   // Name string is a URL
   static uuid uuid_namespace_url{ {0x6b, 0xa7, 0xb8, 0x11, 0x9d, 0xad, 0x11, 0xd1, 0x80, 0xb4, 0x00, 0xc0, 0x4f, 0xd4, 0x30, 0xc8} };

   // Name string is an ISO OID (See https://oidref.com/, https://en.wikipedia.org/wiki/Object_identifier)
   static uuid uuid_namespace_oid{ {0x6b, 0xa7, 0xb8, 0x12, 0x9d, 0xad, 0x11, 0xd1, 0x80, 0xb4, 0x00, 0xc0, 0x4f, 0xd4, 0x30, 0xc8} };

   // Name string is an X.500 DN, in DER or a text output format (See https://en.wikipedia.org/wiki/X.500, https://en.wikipedia.org/wiki/Abstract_Syntax_Notation_One)
   static uuid uuid_namespace_x500{ {0x6b, 0xa7, 0xb8, 0x14, 0x9d, 0xad, 0x11, 0xd1, 0x80, 0xb4, 0x00, 0xc0, 0x4f, 0xd4, 0x30, 0xc8} };

   // --------------------------------------------------------------------------------------------------------------------------
   // uuid generators
   // --------------------------------------------------------------------------------------------------------------------------

#ifdef UUID_SYSTEM_GENERATOR
   class uuid_system_generator
   {
   public:
      using result_type = uuid;

      uuid operator()()
      {
#ifdef _WIN32

         GUID newId;
         HRESULT hr = ::CoCreateGuid(&newId);

         if (FAILED(hr))
         {
            throw std::system_error(hr, std::system_category(), "CoCreateGuid failed");
         }

         std::array<uint8_t, 16> bytes =
         { {
               static_cast<unsigned char>((newId.Data1 >> 24) & 0xFF),
               static_cast<unsigned char>((newId.Data1 >> 16) & 0xFF),
               static_cast<unsigned char>((newId.Data1 >> 8) & 0xFF),
               static_cast<unsigned char>((newId.Data1) & 0xFF),

               (unsigned char)((newId.Data2 >> 8) & 0xFF),
               (unsigned char)((newId.Data2) & 0xFF),

               (unsigned char)((newId.Data3 >> 8) & 0xFF),
               (unsigned char)((newId.Data3) & 0xFF),

               newId.Data4[0],
               newId.Data4[1],
               newId.Data4[2],
               newId.Data4[3],
               newId.Data4[4],
               newId.Data4[5],
               newId.Data4[6],
               newId.Data4[7]
            } };

         return uuid{ std::begin(bytes), std::end(bytes) };

#elif defined(__linux__) || defined(__unix__)

         uuid_t id;
         uuid_generate(id);

         std::array<uint8_t, 16> bytes =
         { {
               id[0],
               id[1],
               id[2],
               id[3],
               id[4],
               id[5],
               id[6],
               id[7],
               id[8],
               id[9],
               id[10],
               id[11],
               id[12],
               id[13],
               id[14],
               id[15]
            } };

         return uuid{ std::begin(bytes), std::end(bytes) };

#elif defined(__APPLE__)
         auto newId = CFUUIDCreate(NULL);
         auto bytes = CFUUIDGetUUIDBytes(newId);
         CFRelease(newId);

         std::array<uint8_t, 16> arrbytes =
         { {
               bytes.byte0,
               bytes.byte1,
               bytes.byte2,
               bytes.byte3,
               bytes.byte4,
               bytes.byte5,
               bytes.byte6,
               bytes.byte7,
               bytes.byte8,
               bytes.byte9,
               bytes.byte10,
               bytes.byte11,
               bytes.byte12,
               bytes.byte13,
               bytes.byte14,
               bytes.byte15
            } };
         return uuid{ std::begin(arrbytes), std::end(arrbytes) };
#else
         return uuid{};
#endif
      }
   };
#endif

   template <typename UniformRandomNumberGenerator>
   class basic_uuid_random_generator
   {
   public:
      using engine_type = UniformRandomNumberGenerator;

      explicit basic_uuid_random_generator(engine_type& gen) :
         generator(&gen, [](auto) {}) {}
      explicit basic_uuid_random_generator(engine_type* gen) :
         generator(gen, [](auto) {}) {}

      [[nodiscard]] uuid operator()()
      {
         alignas(uint32_t) uint8_t bytes[16];
         for (int i = 0; i < 16; i += 4)
            *reinterpret_cast<uint32_t*>(bytes + i) = distribution(*generator);

         // variant must be 10xxxxxx
         bytes[8] &= 0xBF;
         bytes[8] |= 0x80;

         // version must be 0100xxxx
         bytes[6] &= 0x4F;
         bytes[6] |= 0x40;

         return uuid{std::begin(bytes), std::end(bytes)};
      }

   private:
      std::uniform_int_distribution<uint32_t>  distribution;
      std::shared_ptr<UniformRandomNumberGenerator> generator;
   };

   using uuid_random_generator = basic_uuid_random_generator<std::mt19937>;
   
   class uuid_name_generator
   {
   public:
      explicit uuid_name_generator(uuid const& namespace_uuid) noexcept
         : nsuuid(namespace_uuid)
      {}

      template <typename StringType>
      [[nodiscard]] uuid operator()(StringType const & name)
      {
         reset();
         process_characters(detail::to_string_view(name));
         return make_uuid();
      }

   private:
      void reset() 
      {
         hasher.reset();
         std::byte bytes[16];
         auto nsbytes = nsuuid.as_bytes();
         std::copy(std::cbegin(nsbytes), std::cend(nsbytes), bytes);
         hasher.process_bytes(bytes, 16);
      }

      template <typename CharT, typename Traits>
      void process_characters(std::basic_string_view<CharT, Traits> const str)
      {
         for (uint32_t c : str)
         {
            hasher.process_byte(static_cast<uint8_t>(c & 0xFF));
            if constexpr (!std::is_same_v<CharT, char>)
            {
               hasher.process_byte(static_cast<uint8_t>((c >> 8) & 0xFF));
               hasher.process_byte(static_cast<uint8_t>((c >> 16) & 0xFF));
               hasher.process_byte(static_cast<uint8_t>((c >> 24) & 0xFF));
            }
         }
      }

      [[nodiscard]] uuid make_uuid()
      {
         detail::sha1::digest8_t digest;
         hasher.get_digest_bytes(digest);

         // variant must be 0b10xxxxxx
         digest[8] &= 0xBF;
         digest[8] |= 0x80;

         // version must be 0b0101xxxx
         digest[6] &= 0x5F;
         digest[6] |= 0x50;

         return uuid{ digest, digest + 16 };
      }

   private:
      uuid nsuuid;
      detail::sha1 hasher;
   };

#ifdef UUID_TIME_GENERATOR
   // !!! DO NOT USE THIS IN PRODUCTION
   // this implementation is unreliable for good uuids
   class uuid_time_generator
   {
      using mac_address = std::array<unsigned char, 6>;

      std::optional<mac_address> device_address;

      [[nodiscard]] bool get_mac_address()
      {         
         if (device_address.has_value())
         {
            return true;
         }
         
#ifdef _WIN32
         DWORD len = 0;
         auto ret = GetAdaptersInfo(nullptr, &len);
         if (ret != ERROR_BUFFER_OVERFLOW) return false;
         std::vector<unsigned char> buf(len);
         auto pips = reinterpret_cast<PIP_ADAPTER_INFO>(&buf.front());
         ret = GetAdaptersInfo(pips, &len);
         if (ret != ERROR_SUCCESS) return false;
         mac_address addr;
         std::copy(pips->Address, pips->Address + 6, std::begin(addr));
         device_address = addr;
#endif

         return device_address.has_value();
      }

      [[nodiscard]] long long get_time_intervals()
      {
         auto start = std::chrono::system_clock::from_time_t(time_t(-12219292800));
         auto diff = std::chrono::system_clock::now() - start;
         auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(diff).count();
         return ns / 100;
      }

      [[nodiscard]] static unsigned short get_clock_sequence()
      {
         static std::mt19937 clock_gen(std::random_device{}());
         static std::uniform_int_distribution<unsigned short> clock_dis;
         static std::atomic_ushort clock_sequence = clock_dis(clock_gen);
         return clock_sequence++;
      }

   public:
      [[nodiscard]] uuid operator()()
      {
         if (get_mac_address())
         {
            std::array<uuids::uuid::value_type, 16> data;

            auto tm = get_time_intervals();

            auto clock_seq = get_clock_sequence();

            auto ptm = reinterpret_cast<uuids::uuid::value_type*>(&tm);

            memcpy(&data[0], ptm + 4, 4);
            memcpy(&data[4], ptm + 2, 2);
            memcpy(&data[6], ptm, 2);

            memcpy(&data[8], &clock_seq, 2);

            // variant must be 0b10xxxxxx
            data[8] &= 0xBF;
            data[8] |= 0x80;

            // version must be 0b0001xxxx
            data[6] &= 0x1F;
            data[6] |= 0x10;

            memcpy(&data[10], &device_address.value()[0], 6);

            return uuids::uuid{std::cbegin(data), std::cend(data)};
         }

         return {};
      }
   };
#endif
}

namespace std
{
   template <>
   struct hash<uuids::uuid>
   {
      using argument_type = uuids::uuid;
      using result_type   = std::size_t;

      [[nodiscard]] result_type operator()(argument_type const &uuid) const
      {
#ifdef UUID_HASH_STRING_BASED
         std::hash<std::string> hasher;
         return static_cast<result_type>(hasher(uuids::to_string(uuid)));
#else
         uint64_t l =
            static_cast<uint64_t>(uuid.data[0]) << 56 |
            static_cast<uint64_t>(uuid.data[1]) << 48 |
            static_cast<uint64_t>(uuid.data[2]) << 40 |
            static_cast<uint64_t>(uuid.data[3]) << 32 |
            static_cast<uint64_t>(uuid.data[4]) << 24 |
            static_cast<uint64_t>(uuid.data[5]) << 16 |
            static_cast<uint64_t>(uuid.data[6]) <<  8 |
            static_cast<uint64_t>(uuid.data[7]);
         uint64_t h =
            static_cast<uint64_t>(uuid.data[8])  << 56 |
            static_cast<uint64_t>(uuid.data[9])  << 48 |
            static_cast<uint64_t>(uuid.data[10]) << 40 |
            static_cast<uint64_t>(uuid.data[11]) << 32 |
            static_cast<uint64_t>(uuid.data[12]) << 24 |
            static_cast<uint64_t>(uuid.data[13]) << 16 |
            static_cast<uint64_t>(uuid.data[14]) <<  8 |
            static_cast<uint64_t>(uuid.data[15]);

         if constexpr (sizeof(result_type) > 4)
         {
            return result_type(l ^ h);
         }
         else
         {
            uint64_t hash64 = l ^ h;
            return result_type(uint32_t(hash64 >> 32) ^ uint32_t(hash64));
         }
#endif
      }
   };

#ifdef STDUUID_USE_STD_FORMAT
   // Formatting a uuid means formatting its canonical textual representation,
   // so the specialization derives from the one for string views. That way the
   // fill, align and width specifiers are all supported, and behave as they do
   // for any other string, without this having to parse them itself.
   template <class CharT>
   struct formatter<uuids::uuid, CharT> : formatter<basic_string_view<CharT>, CharT>
   {
      template <class FormatContext>
      auto format(uuids::uuid const & id, FormatContext & ctx) const
      {
         CharT buffer[36];
         uuids::to_string<CharT>(id, buffer);

         return formatter<basic_string_view<CharT>, CharT>::format(
            basic_string_view<CharT>(buffer, 36), ctx);
      }
   };
#endif
}

// Internal implementation macros.  STDUUID_DISABLE_SIMD (and the existing
// UUID_* configuration macros) are intentionally left for the user.
#undef STDUUID_LIKELY
#undef STDUUID_IS_CONSTANT_EVALUATED
#undef STDUUID_SIMD_ENABLED
#undef STDUUID_ARCH_X86
#undef STDUUID_ARCH_NEON
#undef STDUUID_X86_RUNTIME_DISPATCH
#undef STDUUID_SSSE3_COMPILE_TIME
#undef STDUUID_SSSE3_CODE
#undef STDUUID_SHA1_CODE
#undef STDUUID_SSE2_CODE

#endif /* STDUUID_H */
