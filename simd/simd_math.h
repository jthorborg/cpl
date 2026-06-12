/*************************************************************************************

	cpl - cross-platform library - v. 0.1.0.

	Copyright (C) 2016 Janus Lynggaard Thorborg (www.jthorborg.com)

	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program.  If not, see <http://www.gnu.org/licenses/>.

	See \licenses\ for additional details on licenses associated with this program.

**************************************************************************************

	file:simdmath.h

		Standard math library routines like trigonometry and stuff.
		Most is derived from the old cephes library.


*************************************************************************************/

#ifndef CPL_SIMD_MATH_H
#define CPL_SIMD_MATH_H

#include "simd_traits.h"
#include "simd_consts.h"
#include "simd_interface.h"
#include "simd_cast.h"

namespace cpl
{
	namespace simd
	{
		/*///////////////////////////////////////////////////////////////////////////////////////////////////

			bool_and functions - enhances compability between code that works for both simd and scalar types.
			Eg:
				auto mask = x < y;
				result = x + bool_and(y, mask);

			translates to this in vector math:
				mask = [0...1];
				result = x + y & mask;
			and this in scalar:
				mask = true/false;
				result = x + y * mask;

		///////////////////////////////////////////////////////////////////////////////////////////////////*/
		template<typename V, typename M>
		CPL_SIMD_FUNC typename std::enable_if<!is_simd<V>::value, V>::type
			bool_and(V V1, M mask)
		{
			return V1 * mask;
		}

		template<typename V>
		CPL_SIMD_FUNC typename std::enable_if<is_simd<V>::value, V>::type
			bool_and(V V1, V mask)
		{
			return V1 & mask;
		}
		/*///////////////////////////////////////////////////////////////////////////////////////////////////

			Vector floating point square roots

		///////////////////////////////////////////////////////////////////////////////////////////////////*/
		CPL_SIMD_FUNC v4sd sqrt(v4sd x) { return _mm256_sqrt_pd(x); }
		CPL_SIMD_FUNC v4sf sqrt(v4sf x) { return _mm_sqrt_ps(x); }
		CPL_SIMD_FUNC v8sf sqrt(v8sf x) { return _mm256_sqrt_ps(x); }
		CPL_SIMD_FUNC v2sd sqrt(v2sd x) { return _mm_sqrt_pd(x); }



		/*///////////////////////////////////////////////////////////////////////////////////////////////////

			Vector floating point absolute values

		///////////////////////////////////////////////////////////////////////////////////////////////////*/
		CPL_SIMD_FUNC v4sf abs(v4sf val)
		{
			return _mm_and_ps(val, consts<v4sf>::sign_mask);
		}

		CPL_SIMD_FUNC v8sf abs(v8sf val)
		{
			return _mm256_and_ps(val, consts<v8sf>::sign_mask);
		}

		CPL_SIMD_FUNC v2sd abs(v2sd val)
		{
			return _mm_and_pd(val, consts<v2sd>::sign_mask);
		}

		CPL_SIMD_FUNC v4sd abs(v4sd val)
		{
			return _mm256_and_pd(val, consts<v4sd>::sign_mask);
		}

		CPL_SIMD_FUNC float abs(float val)
		{
			return std::abs(val);
		}

		CPL_SIMD_FUNC double abs(double val)
		{
			return std::abs(val);
		}

		/*///////////////////////////////////////////////////////////////////////////////////////////////////

			Vector floating point bit or

		///////////////////////////////////////////////////////////////////////////////////////////////////*/
		CPL_SIMD_FUNC v4sf vor(v4sf a, v4sf b) { return _mm_or_ps(a, b); }
		CPL_SIMD_FUNC v8sf vor(v8sf a, v8sf b) { return _mm256_or_ps(a, b); }
		CPL_SIMD_FUNC v4sd vor(v4sd a, v4sd b) { return _mm256_or_pd(a, b); }
		CPL_SIMD_FUNC v2sd vor(v2sd a, v2sd b) { return _mm_or_pd(a, b); }

		CPL_SIMD_FUNC float vor(float a, float b)
		{
			auto result = reinterpret_vector_cast<std::uint32_t>(a) | reinterpret_vector_cast<std::uint32_t>(b);
			return reinterpret_vector_cast<float>(result);
		}

		CPL_SIMD_FUNC double vor(double a, double b)
		{
			auto result = reinterpret_vector_cast<std::uint64_t>(a) | reinterpret_vector_cast<std::uint64_t>(b);
			return reinterpret_vector_cast<double>(result);
		}

		/*///////////////////////////////////////////////////////////////////////////////////////////////////

		 Floating point bit-ands

		 /////////////////////////////////////////////////////////////////////////////////////////////////*/
		CPL_SIMD_FUNC v4sf vand(v4sf a, v4sf b) { return _mm_and_ps(a, b); }
		CPL_SIMD_FUNC v8sf vand(v8sf a, v8sf b) { return _mm256_and_ps(a, b); }
		CPL_SIMD_FUNC v4sd vand(v4sd a, v4sd b) { return _mm256_and_pd(a, b); }
		CPL_SIMD_FUNC v2sd vand(v2sd a, v2sd b) { return _mm_and_pd(a, b); }

