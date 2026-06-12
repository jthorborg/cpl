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

	file:dsp.h

		Some definitions, algorithms for dsp. Includes other stuff as well.

*************************************************************************************/

#ifndef CPL_DSP_H
#define CPL_DSP_H

#include "Mathext.h"
#include <vector>
#include <complex>
#include <cstdint>
#include "Utility.h"
#include <complex>
#include "simd.h"
#include "lib/uarray.h"

namespace cpl
{
	namespace dsp
	{

		template<typename Ty>
		struct DualComplex
		{
			typedef Ty type;

			std::complex<type> val[2];

		};
		template<typename Ty>
		inline DualComplex<Ty> getZFromNFFT(Ty * tsf, std::size_t idx, std::size_t N)
		{
			idx <<= 1;
			N <<= 1;
			Ty x1 = tsf[idx];
			Ty x2 = tsf[N - idx];
			Ty y1 = tsf[idx + 1];
			Ty y2 = tsf[N - idx + 1];

			DualComplex<Ty> ret;

			ret.val[0] = std::complex<Ty>((x1 + x2) * Ty(0.5), (y1 - y2) * Ty(0.5));
			ret.val[1] = std::complex<Ty>((y1 + y2) * Ty(0.5), -(x1 - x2) * Ty(0.5));

			return ret;
		}

		template<typename T, class alloc>
		std::vector<T, alloc> real(const std::vector<std::complex<T>, alloc> & cmplx)
		{
			std::vector<T, alloc> ret(cmplx.size());
			for (std::size_t i = 0; i < cmplx.size(); ++i)
				ret[i] = cmplx[i].real();

			return ret;
		}

		template<typename T, class alloc>
		std::vector<T, alloc> imag(const std::vector<std::complex<T>, alloc> & cmplx)
		{
			std::vector<T, alloc> ret(cmplx.size());
			for (std::size_t i = 0; i < cmplx.size(); ++i)
				ret[i] = cmplx[i].imag();

			return ret;
		}

		template<typename T>
		void fftshift(std::complex<T> * fft, std::size_t N)
		{
			std::rotate(fft, fft + N / 2, fft + N);
		}

		template<typename T, class A>
		void fftshift(std::vector<T, A> & fft)
		{
			std::rotate(fft.begin(), fft.begin() + fft.size() / 2, fft.end());
		}

		template<typename T>
		void normalize(T * out, std::size_t N)
		{
			T scale = T(1) / *std::max_element(out, out + N);
			std::for_each(out, out + N, [=](T & z) { z *= scale; });
		}

		template<typename T, typename A>
		void normalize(std::vector<T, A> & out)
		{
			T scale = T(1) / *std::max_element(out.begin(), out.end());
			std::for_each(out.begin(), out.end(), [=](T & z) { z *= scale; });
		}


		template<typename T>
		void normalize(std::complex<T> * out, std::size_t N)
		{
			T max = std::abs(out[0]);

			for (std::size_t i = 1; i < N; ++i)
			{
				const T current = std::abs(out[i]);
				if (current > max)
					max = current;
			}

			const T scale = T(1) / max;

			std::for_each(out, out + N, [=](T & z) { z *= scale; });
		}

		template<typename T, typename A>
		void normalize(std::vector<std::complex<T>, A> & out)
		{
			T max = std::abs(out[0]);

			for (std::size_t i = 1; i < out.size(); ++i)
			{
				const T current = std::abs(out[i]);
				if (current > max)
					max = current;
			}

			const T scale = T(1) / max;

			std::for_each(out.begin(), out.end(), [=](T & z) { z *= scale; });
		}


		template<typename T>
		class Resonator
		{
			typedef T Scalar;
		public:
			Resonator(Scalar omega, std::size_t size)
				: buffer(size), ptr(0), N(size)
			{
				q = 2 * M_PI / N;
				r = Scalar(1.0) - std::numeric_limits<Scalar>::epsilon();
				rn = static_cast<Scalar>(std::pow(1.0 - std::numeric_limits<Scalar>::epsilon(), N));
				pole[0] = std::polar(r, omega - q);
				pole[1] = std::polar(r, omega);
				pole[2] = std::polar(r, omega + q);
			}

