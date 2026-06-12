/*************************************************************************************

	cpl - cross-platform library - v. 0.1.0.

	Copyright (C) 2026 Janus Lynggaard Thorborg (www.jthorborg.com)

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

	file:simd_consts.h

		Declarations of common simd constants in constant memory.


*************************************************************************************/


#ifndef CPL_SIMD_CONSTS_H
#define CPL_SIMD_CONSTS_H
#include "../Mathext.h"
#include <float.h>
#include "simd_traits.h"

namespace cpl
{
	namespace simd
	{

		template<typename V>
		struct consts
		{
			typedef typename scalar_of<V>::type VTy;

			// standard constants
			static const V pi;
			static const V e;
			static const V tau;
			static const V pi_half;
			static const V pi_quarter;
			static const V pi_squared;
			static const V four_over_pi;
			static const V one;
			static const V minus_one;
			static const V minus_two;
			static const V half;
			static const V quarter;
			static const V two;
			static const V four;
			static const V sqrt_two;
			static const V sqrt_half_two;
			static const V sqrt_half_two_minus;
			static const V sign_bit;
			static const V sign_mask;
			static const V epsilon;
			static const V max;
			static const V min;
			static const V zero;
			static const V all_bits;

			// cephes specific magic constants
			static const V cephes_e__4;
			static const V cephes_small;
			static const V cephes_2414;
			static const V cephes_0414;
			static const V cephes_8053;
			static const V cephes_1387;
			static const V cephes_1997;
			static const V cephes_3333;
			// for extended precision modular passes.
			static const V cephes_mdp1;
			static const V cephes_mdp2;
			static const V cephes_mdp3;
			// sine / cosine calculation coefficients
			static const V cephes_sin_p0;
			static const V cephes_sin_p1;
			static const V cephes_sin_p2;
			static const V cephes_cos_p0;
			static const V cephes_cos_p1;
			static const V cephes_cos_p2;
		};
	}; // simd
}; // cpl

#ifdef CPL_COMPILER_MULTIPLE_STATICS_SUPPORTED
#include "simd_data.inl"
#endif

#endif