		CPL_SIMD_FUNC float vand(float a, float b)
		{
			auto result = reinterpret_vector_cast<std::uint32_t>(a) & reinterpret_vector_cast<std::uint32_t>(b);
			return reinterpret_vector_cast<float>(result);
		}

		CPL_SIMD_FUNC double vand(double a, double b)
		{
			auto result = reinterpret_vector_cast<std::uint64_t>(a) & reinterpret_vector_cast<std::uint64_t>(b);
			return reinterpret_vector_cast<double>(result);
		}

		/*///////////////////////////////////////////////////////////////////////////////////////////////////

			Vector floating point bit exclusive-or

		///////////////////////////////////////////////////////////////////////////////////////////////////*/
		CPL_SIMD_FUNC v4sf vxor(v4sf a, v4sf b) { return _mm_xor_ps(a, b); }
		CPL_SIMD_FUNC v8sf vxor(v8sf a, v8sf b) { return _mm256_xor_ps(a, b); }
		CPL_SIMD_FUNC v4sd vxor(v4sd a, v4sd b) { return _mm256_xor_pd(a, b); }
		CPL_SIMD_FUNC v2sd vxor(v2sd a, v2sd b) { return _mm_xor_pd(a, b); }

		CPL_SIMD_FUNC float vxor(float a, float b)
		{
			auto result = reinterpret_vector_cast<std::uint32_t>(a) ^ reinterpret_vector_cast<std::uint32_t>(b);
			return reinterpret_vector_cast<float>(result);
		}

		CPL_SIMD_FUNC double vxor(double a, double b)
		{
			auto result = reinterpret_vector_cast<std::uint64_t>(a) ^ reinterpret_vector_cast<std::uint64_t>(b);
			return reinterpret_vector_cast<double>(result);
		}

		/*///////////////////////////////////////////////////////////////////////////////////////////////////

			Vector integer bit-ands

		/////////////////////////////////////////////////////////////////////////////////////////////////*/

		CPL_SIMD_FUNC v128i vand(v128i ia, v128i ib)
		{
			return _mm_and_si128(ia, ib);
		}

		CPL_SIMD_FUNC v256i vand(v256i ia, v256i ib)
		{
			return _mm256_and_si256(ia, ib);
		}

		// does a integer and using floating point lines; usable for non-avx512 modes
		template<typename V>
		CPL_SIMD_FUNC typename cpl::simd::to_integer<V>::type
			vfloat_and(typename cpl::simd::to_integer<V>::type a, typename cpl::simd::to_integer<V>::type b)
		{
			return reinterpret_vector_cast<typename cpl::simd::to_integer<V>::type>
				(
					vand(reinterpret_vector_cast<V>(a), reinterpret_vector_cast<V>(b))
					);
		}

		/*///////////////////////////////////////////////////////////////////////////////////////////////////

			Vector integer bit-ands

		/////////////////////////////////////////////////////////////////////////////////////////////////*/

		CPL_SIMD_FUNC v128i vandnot(v128i ia, v128i ib)
		{
			return _mm_andnot_si128(ia, ib);
		}

		CPL_SIMD_FUNC v256i vandnot(v256i ia, v256i ib)
		{
			return _mm256_andnot_si256(ia, ib);
		}


		/*///////////////////////////////////////////////////////////////////////////////////////////////////

			Vector floating point bit not operations, emulated through andnot intrinsic

		/////////////////////////////////////////////////////////////////////////////////////////////////*/
		CPL_SIMD_FUNC v4sf vnot(v4sf a)
		{
			return _mm_andnot_ps(a, consts<v4sf>::all_bits);
		}
		CPL_SIMD_FUNC v8sf vnot(v8sf a)
		{
			return _mm256_andnot_ps(a, consts<v8sf>::all_bits);
		}
		CPL_SIMD_FUNC v4sd vnot(v4sd a)
		{
			return _mm256_andnot_pd(a, consts<v4sd>::all_bits);
		}
		CPL_SIMD_FUNC v2sd vnot(v2sd a)
		{
			return _mm_andnot_pd(a, consts<v2sd>::all_bits);
		}

		CPL_SIMD_FUNC float vnot(float a)
		{
			auto result = ~reinterpret_vector_cast<std::uint32_t>(a);
			return reinterpret_vector_cast<float>(result);
		}

		CPL_SIMD_FUNC double vnot(double a)
		{
			auto result = ~reinterpret_vector_cast<std::uint64_t>(a);
			return reinterpret_vector_cast<double>(result);
		}

		/*///////////////////////////////////////////////////////////////////////////////////////////////////

			Vector floating point bit-andnots
			ret = ~a & b

		/////////////////////////////////////////////////////////////////////////////////////////////////*/
		CPL_SIMD_FUNC v4sf vandnot(v4sf a, v4sf b) { return _mm_andnot_ps(a, b); }
		CPL_SIMD_FUNC v8sf vandnot(v8sf a, v8sf b) { return _mm256_andnot_ps(a, b); }
		CPL_SIMD_FUNC v4sd vandnot(v4sd a, v4sd b) { return _mm256_andnot_pd(a, b); }
		CPL_SIMD_FUNC v2sd vandnot(v2sd a, v2sd b) { return _mm_andnot_pd(a, b); }

		template<typename V>
		CPL_SIMD_FUNC V vandnot(V a, V b)
		{
			return vand(a, vnot(b));
		}

