#ifndef CPL_CUSTOMFORMATTERS_H
#define CPL_CUSTOMFORMATTERS_H

#include <string>
#include <cpl/LexicalConversion.h>
#include <type_traits>
#include <stdio.h>
#include "CustomTransforms.h"

namespace cpl
{
	enum class FormattingFlags : std::int32_t
	{
		none,
		includeUnit = 1 << 0,
		defaultFlags = includeUnit
	};

	inline FormattingFlags operator ~(FormattingFlags flags)
	{
		return static_cast<FormattingFlags>(~static_cast<std::int32_t>(flags));
	}

	inline FormattingFlags operator & (FormattingFlags a, FormattingFlags b)
	{
		return static_cast<FormattingFlags>(static_cast<std::int32_t>(a) & static_cast<std::int32_t>(b));
	}

	inline FormattingFlags operator | (FormattingFlags a, FormattingFlags b)
	{
		return static_cast<FormattingFlags>(static_cast<std::int32_t>(a) | static_cast<std::int32_t>(b));
	}

	template<typename T>
	typename std::enable_if<std::is_floating_point<T>::value, std::string>::type printer(const T & val, int precision = 2)
	{
		char buf[100];
		cpl::sprintfs(buf, "%.*f", precision, (double)val);
		return buf;
	}

	template<typename T>
	typename std::enable_if<!std::is_floating_point<T>::value, std::string>::type printer(const T & val, int precision = 2)
	{
		return std::to_string(val);
	}

	template<typename T>
	class VirtualFormatter
	{
	public:

		virtual bool format(const T & val, std::string & buf, FormattingFlags flags = FormattingFlags::defaultFlags) = 0;
		virtual bool interpret(const string_ref buf, T & val) = 0;
		virtual std::string_view getUnit() const { return {}; }
		virtual ~VirtualFormatter() {}
	};

	template<typename T>
	class BasicFormatter : public VirtualFormatter<T>
	{
	public:
		virtual bool format(const T & val, std::string & buf, FormattingFlags flags) override
		{
			buf = printer(val, 2);
			return true;
		}

		virtual bool interpret(const string_ref buf, T & val) override
		{
			return cpl::lexicalConversion(buf, val);
		}
	};

	template<typename T>
	class IntegerFormatter : public VirtualFormatter<T>
	{
	public:
		virtual bool format(const T & val, std::string & buf, FormattingFlags flags) override
		{
			auto intValue = static_cast<std::int64_t>(std::round(val));
			buf = printer(intValue, 2);
			return true;
		}

		virtual bool interpret(const string_ref buf, T & val) override
		{
			std::int64_t ret;
			if (cpl::lexicalConversion(buf, ret))
			{
				val = static_cast<T>(ret);
				return true;
			}

			return false;
		}
	};

	template<typename T>
	class HexFormatter : public VirtualFormatter<T>
	{
	public:
		virtual bool format(const T & val, std::string & buf, FormattingFlags flags) override
		{
			char buffer[100];
			cpl::sprintfs(buffer, "0x%X", (int)val);
			buf = buffer;
			return true;
		}

		virtual bool interpret(const string_ref buf, T & val) override
		{
			return cpl::lexicalConversion(buf, val);
		}
	};

	template<typename T>
	class BooleanFormatter : public VirtualFormatter<T>
	{
	public:
		virtual bool format(const T & val, std::string & buf, FormattingFlags flags) override
		{
			if (val >= (T)0.5)
				buf = "true";
			else
				buf = "false";
			return true;
		}

		virtual bool interpret(const string_ref buf, T & val) override
		{
			if (buf == "true" || buf == "True" || buf == "on" || buf == "On" || buf == "1")
				val = (T)1;
			else
				val = (T)0;
			return true;
		}
	};

	template<typename T>
	class UnitFormatter : public BasicFormatter<T>
	{
	public:

		UnitFormatter(const std::string_view unitToUse) { setUnit(unitToUse); }
		UnitFormatter() { }

		using BasicFormatter<T>::interpret;

		std::string_view getUnit() const override { return this->unit; }

		virtual bool format(const T & val, std::string & buf, FormattingFlags flags) override
		{
			BasicFormatter<T>::format(val, buf, flags);

			if ((flags & FormattingFlags::includeUnit) != FormattingFlags::none)
				buf += " " + unit;

			return true;
		}

		void setUnit(const std::string_view unit) { this->unit = unit; }

	private:
		std::string unit;
	};

	template<typename T>
	class DBFormatter : public UnitFormatter<T>
	{
	public:

		DBFormatter() : UnitFormatter<T>("dB") {}

		virtual bool format(const T & val, std::string & buf, FormattingFlags flags) override
		{
			return UnitFormatter<T>::format(20 * std::log10(val), buf, flags);
		}

		virtual bool interpret(const string_ref buf, T & val) override
		{
			T dbVal;
			if (UnitFormatter<T>::interpret(buf, dbVal))
			{
				val = std::pow(10, dbVal / 20);
				return true;
			}

			return false;
		}
	};

	template<typename T>
	class PercentageFormatter : public UnitFormatter<T>
	{
	public:

		PercentageFormatter() : UnitFormatter<T>("%") {}

		virtual bool format(const T & val, std::string & buf, FormattingFlags flags) override
		{
			return UnitFormatter<T>::format(std::round(val * 10000) / 100, buf, flags);
		}

		virtual bool interpret(const string_ref buf, T & val) override
		{
			T dbVal;
			if (UnitFormatter<T>::interpret(buf, dbVal))
			{
				val = dbVal / 100;
				return true;
			}

			return false;
		}
	};

	template<typename T>
	class ChoiceFormatter : public VirtualFormatter<T>
	{
	public:

		ChoiceFormatter(ChoiceTransformer<T> & transformerRef) : transformer(transformerRef) {}

		void setValues(std::vector<std::string> valuesToConsume)
		{
			this->values = std::move(valuesToConsume);
			transformer.setQuantization(static_cast<int>(values.size()));
		}
		const std::vector<std::string> & getValues() const noexcept { return values; }

		virtual bool format(const T & val, std::string & buf, FormattingFlags flags) override
		{
			if (values.size() == 0)
			{
				return false;
			}

			auto index = static_cast<std::size_t>(
				std::min<std::size_t>(values.size() - 1, static_cast<std::size_t>(std::max<T>(std::round(val), (T)0)))
			);
			buf = values[index];

			return true;
		}

		virtual bool interpret(const string_ref buf, T & val) override
		{
			for (std::size_t i = 0; i < values.size(); ++i)
			{
				if (values[i] == buf)
				{
					val = static_cast<T>(i);
					return true;
				}
			}

			return false;
		}

	private:
		ChoiceTransformer<T> & transformer;
		std::vector<std::string> values;
	};
};

#endif