			inline void resonate(Scalar x)
			{
				//Scalar input = x;
				Scalar input = buffer[ptr] * rn + x;
				state[0] = state[0] * pole[0] - input;
				state[1] = state[1] * pole[1] - input;
				state[2] = state[2] * pole[2] - input;


				buffer[ptr] = x;
				ptr++;
				ptr %= N;
			}

			std::complex<Scalar> getResonance() {
				return std::conj(Scalar(-0.25) * state[0] + Scalar(0.5) * state[1] + Scalar(-0.25) * state[2]) / Scalar(N);
			}

		private:
			std::complex<Scalar> state[3], pole[3];
			std::vector<Scalar> buffer;
			Scalar r, rn, q;
			std::size_t ptr, N;
		};


		template<typename T>
		inline T radsToDegrees(T input)
		{
			return 360 * input / TAU;
		}


		template<typename Scalar>
		class CFastOscillator
		{
		public:
			typedef Scalar ScalarTy;
			CFastOscillator(ScalarTy period) : z1(0), z2(0) { reset(period); };
			CFastOscillator() {}
			void reset(ScalarTy period, ScalarTy freq = ScalarTy(1), ScalarTy phase = ScalarTy(0))
			{
				double w = (freq * TAU) / period;
				omega = 2 * std::cos(w);
				z1 = std::cos(-w + phase - HALFPI);
				z2 = std::cos(-2 * w + phase - HALFPI);
			}

			inline ScalarTy tick()
			{
				ScalarTy sample = omega * z1 - z2;
				z2 = z1;
				z1 = sample;
				return sample;
			}

		private:
			ScalarTy z1, z2, omega;
		};

		template<bool precise>
		typename std::enable_if<precise, double>::type
			lzresponse(double x, int size)
		{
			return x ? (size * sin(simd::consts<double>::pi * x) * sin(simd::consts<double>::pi * x / size)) / (simd::consts<double>::pi_squared * x * x) : 1;
		}

		template<bool precise>
		typename std::enable_if<precise, float>::type
			lzresponse(float x, int size)
		{
			return x ? (size * sinf(simd::consts<float>::pi * x) * sinf(simd::consts<float>::pi * x / size)) / (simd::consts<float>::pi_squared * x * x) : 1;
		}

		template<bool precise>
		typename std::enable_if<precise, double>::type
			scresponse(double x)
		{
			return x ? (sin(simd::consts<double>::pi * x)) / (simd::consts<double>::pi * x) : 1;
		}

		template<bool precise>
		typename std::enable_if<precise, float>::type
			scresponse(float x)
		{
			return x ? (std::sin(simd::consts<float>::pi * x)) / (simd::consts<float>::pi * x) : 1;
		}

		template<typename T, bool precise>
		typename std::enable_if<!precise, double>::type
			lzresponse(double x, int size)
		{
			return x ? (size * cpl::Math::fastsine(simd::consts<float>::pi * x) * cpl::Math::fastsine(simd::consts<float>::pi * x / size)) / (simd::consts<float>::pi_squared * x * x) : 1;
		}

		template<typename T, bool precise>
		typename std::enable_if<!precise, float>::type
			lzresponse(float x, int size)
		{
			return x ? (size * cpl::Math::fastsine(simd::consts<float>::pi * x) * cpl::Math::fastsine(simd::consts<float>::pi * x / size)) / (simd::consts<float>::pi_squared * x * x) : 1;
		}


		template<typename Scalar, class Vector>
		static std::complex<Scalar> goertzel(const Vector & data, std::size_t size, Scalar omega)
		{
			Scalar sine, cosine, coeff, q0(0), q1(0), q2(0);

			sine = sin(omega);
			cosine = cos(omega);
			coeff = 2.0 * cosine;

			for (Types::fint_t t = 0; t < size; t++)
			{
				q0 = coeff * q1 - q2 + data[t];
				q2 = q1;
				q1 = q0;
			}

			Scalar real = (q1 - q2 * cosine);
			Scalar imag = (q2 * sine);

			return std::complex<Scalar>(real, imag);

		}