		/*///////////////////////////////////////////////////////////////////////////////////////////////////

			Select elements from two vectors depending on a third.
				output[n] = mask[n] ? a[n] : b[n]
					(return (a & mask) | (b & ~mask))

				note that mask should strictly be either all 0 or 1 bits (like, from comparisons)
		///////////////////////////////////////////////////////////////////////////////////////////////////*/
		template<typename V>
		CPL_SIMD_FUNC V vselect(V a, V b, V mask)
		{
			return vor(vand(mask, a), vandnot(mask, b));
		}

		/*///////////////////////////////////////////////////////////////////////////////////////////////////

			Vector max

		 ///////////////////////////////////////////////////////////////////////////////////////////////////*/

		template<typename V>
		CPL_SIMD_FUNC V max(V a, V b)
		{
			auto mask = (V)(a > b);
			return vor(vand(a, mask), vandnot(mask, b));
		}

		template<>
		CPL_SIMD_FUNC float max(float a, float b)
		{
			return std::max(a, b);
		}

		template<>
		CPL_SIMD_FUNC double max(double a, double b)
		{
			return std::max(a, b);
		}

		/*///////////////////////////////////////////////////////////////////////////////////////////////////

			Vector floating point sign extraction (only the MSB is set)

		/////////////////////////////////////////////////////////////////////////////////////////////////*/

		CPL_SIMD_FUNC v4sf sign(v4sf val)
		{
			return _mm_and_ps(val, consts<v4sf>::sign_bit);
		}

		CPL_SIMD_FUNC v8sf sign(v8sf val)
		{
			return _mm256_and_ps(val, consts<v8sf>::sign_bit);
		}

		CPL_SIMD_FUNC v2sd sign(v2sd val)
		{
			return _mm_and_pd(val, consts<v2sd>::sign_bit);
		}

		CPL_SIMD_FUNC v4sd sign(v4sd val)
		{
			return _mm256_and_pd(val, consts<v4sd>::sign_bit);
		}

		CPL_SIMD_FUNC float sign(float val)
		{
			return (float)std::copysign(1.0, val);
		}

		CPL_SIMD_FUNC double sign(double val)
		{
			return (double)std::copysign(1.0, val);
		}

		/*///////////////////////////////////////////////////////////////////////////////////////////////////

				Vector integer additions

		/////////////////////////////////////////////////////////////////////////////////////////////////*/

		template<std::size_t elements, bool is_signed = true, typename V>
		CPL_SIMD_FUNC typename std::enable_if<4 == elements && is_signed && std::is_same<V, v128i>::value, V>::type
			viadd(V ia, V ib)
		{
			return _mm_add_epi32(ia, ib);
		}

		template<std::size_t elements, bool is_signed = true, typename V>
		CPL_SIMD_FUNC typename std::enable_if<2 == elements && is_signed && std::is_same<V, v128i>::value, V>::type
			viadd(V ia, V ib)
		{
			return _mm_add_epi64(ia, ib);
		}

		template<std::size_t elements, bool is_signed = true, typename V>
		CPL_SIMD_FUNC typename std::enable_if<8 == elements && is_signed && std::is_same<V, v256i>::value, V>::type
			viadd(V ia, V ib)
		{
			return _mm256_add_epi32(ia, ib);
		}

		template<std::size_t elements, bool is_signed = true, typename V>
		CPL_SIMD_FUNC typename std::enable_if<4 == elements && is_signed && std::is_same<V, v256i>::value, V>::type
			viadd(V ia, V ib)
		{
			return _mm256_add_epi64(ia, ib);
		}

		/*///////////////////////////////////////////////////////////////////////////////////////////////////

			Vector integer subtraction

		/////////////////////////////////////////////////////////////////////////////////////////////////*/

		template<std::size_t elements, bool is_signed = true, typename V>
		CPL_SIMD_FUNC typename std::enable_if<4 == elements && is_signed && std::is_same<V, v128i>::value, V>::type
			visub(V ia, V ib)
		{
			return _mm_sub_epi32(ia, ib);
		}

		template<std::size_t elements, bool is_signed = true, typename V>
		CPL_SIMD_FUNC typename std::enable_if<2 == elements && is_signed && std::is_same<V, v128i>::value, V>::type
			visub(V ia, V ib)
		{
			return _mm_sub_epi64(ia, ib);
		}

		template<std::size_t elements, bool is_signed = true, typename V>
		CPL_SIMD_FUNC typename std::enable_if<8 == elements && is_signed && std::is_same<V, v256i>::value, V>::type
			visub(V ia, V ib)
		{
			return _mm256_sub_epi32(ia, ib);
		}

		template<std::size_t elements, bool is_signed = true, typename V>
		CPL_SIMD_FUNC typename std::enable_if<4 == elements && is_signed && std::is_same<V, v256i>::value, V>::type
			visub(V ia, V ib)
		{
			return _mm256_sub_epi64(ia, ib);
		}

		/*///////////////////////////////////////////////////////////////////////////////////////////////////

				Vector integer left shifts

		/////////////////////////////////////////////////////////////////////////////////////////////////*/

		template<std::size_t elements, std::size_t shift_amount, bool is_signed = true, typename V>
		CPL_SIMD_FUNC typename std::enable_if<8 == elements && is_signed && std::is_same<V, v256i>::value, V>::type
			vileft_shift(V ia)
		{
			return _mm256_slli_epi32(ia, shift_amount);
		}


