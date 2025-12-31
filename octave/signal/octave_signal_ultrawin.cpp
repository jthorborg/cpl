// Copyright (c) 2013 Rob Sykes <robs@users.sourceforge.net>
//
// This program is free software; you can redistribute it and/or modify it under
// the terms of the GNU General Public License as published by the Free Software
// Foundation; either version 3 of the License, or (at your option) any later
// version.
//
// This program is distributed in the hope that it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
// FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
// details.
//
// You should have received a copy of the GNU General Public License along with
// this program; if not, see <http://www.gnu.org/licenses/>.

/*
 * This file has been modified from its original (__ultrwin__.cc) from the octave signal package.
 * The current version was last modified 27th of december, 2015, to support a parameterized interface
 * callable from pure C++, as well as replacing any buffer allocations with in-place calculations,
 * such that it is deterministic in run-time.
 * Modified by Janus Thorborg (jthorborg.com) as part of the cpl C++ library.
 * The code is an implementation of the algorithm presented in:
 *	S.W.A. Bergen, "Design of the ultraspherical window function and Its applications," Ph.D. Dissertation, University of Victoria, Sept. 2005.
 */

#include <cmath>
#include <cstdlib>
#include <cstdio>
#include "octave_signal_ultrawin.h"

#if !defined M_PI
#define M_PI 3.14159265358979323846
#endif



#if DEBUG_ULTRWIN
#define SHOW1(x) fprintf(stderr, "%c%+.3e", " 1"[(x) > .5], (x) - ((x) > .5))
#endif

#define EPSILON     (1./0x100000) /* For caller rounding error. */
#define BETA_MAX    (12*(1+EPSILON))
#define XMU_MIN     (.99*(1-EPSILON))

namespace octave
{
	namespace signal
	{

		template<typename TOut, typename T>
			void internal_ultraspherical_win(TOut* w, int n, T mu, T xmu)
			{
			  int bad = n < 1 || xmu < XMU_MIN || (!mu && xmu == 1) ||
				(n > BETA_MAX * 2 && xmu * std::cos((T)M_PI * (T)BETA_MAX / n) > 1);
			  if (!bad && w) {
				int i, j, k, l = 0, m = (n + 1) / 2, met;
				TOut* divs = w + m - 1;
				T c = 1 - 1 / (xmu * xmu), t, u, v[64], vp, s;
				for (i = 0; i < (int)(sizeof(v) / sizeof(v[0])); v[i++] = 0);
				if (n > 1) for (i = 0; i < m; l = j - (j <= i++)) {
				  vp = *v, s = *v = i? (*v + v[1]) * mu * (divs[i] = (TOut)1/i) : 1;
				  for (met = 0, j = 1, u = 1; ; ++l, v[l] = vp * (i - l) / (mu + l - 1)) {
					#define _ t = v[j], v[j] += vp, vp = t, t = s, s += \
						v[j] * (u *= c * (n - i - j) * divs[j]), met = s && s == t, ++j,
					for (k = ((l-j+1) & ~7) + j; j < k && !met; _ _ _ _ _ _ _ _ (void)0);
					for (; j <= l && !met; _ (void)0);
					#undef _
					if (met || !(j <= i)) break;
				  }
				  w[i] = static_cast<TOut>(s / (n - i - 1));
				}
				else w[0] = 1;
				u = 1 / w[i = m - 1], w[i] = 1;
				for (--i ; i >= 0; u *= (n - 2 - i + mu) / (n - 2 - i), w[i] = static_cast<TOut>(w[i] * u), --i);
				for (i = 0; i < m; w[n - 1 - i] = w[i], ++i);
			  }
			}

		template<typename TOut, typename T>
			uspv_t<T> ultraspherical_polyval(int n, T mu, T x, TOut const *divs)
			{
			  T fp = n > 0 ? 2 * x * mu : 1, fpp = 1, f;
			  uspv_t<T> result;
			  int i, k;
			  #define _ f = (2*x*(i+mu)*fp - (i+2*mu-1)*fpp) * divs[i+1], fpp=fp, fp=f, ++i,
			  for (i = 1, k = i + ((n - i) & ~7); i < k; _ _ _ _ _ _ _ _ (void)0);
			  for (; i < n; _ (void)0);
			  #undef _
			  result.f = fp, result.fp = fpp;
			  return result;
			}