		template<typename TResonance, bool precise = true, typename TInput, typename TFraction>
		inline auto lanczosFilter(TInput* vec, Types::fsint_t asize, TFraction x, Types::fsint_t wsize)
		{
            typedef typename std::remove_reference<decltype(vec[0])>::type TRet;

			TResonance resonance = 0;
			const auto start = cpl::Math::floorToNInf<Types::fsint_t>(x);

			for (Types::fsint_t i = start - wsize + 1; i < (start + wsize + 1); ++i)
			{
				if (i >= 0 && i < asize)
				{
					auto impulse = vec[i];
					auto response = lzresponse<precise>(x - i, wsize);
					resonance += impulse * response;
				}
			}

			return static_cast<TRet>(resonance);
		}

		template<typename R, bool precise = true, typename T, typename Y>
		inline R lanczosFilter(uarray<T> vec, Y x, Types::fsint_t wsize)
		{
            typedef typename std::remove_reference<decltype(vec[0])>::type TRet;

			R resonance = 0;
			Types::fsint_t start = cpl::Math::floorToNInf<Types::fsint_t>(x);
			Types::fsint_t asize = static_cast<Types::fsint_t>(vec.size());

			for (Types::fsint_t i = start - wsize + 1; i < (start + wsize + 1); ++i)
			{
				if (i >= 0 && i < asize)
				{
					auto impulse = vec[i];
					auto response = lzresponse<precise>(x - i, wsize);
					resonance += static_cast<R>(impulse * response);
				}
			}

			return static_cast<TRet>(resonance);
		}

		template<typename R, bool precise = true, typename T, typename Y>
		inline auto sincFilter(T* vec, std::size_t asize, Y x, Types::fsint_t wsize)
		{
            typedef typename std::remove_reference<decltype(vec[0])>::type TRet;

			R resonance = 0;
			Types::fsint_t start = cpl::Math::floorToNInf<Types::fsint_t>(x);

			for (Types::fsint_t i = start - wsize + 1; i < (start + wsize + 1); ++i)
			{
				if (i >= 0 && i < asize)
				{
					auto impulse = vec[i];
					auto response = scresponse<precise>(x - i);
					resonance += static_cast<R>(impulse * response);
				}
			}

			return static_cast<TRet>(resonance);
		}

		template<typename R, typename T, typename Y>
		inline auto linearFilter(T* vec, Types::fsint_t asize, Y x) -> typename std::remove_reference<decltype(vec[0])>::type
		{
			Types::fsint_t x1 = cpl::Math::floorToNInf<Types::fsint_t>(x);
			Types::fsint_t x2 = std::min(asize - 1, x1 + 1);

			//if (x2 == asize - 1)
			//	return vec[x1];

			auto frac = x - x1;

			return vec[x1] * (1 - frac) + vec[x2] * frac;
		}

		template<typename T, typename Y>
		inline T linearFilter(cpl::uarray<T> vec, Y x)
		{
			Types::fsint_t x1 = cpl::Math::floorToNInf<Types::fsint_t>(x);
			Types::fsint_t x2 = std::min(static_cast<Types::fsint_t>(vec.size()) - 1, x1 + 1);

			//if (x2 == asize - 1)
			//	return vec[x1];

			auto frac = x - x1;

			return vec[x1] * (1 - frac) + vec[x2] * frac;
		}

		template<typename T>
		void fillWithFreq(T& vec, size_t size, double freq, double samplingRate, double initialPhase, double amplitude = 1)
		{
			auto omega = 2 * M_PI * freq / samplingRate;
			double phase = initialPhase;
			for (unsigned i = 0; i < size; ++i)
			{
				vec[i] = amplitude * std::sin(phase);
				phase += omega;
				if (phase > 2 * M_PI)
					phase -= 2 * M_PI;
			}

		}