		template<std::size_t elements, std::size_t shift_amount, bool is_signed = true, typename V>
		CPL_SIMD_FUNC typename std::enable_if<4 == elements && is_signed && std::is_same<V, v256i>::value, V>::type
			vileft_shift(V ia)
		{
			return _mm256_slli_epi64(ia, shift_amount);
		}


		template<std::size_t elements, std::size_t shift_amount, bool is_signed = true, typename V>
		CPL_SIMD_FUNC typename std::enable_if<4 == elements && is_signed && std::is_same<V, v128i>::value, V>::type
			vileft_shift(V ia)
		{
			return _mm_slli_epi32(ia, shift_amount);
		}

		template<std::size_t elements, std::size_t shift_amount, bool is_signed = true, typename V>
		CPL_SIMD_FUNC typename std::enable_if<2 == elements && is_signed && std::is_same<V, v128i>::value, V>::type
			vileft_shift(V ia)
		{
			return _mm_slli_epi64(ia, shift_amount);
		}

		/*///////////////////////////////////////////////////////////////////////////////////////////////////

				Trigonometry

		/////////////////////////////////////////////////////////////////////////////////////////////////*/

		template<typename V>
		CPL_SIMD_FUNC V atan(V x)
		{
			using c = cpl::simd::consts<V>;
			V y, z, z1, z2;
			V a1, a2, a3;
			/* make argument positive and save the sign */
			V sign = simd::sign(x);
			x = vxor(x, sign);

			/* range reduction */
			a1 = x > c::cephes_2414;
			a2 = x > c::cephes_0414;
			a3 = vnot(a2);
			a2 = vxor(a2, a1);

			z1 = c::minus_one / (x + c::cephes_small);
			z2 = (x - c::one) / (x + c::one);
			z1 = vand(z1, a1);
			z2 = vand(z2, a2);
			x = vand(x, a3);
			x = vor(x, z1);
			x = vor(x, z2);

			y = c::pi_half;
			z1 = c::pi_quarter;
			y = vand(y, a1);
			z1 = vand(z1, a2);
			y = vor(y, z1);

			z = x * x;
			y +=
				(((c::cephes_8053 * z
					- c::cephes_1387) * z
					+ c::cephes_1997) * z
					- c::cephes_3333) * z * x
				+ x;


			return vxor(y, sign);
		}


		template<typename V>
		static CPL_SIMD_FUNC V atan2(V y, V x)
		{
			using c = cpl::simd::consts<V>;
			V z, w;
			V x_neg_PI = c::pi;

			V cf4_0 = zero<V>();
			V cf4_1 = c::one;
			x_neg_PI = vand(x_neg_PI, cf4_0 > x);
			V y_negativ_2 = c::two;
			y_negativ_2 = vand(y_negativ_2, cf4_0 > y);

			V i_x_zero = cf4_0 == x;
			V i_y_zero = cf4_0 == y;
			V x_zero_PIO2 = c::pi_half;
			x_zero_PIO2 = vand(x_zero_PIO2, i_x_zero);
			V y_zero = cf4_1;
			y_zero = vand(y_zero, i_y_zero);


			w = x_neg_PI * (cf4_1 - y_negativ_2);

			z = atan(y / (x + x_zero_PIO2));
			z = vand(z, vnot(vor(i_x_zero, i_y_zero)));

			return w + z + x_zero_PIO2 * (cf4_1 - y_zero - y_negativ_2)
				+ y_zero *  x_neg_PI;
		}



		CPL_SIMD_FUNC void sincos(float x, float * s, float * c)
		{
			*s = std::sin(x);
			*c = std::cos(x);
		}
		CPL_SIMD_FUNC void sincos(double x, double * s, double * c)
		{
			*s = std::sin(x);
			*c = std::cos(x);
		}
		#ifdef _CPL_HAS_AVX_512
		template<typename V>
		V sin(V x)
		{ // any x
			V xmm2 = zero<V>(), xmm3, y;
			typedef scalar_of<V>::type Ty;
			using c = cpl::simd::consts<V>;

			auto const elements = elements_of<V>::value;

			typedef to_integer<V>::type VInt;
			VInt emm0, emm2;

			/* take the absolute value and extract the sign bit */
			V sign_bit = simd::sign(x);
			x = vxor(x, sign_bit);

			/* scale by 4/Pi */
			y = x * c::four_over_pi;

			/* store the integer part of y in mm0 */
			emm2 = static_vector_cast<VInt>(y);
			/* j=(j+1) & (~1) (see the cephes sources) */
			emm2 = viadd<elements>(emm2, set1<elements, VInt>(1));
			emm2 = vand(emm2, set1<elements, VInt>(-2));
			y = static_vector_cast<V>(emm2);

			/* get the swap sign flag */
			emm0 = vand(emm2, set1<elements, VInt>(4));
			emm0 = vileft_shift<elements, 29>(emm0);
			/* get the polynom selection mask
			there is one polynom for 0 <= x <= Pi/4
			and another one for Pi/4<x<=Pi/2

			Both branches will be computed.
			*/
			emm2 = vand(emm2, set1<elements, VInt>(2));
			emm2 = viequals<elements>(emm2, zero<VInt>());

			V swap_sign_bit = reinterpret_vector_cast<V>(emm0);
			V poly_mask = reinterpret_vector_cast<V>(emm2);
			sign_bit = vxor(sign_bit, swap_sign_bit);


			/* The magic pass: "Extended precision modular arithmetic"
			x = ((x - y * DP1) - y * DP2) - y * DP3;
			*/
			x += y * c::cephes_mdp1;
			x += y * c::cephes_mdp2;
			x += y * c::cephes_mdp3;

			/* Evaluate the first polynom  (0 <= x <= Pi/4) */
			y = c::cephes_cos_p0;
			auto z = x * x;

			y *= z;
			y += c::cephes_cos_p1;
			y *= z;
			y += c::cephes_cos_p2;
			y *= z;
			y *= z;
			y -= z * c::half;
			y += c::one;


			/* Evaluate the second polynom  (Pi/4 <= x <= 0) */

			auto y2 = c::cephes_sin_p0;
			y2 *= z;
			y2 += c::cephes_sin_p1;
			y2 *= z;
			y2 += c::cephes_sin_p2;
			y2 *= z;
			y2 *= x;
			y2 += x;

			/* select the correct result from the two polynoms */
			xmm3 = poly_mask;
			y2 = vand(xmm3, y2); //, xmm3);
			y = vandnot(xmm3, y);
			y += y2;
			/* update the sign */
			y = vxor(y, sign_bit);
			return y;
		}