		#define MU_EPSILON  ((T)1./0x4000)
		#define EQ(mu,x)    (std::abs((mu)-(x)) < MU_EPSILON)


		template<typename TOut, typename T>
			uspv_t<T> ultraspherical_polyval2(     /* With non-+ve integer protection */
				int n, T mu, T x, TOut const * divs)
			{
			  int sign = (~(int)std::floor(mu) & ~(~2u/2))? 1:-1; /* -ve if floor(mu) <0 & odd */
			  uspv_t<T> r;
			  if (mu < MU_EPSILON && EQ(mu,(int)mu))
				mu = floor(mu + (T).5) + MU_EPSILON * ((int)mu > mu? -1:1);
			  r = ultraspherical_polyval(n, mu, x, divs);
			  r.f *= sign, r.fp *= sign;
			  return r;
			}


		template<typename TOut, typename T>
		T find_zero(int n, T mu, int l, T extremum_mag, T ripple_ratio,
				  T lower_bound, TOut const *divs)
		{
		  T dx, x0, t, x, epsilon = (T)1e-10, one_over_deriv, target = 0;
		  int i, met = 0;
		  if (!divs)
			return 0;
		  if (!l) {
			T r = ripple_ratio;   /* FIXME: factor in weighted extremum_mag here */
			x = r > 1 ? std::cosh(std::acosh(r) / n) : std::cos(std::acos(r) / n); /* invert chebpoly-1st */
			x0 = x *= lower_bound / std::cos((T)(M_PI * .5 / n)) + epsilon;
			target = log(extremum_mag * ripple_ratio);
		  }
		  else {
			T cheb1 = std::cos(T(M_PI * (l - .5) / n)), cheb2 = std::cos(T(M_PI * l / (n + 1)));
			if (mu < 1 - l && EQ((int)(mu + (T).5),mu+ (T).5)) x = (T)(met = 1);
			else if (EQ(mu,0)) x = cheb1, met = 1;               /* chebpoly-1st-kind */
			else if (EQ(mu,1)) x = cheb2, met = 1;               /* chebpoly-2nd-kind */
			else x = (cheb1 * cheb2) / (mu * cheb1 + (1 - mu) * cheb2);
			x0 = x;
		  }
		  for (i = 0; i < 24 && !met; ++i, met = std::abs(dx) < epsilon) {/*Newton-Raphson*/
			uspv_t<T> r = ultraspherical_polyval2(n, mu, x, divs);
			if (!(t = ((2*mu + n-1) * r.fp - n*x * r.f)))         /* Fail if div by 0 */
			  break;
			one_over_deriv = (1 - x*x) / t;    /* N-R slow for deriv~=1, so take log: */
			if (!l) {                               /* d/dx(f(g(x))) = f'(g(x)).g'(x) */
			  one_over_deriv *= r.f;                             /* d/dx(log x) = 1/x */
			  if (r.f <= 0)                                 /* Fail if log of non-+ve */
				break;
			  if (x + (dx = (target - log(r.f)) * one_over_deriv) <= lower_bound)
				dx = (lower_bound - x) * (T).875;
			  x += dx;
			}
			else x += dx = -r.f * one_over_deriv;
		#if DEBUG_ULTRWIN
			fprintf(stderr, "1/deriv=%9.2e dx=%9.2e x=", one_over_deriv, dx);
			SHOW1(x); fprintf(stderr, "\n");
		#endif
		  }
		#if DEBUG_ULTRWIN
		  fprintf(stderr, "find_zero(n=%i mu=%g l=%i target=%g r=%g x0=",
			  n, mu, l, target, ripple_ratio);
		  SHOW1(x0); fprintf(stderr, ") %s ", met? "converged to" : "FAILED at");
		  SHOW1(x); fprintf(stderr, " in %i iterations\n", i);
		#else
		  static_cast<void>(x0);
		#endif
		  return met? x : 0;
		}

		template<typename T>
		T * make_divs(int n, T **divs, T * storage)
		{
		  int i;
		  if (!*divs) {
			*divs = storage;
			if (*divs)
			  for (i = 0; i < n; (*divs)[i] = (T)1./(i+1), ++i);
		  }
		  return *divs? *divs - 1 : 0;
		}