		template<typename T>
		void addFillWithFreq(T & vec, size_t size, double freq, double samplingRate, double initialPhase, double amplitude = 1)
		{
			auto omega = 2 * M_PI * freq / samplingRate;
			double phase = initialPhase;
			for (unsigned i = 0; i < size; ++i)
			{
				vec[i] += amplitude * std::sin(phase);
				phase += omega;
				if (phase > 2 * M_PI)
					phase -= 2 * M_PI;
			}

		}

		template<typename T>
		void fillWithRand(T & vec, size_t size)
		{
			typedef unq_typeof(vec[0]) T2;
			for (std::size_t i = 0; i < size; ++i)
			{
				vec[i] = ((T2(RAND_MAX) / (T2)2) - (T2)rand()) / (T2(RAND_MAX) / (T2)2);
			}

		}

		template<typename T, typename T2>
		void linspace(T & vec, std::size_t size, T2 min, T2 max)
		{
			if (size == 1)
				vec[0] = min;
			else
				for (std::size_t i = 0; i < size; ++i)
				{
					vec[i] = min + (max - min) * i / (size - 1);
				}
		}

		typedef std::uint_fast32_t inttype;

		inline void separateTransforms(const double * tsf, double * real, double * imag, std::size_t N)
		{
			N <<= 1;
			double x1, x2, y1, y2;
			for (std::size_t k = 2u; k < N; k += 2)
			{
				x1 = tsf[k];
				x2 = tsf[N - k];
				y1 = tsf[k + 1];
				y2 = tsf[N - k + 1];

				real[k] = (x1 + x2) * 0.5;
				real[k + 1] = (y1 - y2) * 0.5;
				imag[k] = (y1 + y2) * 0.5;
				imag[k + 1] = -(x1 - x2) * 0.5;
			}
			real[0] = tsf[0];
			imag[0] = tsf[1];
		}

		/// <summary>
		/// Assuming a fourier transform, where the input real and imaginary signal have pure real transforms,
		/// separates the transforms such that they mirror around the nyquist bins. Example output:
		/// 
		///		bin[0] = {real DC, imag DC}
		///		bin[X ... N/2 -2] = real bin X
		///		bin[N/2 -1] = real nyquist
		///		bin[N/2] = imag nyquist
		///		bin[N/2 + X... N - 1] = imag bin N/2 - X
		/// 
		/// Based on the concept, "two for the price of one".
		/// http://www.engineeringproductivitytools.com/stuff/T0001/PT10.HTM
		/// </summary>
		/// <param name="tsf">
		/// Array of complex T (real, imag pairs)
		/// </param>
		/// <param name="N">
		/// The amount of complex number pairs in the array. Behaviour is undefined if N isn't a power of 2.
		/// </param>
		template<typename T>
		inline typename std::enable_if<std::is_arithmetic<T>::value, void>::type
			separateTransformsIPL(uarray<T> tsf)
		{
			auto N = tsf.size() >> 1;
			auto N2 = N; // N = total size, N2 = half size
			N <<= 1;
			T x1, x2, y1, y2;

			for (auto k = 2u; k < N2; k += 2)
			{
				x1 = tsf[k];
				x2 = tsf[N - k];
				y1 = tsf[k + 1];
				y2 = tsf[N - k + 1];

				// real[k] = (x1 + x2) * 0.5;
				tsf[k] = (x1 + x2) * T(0.5); // real bin N channel 1
				// real[k + 1] = (y1 - y2) * 0.5;
				tsf[N - k] = (y1 + y2) * T(0.5); // real bin N channel 2
				// real[k + 1] = (y1 - y2) * 0.5;
				tsf[k + 1] = (y1 - y2) * T(0.5); // imag bin N channel 1
				// imag[k + 1] = -(x1 - x2) * 0.5;
				tsf[N - k + 1] = -(x1 - x2) * T(0.5); // imag bin N channel 2
			}
			// TODO: figure out why real nyquist is half value.
			tsf[N2 - 2] *= T(2);
			tsf[N2 - 1] *= T(2);
		}