		template<typename V>
		CPL_SIMD_FUNC void sincos(V x, V * s, V * c)
		{

			//typedef v4sf V;
			typedef scalar_of<V>::type Ty;
			using consts = cpl::simd::consts<V>;

			auto const elements = elements_of<V>::value;

			typedef to_integer<V>::type VInt;


			V xmm1, xmm2, xmm3 = zero<V>(), sign_bit_sin, y;
			V poly_mask, sign_bit_cos, swap_sign_bit_sin;

			sign_bit_sin = sign(x);
			/* take the absolute value */
			x = vxor(x, sign_bit_sin);


			/* scale by 4/Pi */
			y = x * consts::four_over_pi;

			// -------- integer part of routine; range reduction  ---------
			// this is also the reference solution; code inside will work for any architechture

			VInt emm0, emm2, emm4;

			/* store the integer part of y in mm0 */
			emm2 = static_vector_cast<VInt>(y + consts::one);
			/* j=(j+1) & (~1) (see the cephes sources)
			add one and make it even
			*/
			//emm2 = viadd<elements>(emm2, set1<elements, VInt>(1));
			emm2 = vfloat_and<V>(emm2, set1<elements, VInt>(~1));
			y = static_vector_cast<V>(emm2);
			emm4 = emm2;

			/* get the swap sign flag
			swap_sign_sin = input & 4 (swap sign each M_PI multiple)
			*/
			emm0 = vfloat_and<V>(emm2, set1<elements, VInt>(4));
			auto comp = reinterpret_vector_cast<V>(emm0) == reinterpret_vector_cast<V>(set1<elements, VInt>(4));
			emm0 = vileft_shift<elements, 29>(emm0);
			swap_sign_bit_sin = vxor(reinterpret_vector_cast<V>(emm0), consts::sign_bit);

			/* get the polynom selection mask for the sine
			poly_mask = input & 2 (swap polynom each M_PI / 2)
			*/
			emm2 = vfloat_and<V>(emm2, set1<elements, VInt>(2));
			emm2 = viequals<elements>(emm2, zero<VInt>());
			poly_mask = reinterpret_vector_cast<V>(emm2);

			/* get the swap sign flag
			swap_sign_cos = (input - 2) & 4 (swap sign each -M_PI multiple)
			*/
			emm4 = visub<elements>(emm4, set1<elements, VInt>(2));
			emm4 = vandnot(emm4, set1<elements, VInt>(4));
			emm4 = vileft_shift<elements, 29>(emm4);
			// save signs
			sign_bit_cos = reinterpret_vector_cast<V>(emm4);
			sign_bit_sin = vxor(sign_bit_sin, swap_sign_bit_sin);




			// -------- back to floating point ---------------
			/* The magic pass: "Extended precision modular arithmetic"
			x = ((x - y * DP1) - y * DP2) - y * DP3; */
			x += y * consts::cephes_mdp1;
			x += y * consts::cephes_mdp2;
			x += y * consts::cephes_mdp3;

			/* Evaluate the first polynom  (0 <= x <= Pi/4) */
			y = consts::cephes_cos_p0;
			auto z = x * x;

			y *= z;
			y += consts::cephes_cos_p1;
			y *= z;
			y += consts::cephes_cos_p2;
			y *= z;
			y *= z;
			y -= z * consts::half;
			y += consts::one;

			/* Evaluate the second polynom  (Pi/4 <= x <= 0) */

			auto y2 = consts::cephes_sin_p0;
			y2 *= z;
			y2 += consts::cephes_sin_p1;
			y2 *= z;
			y2 += consts::cephes_sin_p2;
			y2 *= z;
			y2 *= x;
			y2 += x;

			/* select the correct result from the two polynoms */
			xmm3 = poly_mask;
			V ysin2 = vand(xmm3, y2);
			V ysin1 = vandnot(xmm3, y);
			y2 -= ysin2;
			y -= ysin1;

			xmm1 = ysin1 + ysin2;
			xmm2 = y + y2;

			/* update the sign */
			*s = vxor(xmm1, sign_bit_sin);
			*c = vxor(xmm2, sign_bit_cos);
		}
		#else