		#define DIVS make_divs(n, &divs, w)


		template<typename TOut, typename T>
		bool internal_ultraspherical_window(int n, TOut* w, T mu, T par, uswpt_t type, int even_norm, T *xmu_)
		{
			auto ret = false;
			T xmu = 0, last_extremum_pos = 0;
			TOut* divs = 0;

			if (n > 0 && fabs(mu) <= (8 * (1 + EPSILON)))
			{
				switch (type) 
				{
				case uswpt_Beta:
					xmu = mu == 1 && par == 1 ? 1 : par < .5 || par > BETA_MAX ? 0 :
						find_zero<TOut, T>(n - 1, mu, 1, 0, 0, 0, DIVS) / std::cos((T)M_PI * par / n);
					break;

				case uswpt_AttFirst: if (par < 0) break; /* Falling... */

				case uswpt_AttLast:
					if (type == uswpt_AttLast && mu >= 0 && par < 0);
					else if (!EQ(mu, 0)) {
						int extremum_num =
							type == uswpt_AttLast ? (int)((n - 2) / 2 + (T).5) : 1 + EQ(mu, (T)-1.5);
						T extremum_pos =
							find_zero<TOut, T>(n - 2, mu + 1, extremum_num, 0, 0, 0, DIVS);
						T extremum_mag = !extremum_pos ? 0 :
							fabs(ultraspherical_polyval2<TOut, T>(n - 1, mu, extremum_pos, DIVS).f);
						T xmu_lower_bound = !extremum_mag ? 0 :
							find_zero<TOut, T>(n - 1, mu, 1, 0, 0, 0, DIVS); /* 1st null */
						xmu = !xmu_lower_bound ? 0 : find_zero<TOut, T>(
							n - 1, mu, 0, extremum_mag, std::pow((T)10, (T)par / 20), xmu_lower_bound, DIVS);
						last_extremum_pos =
							type == uswpt_AttLast ? extremum_pos : last_extremum_pos;
					}
					else xmu = std::cosh(std::acosh(std::pow((T)10, (T)par / 20)) / (n - 1)); /* Cheby 1st kind */
					break;

				default: case uswpt_Xmu: xmu = par; break;
				}
			}
		#if DEBUG_ULTRWIN
		  fprintf(stderr, "n=%i mu=%.3f xmu=%.16g\n", n, mu, xmu);
		#endif
		  int i = n / 2 - 1, j = 1;
		  T x = 0;

		  if (xmu > 0)
		  {
			  x = even_norm == 1 ? 0 : last_extremum_pos ?
				  last_extremum_pos : find_zero<TOut, T>(n - 2, mu + 1, i, 0, 0, 0, DIVS);

			  internal_ultraspherical_win(w, n, mu, xmu);
			  ret = true;
		  }


		  if (ret && w && (~n & !!even_norm) && n > 2 && !(mu == 1 && xmu == 1)) {

			T t = 0, s = -1;
			x = x? (T)M_PI/2 - std::acos(x/xmu) : 0;
			for (; i >= 0; t += w[i] * (T)1. / (j + 1) * (s=-s) * (x?cos(j*x):1), --i, j += 2);
			for (t = (T)M_PI/4 / t, i = 0; t < 1 && i < n; w[i] *= t, ++i);
		#if DEBUG_ULTRWIN
			fprintf(stderr, "%snorm DFT(w.sinc πx) @ %g %.16g\n", t<1? "":"NO ", 2*x,t);
		#endif
		  }

		  if (xmu_ && ret)
			*xmu_ = xmu;

		  if (!ret)
			  // construct a rectangular window..
			  for (int i = 0; i < n; ++i)
				  w[i] = (T)1;

		  return ret;
		}

		bool ultraspherical_window(int n, double * w, double mu, double par, uswpt_t type, int even_norm, double *xmu_)
		{
			return internal_ultraspherical_window<double, double>(n, w, mu, par, type, even_norm, xmu_);
		}

		bool ultraspherical_window(int n, float * w, double mu, double par, uswpt_t type, int even_norm, double*xmu_)
		{
			return internal_ultraspherical_window<float, double>(n, w, mu, par, type, even_norm, xmu_);
		}


	};
};