		/// <summary>
		/// Assuming a fourier transform, where the input real and imaginary signal have pure real transforms,
		/// separates the transforms such that they mirror around the nyquist bins. Example output:
		/// 
		///		bin[0] = {real DC, imag DC}
		///		bin[X ... N/2 -2] = real bin X
		///		bin[N/2 -1] = real nyquist
		///		bin[N/2] = imag nyquist
		///		bin[N/2 + X... N - 1] = imag bin N/2 - X
		/// 
		/// Based on the concept, "two for the price of one".
		/// http://www.engineeringproductivitytools.com/stuff/T0001/PT10.HTM
		/// </summary>
		/// <param name="tsf">
		/// Array of complex T (real, imag pairs)
		/// </param>
		/// <param name="N">
		/// The amount of complex number pairs in the array. Behaviour is undefined if N isn't a power of 2.
		/// </param>
		template<typename T>
		inline void separateTransformsIPL(uarray<std::complex<T>> tsf)
		{
			return separateTransformsIPL(tsf.template reinterpret<T>());
		}

		template<typename Scalar, class Vector, bool scale = false>
		std::complex<Scalar> fourierTransform(const Vector & data, Scalar frequency, Scalar sampleRate, inttype a, inttype b)
		{
			// unpacked xmm registers
			float unpreal[4];
			float  unpimag[4];

			cpl::Types::v4sf
				xmmsin, // the next 4 sines
				xmmcos, // the next 4 cosines
				xmmsignal, // the next 4 signals
				xmmc1, xmmc2, // coefficients for oscillator
				xmmtemp1, xmmtemp2, xmmtemp3,  // temporary accumulator
				xmmreals = _mm_setzero_ps(), // accumulated reals
				xmmimags = _mm_setzero_ps(); // accumulated imaginaries

			/*
			Now we init the phases for the oscillators.
			Since we run 4 sine/cosine pairs, we shift the phase offset by omega * n
			for the n'th pair
			*/
			Scalar omega = 2 * frequency * Scalar(PI) / sampleRate;
			xmmtemp1 = _mm_setr_ps(0, omega, omega * 2, omega * 3);
			cpl::simd::sincos(xmmtemp1, &xmmsin, &xmmcos);




			/*

				http://www.kvraudio.com/forum/viewtopic.php?p=5775364

			coefficient calculations:
			const omega = tan(pi*cutoff / samplerate);
			const z = 2 / (1 + omega*omega)
			c1 = z - 1
			c2 = omega * z
			xmmcos = cos(0)
			xmmsin = sin(0)
			*/
			// omega
			// note the omega * 2 here - in reality, it should be / 2 given the formulae.
			// however, since we run n oscillators phase shifted by omega * n, they actually have to run at 4x frequency
			// this is to satisfy that xmmsin(t)[n] == sin((n + t) * omega))
			Scalar real = tan(omega * 2);
			// z
			Scalar imag = Scalar(2.0) / (Scalar(1.0) + real * real);

			// c2 = omega * z
			Scalar c0 = real * imag;
			xmmc2 = _mm_load_ps1(&c0);
			//c1 = z - 1
			c0 = imag - Scalar(1.0);
			xmmc1 = _mm_load_ps1(&c0);


			/*

			run the main loop

			*/

			//_mm_prefetch((const char *)data, _MM_HINT_T0); // doesn't do much

			for (inttype t = 0; t < b; t += 4)
			{
				/* run the fourier transform here. */

				// get the next batch of floats from the input

				xmmsignal = _mm_load_ps(&data[t]);
				xmmtemp1 = _mm_mul_ps(xmmsignal, xmmcos);
				xmmtemp2 = _mm_mul_ps(xmmsignal, xmmsin);
				// real += signal[t] * cos(w * t)
				xmmreals = _mm_add_ps(xmmreals, xmmtemp1);
				// imag += signal[t] * sin(w * t)
				xmmimags = _mm_add_ps(xmmimags, xmmtemp2);

				/*
				generate the next sines and cosines:
				const t0 = c1 * cos - c2 * sin
				const t1 = c2 * cos + c1 * sin
				..
				cos = t0
				sin = t1

				*/

				xmmtemp1 = _mm_mul_ps(xmmcos, xmmc1); // c1 * cos
				xmmtemp2 = _mm_mul_ps(xmmsin, xmmc2); // c2 * sin
				xmmtemp3 = _mm_sub_ps(xmmtemp1, xmmtemp2); // final cosine, t0

				// these two lines have no dependency on the upper three, so should be pipelined
				xmmcos = _mm_mul_ps(xmmcos, xmmc2); // c2 * cos
				xmmsin = _mm_mul_ps(xmmsin, xmmc1); // c1 * sin

				xmmsin = _mm_add_ps(xmmcos, xmmsin); // final sine, t1
				xmmcos = xmmtemp3;
			}
			/*
			store the accumulated real/imag pairs into a complex
			*/

			_mm_store_ps(unpreal, xmmreals);
			_mm_store_ps(unpimag, xmmimags);
			real = 0; real += unpreal[0]; real += unpreal[1]; real += unpreal[2]; real += unpreal[3];
			imag = 0; imag += unpimag[0]; imag += unpimag[1]; imag += unpimag[2]; imag += unpimag[3];


			if (scale)
			{
				return std::complex<Scalar>(real, -imag) / std::complex<Scalar>((b - a) / 2.0);
			}
			else
				return std::complex<Scalar>(real, -imag);

		}