		template<typename V>
		CPL_SIMD_FUNC typename std::enable_if<std::is_same<typename scalar_of<V>::type, float>::value, V>::type
			sin(V x)
		{ // any x

			auto const elements = elements_of<V>::value;
			typedef typename to_integer<V>::type VInt;

			using VConsts = cpl::simd::consts<V>;
			using VIConsts = cpl::simd::consts<VInt>;

			V  y;

			/* take the absolute value and extract the sign bit */
			V sign_bit = simd::sign(x);
			x = vxor(x, sign_bit);

			/* scale by 4/Pi */
			y = x * VConsts::four_over_pi;

			// -------- integer part of routine; range reduction and sign/selection masks ---------
			// this is also the reference solution; code inside will work for any architechture
			const V four_as_int = reinterpret_vector_cast<V>(set1<elements, VInt>(4));
			const V two_as_int = reinterpret_vector_cast<V>(set1<elements, VInt>(2));

			// store the integer part of y in mm0
			VInt j = static_vector_cast<VInt>(y + VConsts::one);
			// j=(j+1) & (~1) (see the cephes sources)
			//	add one and make it even

			//j = viadd<elements>(j, set1<elements, VInt>(1));
			j = vfloat_and<V>(j, set1<elements, VInt>(~1));
			y = static_vector_cast<V>(j);
			const V j_as_float = reinterpret_vector_cast<V>(j);

			// get the swap sign flag
			//	swap_sign_sin = input & 4 (swap sign each M_PI multiple)
			//
			V has_fourth_bit = vand(j_as_float, four_as_int) == four_as_int;
			V swap_sign_bit = vand(has_fourth_bit, VConsts::sign_bit);

			/* get the polynom selection mask
			there is one polynom for 0 <= x <= Pi/4
			and another one for Pi/4<x<=Pi/2

			Both branches will be computed.
			*/
			V poly_mask = vand(j_as_float, two_as_int) == VConsts::zero;

			sign_bit = vxor(sign_bit, swap_sign_bit);


			/* The magic pass: "Extended precision modular arithmetic"
			x = ((x - y * DP1) - y * DP2) - y * DP3;
			*/
			x += y * VConsts::cephes_mdp1;
			x += y * VConsts::cephes_mdp2;
			x += y * VConsts::cephes_mdp3;

			/* Evaluate the first polynom  (0 <= x <= Pi/4) */
			y = VConsts::cephes_cos_p0;
			auto z = x * x;

			y *= z;
			y += VConsts::cephes_cos_p1;
			y *= z;
			y += VConsts::cephes_cos_p2;
			y *= z;
			y *= z;
			y -= z * VConsts::half;
			y += VConsts::one;


			/* Evaluate the second polynom  (Pi/4 <= x <= 0) */

			auto y2 = VConsts::cephes_sin_p0;
			y2 *= z;
			y2 += VConsts::cephes_sin_p1;
			y2 *= z;
			y2 += VConsts::cephes_sin_p2;
			y2 *= z;
			y2 *= x;
			y2 += x;

			/* select the correct result from the two polynoms */
			y2 = vand(poly_mask, y2); //, xmm3);
			y = vandnot(poly_mask, y);
			y += y2;
			/* update the sign */
			y = vxor(y, sign_bit);
			return y;
		}


		template<typename V>
		CPL_SIMD_FUNC typename std::enable_if<std::is_same<typename scalar_of<V>::type, double>::value, V>::type
			sin(V x)
		{ // any x
			#pragma message cwarn("Seems to return cosines. Output of 4.71 == ~0")
			V y;
			typedef v4sf VFloat;
			using VConsts = cpl::simd::consts<V>;

			auto const elements = elements_of<V>::value;
			auto const int_elements = elements_of<VFloat>::value;
			typedef v128i VInt;



			/* take the absolute value and extract the sign bit */
			V sign_bit = simd::sign(x);
			x = vxor(x, sign_bit);

			/* scale by 4/Pi */
			y = x * VConsts::four_over_pi;

			// -------- integer part of routine; range reduction and sign/selection masks ---------
			// this is also the reference solution; code inside will work for any architechture
			const VFloat four_as_int = reinterpret_vector_cast<VFloat>(set1<int_elements, VInt>(4));
			const VFloat two_as_int = reinterpret_vector_cast<VFloat>(set1<int_elements, VInt>(2));

			// store the integer part of y in mm0
			VInt j = vdouble_cvt_int32(y + VConsts::one);

#ifdef CPL_M_X86
			// jump out of avx here.
			if (std::is_same<V, v4sd>::value) {
				_mm256_zeroupper();
			}
#endif

			// j=(j+1) & (~1) (see the cephes sources)
			//	add one and make it even
			j = vfloat_and<VFloat>(j, set1<int_elements, VInt>(~1));
			y = vint32_cvt_double<V>(j);
			const VFloat j_as_float = reinterpret_vector_cast<VFloat>(j);

			// get the swap sign flag
			//	swap_sign_sin = input & 4 (swap sign each M_PI multiple)
			//
			auto has_fourth_bit = vand(j_as_float, four_as_int) == four_as_int;
			V swap_sign_bit = vfloat_reinterpret_double<V>(
				vand(vfloat_reinterpret_double<V>(has_fourth_bit), consts<VFloat>::sign_bit)
				);

			/* get the polynom selection mask
			there is one polynom for 0 <= x <= Pi/4
			and another one for Pi/4<x<=Pi/2

			Both branches will be computed.
			*/
			V poly_mask = vfloat_reinterpret_double<V>(vand(j_as_float, two_as_int) == consts<VFloat>::zero);

			sign_bit = vxor(sign_bit, swap_sign_bit);


			/* The magic pass: "Extended precision modular arithmetic"
			x = ((x - y * DP1) - y * DP2) - y * DP3;
			*/
			x += y * VConsts::cephes_mdp1;
			x += y * VConsts::cephes_mdp2;
			x += y * VConsts::cephes_mdp3;

			/* Evaluate the first polynom  (0 <= x <= Pi/4) */
			y = VConsts::cephes_cos_p0;
			auto z = x * x;

			y *= z;
			y += VConsts::cephes_cos_p1;
			y *= z;
			y += VConsts::cephes_cos_p2;
			y *= z;
			y *= z;
			y -= z * VConsts::half;
			y += VConsts::one;


			/* Evaluate the second polynom  (Pi/4 <= x <= 0) */

			auto y2 = VConsts::cephes_sin_p0;
			y2 *= z;
			y2 += VConsts::cephes_sin_p1;
			y2 *= z;
			y2 += VConsts::cephes_sin_p2;
			y2 *= z;
			y2 *= x;
			y2 += x;

			/* select the correct result from the two polynoms */

			y2 = vand(poly_mask, y2); //, xmm3);
			y = vandnot(poly_mask, y);
			y += y2;
			/* update the sign */
			y = vxor(y, sign_bit);
			return y;
		}

