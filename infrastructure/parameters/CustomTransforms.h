#ifndef CPL_CUSTOMTRANSFORMS_H
#define CPL_CUSTOMTRANSFORMS_H

#include <string>
#include <type_traits>
#include "../../Mathext.h"
#include "../../Exceptions.h"
namespace cpl
{
	template<typename T>
	class VirtualTransformer
	{
	public:
		/// <summary>
		/// Transforms a normalized value (bounded by a Restrictor, usually 0-1) to 
		/// a semantic value.
		/// </summary>
		virtual T transform(T val) const noexcept = 0;
		/// <summary>
		/// Normalized a transformed value, ie. the reversed process of transform()
		/// </summary>
		virtual T normalize(T val) const noexcept = 0;
		/// <summary>
		/// Retrieve the reciprocal quantization, i.e. the amount of uniquely represented values.
		/// Negative values means this transformer is not quantized
		/// </summary>
		virtual int getQuantization() { return -1; }
		/// <summary>
		/// Retrieve the reciprocal quantization, i.e. the amount of uniquely represented values.
		/// </summary>
		virtual void setQuantization(int) { }
		virtual ~VirtualTransformer() {}
	};

	template<typename T>
	class ChoiceTransformer : public VirtualTransformer<T>
	{
	public:
		virtual T transform(T val) const noexcept override
		{
			if (quantization <= 1)
				return 0;

			return std::round(val * (quantization - 1));
		}

		virtual T normalize(T val) const noexcept override
		{
			if (quantization <= 1)
				return 0;

			return val / (quantization - 1);
		}
		/// <summary>
		/// Retrieve the quantization, i.e. the amount of uniquely represented values.
		/// Negative values means this transformer is not quantized
		/// </summary>
		virtual int getQuantization() override { return quantization; }
		/// <summary>
		/// Sets the quantization steps. Notice that values below 2 are not meaningful.
		/// </summary>
		virtual void setQuantization(int rquantization) override
		{
			if (rquantization <= 1)
			{
				CPL_RUNTIME_EXCEPTION("quantization must be higher than zero");
			}
			quantization = rquantization;
		}

	private:
		int quantization = 0;
	};

	template<typename T>
	class RangedVirtualTransformerBase : public VirtualTransformer<T>
	{
	public:
		RangedVirtualTransformerBase(T minimum, T maximum) : min(minimum), max(maximum) {}
		RangedVirtualTransformerBase() : min(0), max(1) {}

		void setMinimum(T minimum) { min = minimum; }
		void setMaximum(T maximum) { max = maximum; }
		void setRange(T minimum, T maximum) { setMinimum(minimum); setMaximum(maximum); }

	protected:
		T min, max;
	};

	template<typename T>
	class UnityRange : public VirtualTransformer<T>
	{
	public:
		T normalize(T val) const noexcept override
		{
			return val;
		}

		T transform(T val) const noexcept override
		{
			return val;
		}
	};

	template<typename T>
	class BooleanRange : public VirtualTransformer<T>
	{
	public:

		int getQuantization() override { return 2; }

		T normalize(T val) const noexcept override
		{
			return val >= 0.5 ? 1 : 0;
		}

		T transform(T val) const noexcept override
		{
			return val >= 0.5 ? 1 : 0;
		}
	};


	template<typename T>
	class LinearRange : public RangedVirtualTransformerBase<T>
	{
	public:

		using RangedVirtualTransformerBase<T>::min;
		using RangedVirtualTransformerBase<T>::max;

		LinearRange(T minimum, T maximum) : RangedVirtualTransformerBase<T>(minimum, maximum) {}
		LinearRange() {}

		T normalize(T val) const noexcept override
		{
			return Math::UnityScale::Inv::linear(val, min, max);
		}

		T transform(T val) const noexcept override
		{
			return Math::UnityScale::linear(val, min, max);
		}
	};

	template<typename T>
	class IntegerLinearRange : public RangedVirtualTransformerBase<T>
	{
	public:

		using RangedVirtualTransformerBase<T>::min;
		using RangedVirtualTransformerBase<T>::max;

		IntegerLinearRange(T minimum, T maximum) : RangedVirtualTransformerBase<T>(minimum, maximum) {}
		IntegerLinearRange() {}

		int getQuantization() override { return max - min; }

		T normalize(T val) const noexcept override
		{
			return static_cast<T>(Math::UnityScale::Inv::linear(std::round(val), min, max));
		}

		T transform(T val) const noexcept override
		{
			return static_cast<T>(std::round(Math::UnityScale::linear(val, min, max)));
		}
	};

	template<typename T>
	class ExponentialRange : public RangedVirtualTransformerBase<T>
	{
	public:

		using RangedVirtualTransformerBase<T>::min;
		using RangedVirtualTransformerBase<T>::max;

		ExponentialRange(T minimum, T maximum) : RangedVirtualTransformerBase<T>(minimum, maximum) {}
		ExponentialRange() {}

		T normalize(T val) const noexcept override
		{
			return Math::UnityScale::Inv::exp(val, min, max);
		}

		T transform(T val) const noexcept override
		{
			return Math::UnityScale::exp(val, min, max);
		}
	};


	template<typename T>
	class ExponentialTranslationRange : public RangedVirtualTransformerBase<T>
	{
	public:

		using RangedVirtualTransformerBase<T>::min;
		using RangedVirtualTransformerBase<T>::max;

		ExponentialTranslationRange(T minimum, T maximum, T translatedMinimum, T translatedMaximum) : RangedVirtualTransformerBase<T>(minimum, maximum) { setTranslatedRange(translatedMinimum, translatedMaximum); }
		ExponentialTranslationRange() { setTranslatedRange(0, 1); }

		void setTranslatedMinimum(T minimum) { tmin = minimum; }
		void setTranslatedMaximum(T maximum) { tmax = maximum; }
		void setTranslatedRange(T minimum, T maximum) { setTranslatedMinimum(minimum); setTranslatedMaximum(maximum); }

		T normalize(T val) const noexcept override
		{
			T translation = min - tmin;
			T scale = tmax / (max - translation);
			return Math::UnityScale::Inv::exp((val + translation) / scale, min, max);
		}

		T transform(T val) const noexcept override
		{
			T translation = min - tmin;
			T scale = tmax / (max - translation);
			return (Math::UnityScale::exp(val, min, max) - translation) * scale;
		}

	protected:

		T tmin, tmax;
	};
};

#endif