		template<typename Scalar, class Vector>
		void haarDWT(Vector & input, Vector & output, std::size_t size)
		{
			for (auto length = size >> 1;; length >>= 1)
			{
				for (int i = 0; i < length; ++i) {
					Scalar sum = input[i * 2] + input[i * 2 + 1];
					Scalar difference = input[i * 2] - input[i * 2 + 1];
					output[i] = sum;
					output[length + i] = difference;
				}


				if (length == 1)
					return;

				std::memcpy(input, output, sizeof(Scalar)* length << 1);
			}

		}


		template<typename Scalar, class Vector>
		std::complex<Scalar> constantQTransform(Vector input,
			size_t size,
			Types::fint_t kbin,
			Scalar lowestFreq,
			Scalar numFiltersPerOctave,
			Scalar sampleRate)
		{
			// "spectral width" per filter
			Scalar filterWidth = std::pow(2, Scalar(1.0) / numFiltersPerOctave); // r = 2 ^ (1 / n)
			filterWidth = std::pow(filterWidth, kbin) * lowestFreq; // r ^ k * fmin == fk

			// centre frequency
			Scalar centreFrequency = std::pow(2, kbin * 1 / numFiltersPerOctave) * lowestFreq;

			// window length for bin
			Scalar windowLength = sampleRate / filterWidth; // fs / fk == N[k]
			Types::fint_t end = static_cast<Types::fint_t>(std::floor(windowLength));
			const Scalar Q = centreFrequency / filterWidth;
			// bounds check
			if (end > size)
				return 0;
			auto hammingWindow = [&](Types::fint_t n) // W[k, n]
			{
				auto const a = 25.0 / 46.0;
				return a - (1 - a) * std::cos((TAU * n) / windowLength);
			};

			std::complex<Scalar> acc;
			Scalar real, imag, sample;

			for (Types::fint_t n = 0; n < end; ++n)
			{
				sample = input[n] * hammingWindow(n);
				real = std::cos((TAU * Q * n) / windowLength) * sample;
				imag = std::sin((TAU * Q * n) / windowLength) * sample;
				acc += std::complex<Scalar>(real, -imag);

			}
			return acc / windowLength; // normalize
		}


	}; // dsp

}; // cpl
#endif