		template<typename V>
		CPL_SIMD_FUNC typename std::enable_if<simd::is_simd<V>::value, V>::type
			cos(V x)
		{
			/*V s, c;
			sincos(x, &s, &c);
			return c;*/

			return sin(x + consts<V>::pi_half);
		}

		template<typename V>
		CPL_SIMD_FUNC typename std::enable_if<std::is_same<typename scalar_of<V>::type, float>::value>::type
			sincos(V x, V * s, V * c)
		{

			using consts = cpl::simd::consts<V>;

			auto const elements = elements_of<V>::value;

			typedef typename to_integer<V>::type VInt;


			V sign_bit_sin, y;
			V poly_mask, sign_bit_cos, swap_sign_bit_sin;

			sign_bit_sin = sign(x);
			// take the absolute value
			x = vxor(x, sign_bit_sin);


			// scale by 4/Pi
			y = x * consts::four_over_pi;


			//static_assert(!std::is_same<Ty, double>::value, "Doubles not implemented yet");


			// -------- integer part of routine; range reduction and sign/selection masks ---------
			// this is also the reference solution; code inside will work for any architechture
			const V four_as_int = reinterpret_vector_cast<V>(set1<elements, VInt>(4));
			const V two_as_int = reinterpret_vector_cast<V>(set1<elements, VInt>(2));

			// store the integer part of y in mm0
			VInt j = static_vector_cast<VInt>(y + consts::one);
			// j=(j+1) & (~1) (see the cephes sources)
			//	add one and make it even

			//j = viadd<elements>(j, set1<elements, VInt>(1));
			j = vfloat_and<V>(j, set1<elements, VInt>(~1));
			y = static_vector_cast<V>(j);
			const V j_as_float = reinterpret_vector_cast<V>(j);

			// get the swap sign flag
			//	swap_sign_sin = input & 4 (swap sign each M_PI multiple)
			//
			auto has_fourth_bit = vand(j_as_float, four_as_int) == four_as_int;
			swap_sign_bit_sin = vand(reinterpret_vector_cast<V>(has_fourth_bit), consts::sign_bit);

			// get the polynom selection mask for the sine
			//	poly_mask = input & 2 (swap polynom each M_PI / 2)
			//
			poly_mask = vand(j_as_float, two_as_int) == consts::zero;


			// get the swap sign flag
			//	swap_sign_cos = (input - 2) & 4 (swap sign each -M_PI multiple)
			//
			auto const y_minus_two = static_vector_cast<VInt>(y - consts::two); // use y before it is rounded down??
			has_fourth_bit = vandnot(reinterpret_vector_cast<V>(y_minus_two), four_as_int) == four_as_int;
			sign_bit_cos = vand(reinterpret_vector_cast<V>(has_fourth_bit), consts::sign_bit);
			// save signs
			sign_bit_sin = vxor(sign_bit_sin, swap_sign_bit_sin);




			// -------- back to floating point ---------------
			/* The magic pass: "Extended precision modular arithmetic"
			x = ((x - y * DP1) - y * DP2) - y * DP3; */
			x += y * consts::cephes_mdp1;
			x += y * consts::cephes_mdp2;
			x += y * consts::cephes_mdp3;

			/* Evaluate the first polynom  (0 <= x <= Pi/4) */
			y = consts::cephes_cos_p0;
			auto z = x * x;

			y *= z;
			y += consts::cephes_cos_p1;
			y *= z;
			y += consts::cephes_cos_p2;
			y *= z;
			y *= z;
			y -= z * consts::half;
			y += consts::one;

			/* Evaluate the second polynom  (Pi/4 <= x <= 0) */

			auto y2 = consts::cephes_sin_p0;
			y2 *= z;
			y2 += consts::cephes_sin_p1;
			y2 *= z;
			y2 += consts::cephes_sin_p2;
			y2 *= z;
			y2 *= x;
			y2 += x;

			/* select the correct result from the two polynoms */
			V ysin2 = vand(poly_mask, y2);
			V ysin1 = vandnot(poly_mask, y);
			y2 -= ysin2;
			y -= ysin1;

			/* calculate final values and update the sign */
			*s = vxor(ysin1 + ysin2, sign_bit_sin);
			*c = vxor(y + y2, sign_bit_cos);
		}




		// note that when compiled for AVX (and not AVX512)
		// this function is ~20 times slower using v4sd,
		// due to mixed sse/avx modes.
		template<typename V>
		CPL_SIMD_FUNC typename std::enable_if<std::is_same<typename scalar_of<V>::type, double>::value>::type
			sincos(V x, V * s, V * c)
		{

			typedef v4sf VFloat;
			using VConsts = cpl::simd::consts<V>;

			auto const elements = elements_of<V>::value;

			typedef v128i VInt;


			V sign_bit_sin, y;
			V poly_mask, sign_bit_cos, swap_sign_bit_sin;

			sign_bit_sin = sign(x);
			// take the absolute value
			x = vxor(x, sign_bit_sin);


			// scale by 4/Pi
			y = x * VConsts::four_over_pi;


			// -------- integer part of routine; range reduction and sign/selection masks ---------
			const VFloat four_as_int = reinterpret_vector_cast<VFloat>(set1<elements_of<VFloat>::value, VInt>(4));
			const VFloat two_as_int = reinterpret_vector_cast<VFloat>(set1<elements_of<VFloat>::value, VInt>(2));

			// store the integer part of y in mm0
			VInt j = vdouble_cvt_int32(y + VConsts::one);

#ifdef CPL_M_X86
			// jump out of avx here.
			if (std::is_same<V, v4sd>::value) {
				_mm256_zeroupper();
			}
#endif

			// j=(j+1) & (~1) (see the cephes sources)
			//	add one and make it even

			j = vfloat_and<VFloat>(j, set1<elements_of<VFloat>::value, VInt>(~1));
			y = vint32_cvt_double<V>(j);
			const VFloat j_as_float = reinterpret_vector_cast<VFloat>(j);

			// get the swap sign flag
			//	swap_sign_sin = input & 4 (swap sign each M_PI multiple)
			//
			auto has_fourth_bit = vand(j_as_float, four_as_int) == four_as_int;
			swap_sign_bit_sin = vfloat_reinterpret_double<V>(
				vand(vfloat_reinterpret_double<V>(has_fourth_bit), consts<VFloat>::sign_bit)
				);
			// get the polynom selection mask for the sine
			//	poly_mask = input & 2 (swap polynom each M_PI / 2)
			//
			poly_mask = vfloat_reinterpret_double<V>(vand(j_as_float, two_as_int) == consts<VFloat>::zero);
			// get the swap sign flag
			//	swap_sign_cos = (input - 2) & 4 (swap sign each -M_PI multiple)
			//
			auto const y_minus_two = vdouble_cvt_int32(y - VConsts::two); // use y before it is rounded down??
			has_fourth_bit = vandnot(reinterpret_vector_cast<VFloat>(y_minus_two), four_as_int) == four_as_int;
			sign_bit_cos = vfloat_reinterpret_double<V>(
				vand(vfloat_reinterpret_double<V>(has_fourth_bit), consts<VFloat>::sign_bit)
				);
			// save signs
			sign_bit_sin = vxor(sign_bit_sin, swap_sign_bit_sin);


			// -------- back to floating point ---------------
			/* The magic pass: "Extended precision modular arithmetic"
			x = ((x - y * DP1) - y * DP2) - y * DP3; */
			x += y * VConsts::cephes_mdp1;
			x += y * VConsts::cephes_mdp2;
			x += y * VConsts::cephes_mdp3;

			/* Evaluate the first polynom  (0 <= x <= Pi/4) */
			y = VConsts::cephes_cos_p0;
			auto z = x * x;

			y *= z;
			y += VConsts::cephes_cos_p1;
			y *= z;
			y += VConsts::cephes_cos_p2;
			y *= z;
			y *= z;
			y -= z * VConsts::half;
			y += VConsts::one;

			/* Evaluate the second polynom  (Pi/4 <= x <= 0) */

			auto y2 = VConsts::cephes_sin_p0;
			y2 *= z;
			y2 += VConsts::cephes_sin_p1;
			y2 *= z;
			y2 += VConsts::cephes_sin_p2;
			y2 *= z;
			y2 *= x;
			y2 += x;

			/* select the correct result from the two polynoms */
			V ysin2 = vand(poly_mask, y2);
			V ysin1 = vandnot(poly_mask, y);
			y2 -= ysin2;
			y -= ysin1;

			/* calculate final values and update the sign */
			*s = vxor(ysin1 + ysin2, sign_bit_sin);
			*c = vxor(y + y2, sign_bit_cos);
		}

		#endif
	}; // simd
}; // cpl
#endif
