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

	file:Graphics.h

		Some rendering routines - move to /rendering

*************************************************************************************/

#ifndef CPL_GRAPHICS_H
#define CPL_GRAPHICS_H

#include <cstdint>
#include <algorithm>
#include <limits>
#include <type_traits>
#include "../PlatformSpecific.h"

#ifdef CPL_JUCE
	#include <juce_opengl/juce_opengl.h>
	using namespace juce::gl;
#else
	#ifdef __APPLE__
		#define GL_SILENCE_DEPRECATION
		#include <OpenGL/gl.h>
		#include <OpenGL/glu.h>
	#else
		#include <GL/gl.h>
		#include <GL/glu.h>
	#endif
#endif

namespace cpl
{
	// it should be called Graphics, but has n-dimensions appended to avoid 
	// name clashes.
	namespace GraphicsND
	{

		template<typename T>
		union Transform3D
		{
			Transform3D(T defaultValue)
			{
				for (auto & c : data)
					c = defaultValue;
			}
			struct component
			{
				T x, y, z;
			};

			T data[9];
			struct
			{
				component position;
				component rotation;
				component scale;
			};

			T & element(int x, int y) noexcept
			{
				return data[x * 3 + y];
			}

			const T & element(int x, int y) const noexcept
			{
				return data[x * 3 + y];
			}

			void applyToOpenGL()
			{
				// move first - bad
				glTranslatef(static_cast<GLfloat>(position.x), static_cast<GLfloat>(position.y), static_cast<GLfloat>(position.z));
				// to avoid clipping. this is probably not how it is done.
				glScalef(1, 1, 0.1f);
				glRotatef(static_cast<GLfloat>(rotation.x), 1.0f, 0.0f, 0.0f);
				glRotatef(static_cast<GLfloat>(rotation.y), 0.0f, 1.0f, 0.0f);
				glRotatef(static_cast<GLfloat>(rotation.z), 0.0f, 0.0f, 1.0f);
				// do the actual scaling
				glScalef(static_cast<GLfloat>(scale.x), static_cast<GLfloat>(scale.y), static_cast<GLfloat>(scale.z));

			}

		};


		/*
		http://en.wikipedia.org/wiki/Bresenham's_line_algorithm
		plots lines fast, but unaliased
		*/
		template <typename Ty, class plot>
		void inline bDrawLine(Ty x0, Ty y0, Ty x1, Ty y1, plot f)
		{
			Ty dx = fastabs(x1 - x0);
			Ty dy = fastabs(y1 - y0);
			Ty sx, sy, err, e2;
			if (x0 < x1)
				sx = 1;
			else
				sx = -1;
			if (y0 < y1)
				sy = 1;
			else
				sy = -1;
			err = dx - dy;

			while (true)
			{
				f(x0, y0);
				if (x0 == x1 && y0 == y1)
					break;
				e2 = err * 2;
				if (e2 > -dy)
				{
					err = err - dy;
					x0 = x0 + sx;
				}
				if (e2 < dx)
				{
					err = err + dx;
					y0 = y0 + sy;
				}
			}


		}


		#define ipart(x) ((int)x)

		#define round(x) ipart(x + 0.5)
		#define fpart(x) (x - ipart(x))
		#define rfpart(x) (1 - fpart(x))


		template <typename Ty, class plotFunction>
		void inline wuDrawLine(Ty x0, Ty y0, Ty x1, Ty y1, plotFunction plot)
		{
			bool steep = std::abs(y1 - y0) > std::abs(x1 - x0);
			if (steep)
			{
				std::swap(x0, y0);
				std::swap(x1, y1);
				if (x0 > x1)
				{
					std::swap(x0, x1);
					std::swap(y0, y1);
				}

				auto dx = x1 - x0;
				auto dy = y1 - y0;
				auto gradient = dy / dx; // float?

				// handle first endpoint
				auto xend = round(x0);
				auto yend = y0 + gradient * (xend - x0);
				auto xgap = rfpart(x0 + 0.5);

				int xpxl1 = xend; //this will be used in the main loop
				int ypxl1 = ipart(yend);

				plot(ypxl1, xpxl1, rfpart(yend) * xgap);
				plot(ypxl1 + 1, xpxl1, fpart(yend) * xgap);

				auto intery = yend + gradient; // first y-intersection for the main loop

				// handle second endpoint

				xend = round(x1);
				yend = y1 + gradient * (xend - x1);
				xgap = fpart(x1 + 0.5);
				auto xpxl2 = xend; //this will be used in the main loop
				auto ypxl2 = ipart(yend);

				plot(ypxl2, xpxl2, rfpart(yend) * xgap);
				plot(ypxl2 + 1, xpxl2, fpart(yend) * xgap);

				// main loop

				for (auto x = xpxl1 + 1; x < xpxl2; ++x)
				{
					plot(ipart(intery), x, rfpart(intery));
					plot(ipart(intery) + 1, x, fpart(intery));
					intery = intery + gradient;

				}
			}
			else
			{ // not steep.
				if (x0 > x1)
				{
					std::swap(x0, x1);
					std::swap(y0, y1);
				}

				auto dx = x1 - x0;
				auto dy = y1 - y0;
				auto gradient = dy / dx; // float?

				// handle first endpoint
				auto xend = round(x0);
				auto yend = y0 + gradient * (xend - x0);
				auto xgap = rfpart(x0 + 0.5);

				int xpxl1 = xend; //this will be used in the main loop
				int ypxl1 = ipart(yend);

				plot(xpxl1, ypxl1, rfpart(yend) * xgap);
				plot(xpxl1, ypxl1 + 1, fpart(yend) * xgap);
				auto intery = yend + gradient; // first y-intersection for the main loop

				// handle second endpoint

				xend = round(x1);
				yend = y1 + gradient * (xend - x1);
				xgap = fpart(x1 + 0.5);
				auto xpxl2 = xend; //this will be used in the main loop
				auto ypxl2 = ipart(yend);
				plot(xpxl2, ypxl2, rfpart(yend) * xgap);
				plot(xpxl2, ypxl2 + 1, fpart(yend) * xgap);

				// main loop

				for (auto x = xpxl1 + 1; x < xpxl2; ++x)
				{

					plot(x, ipart(intery), rfpart(intery));
					plot(x, ipart(intery) + 1, fpart(intery));
					intery = intery + gradient;

				}
			}
		}
		#undef ipart
		#undef round
		#undef fpart
		#undef rfpart

		struct RGBPixel
		{
			union
			{
				struct
				{
					uint8_t r, g, b;
				};
				uint8_t data[3];
			};
			inline void setColour(RGBPixel other)
			{
				*this = other;
			}
			inline void setColour(uint8_t red, uint8_t green, uint8_t blue)
			{
				this->r = red; this->g = green; this->b = blue;
			}
			inline void setColour(uint32_t colour)
			{
				r = std::uint8_t((colour & 0x00FF0000) >> 16);
				g = std::uint8_t((colour & 0x0000FF00) >> 8);
				b = std::uint8_t((colour & 0x000000FF));
			}
			inline void setColour(float colour)
			{

				setColour(std::uint32_t((colour * 0xFFFFFF)) | 0xFF000000);
			}
			inline void blendOtherChannels(float intensity, uint8_t channel)
			{
				auto p1 = data + channel + 1 % 2;
				auto p2 = data + channel - 1 % 2;
				*p1 = static_cast<uint8_t>((*p1 + ((0xFF - *p1) >> 1)) * intensity);
				*p2 = static_cast<uint8_t>((*p2 + ((0xFF - *p2) >> 1)) * intensity);
			}
			inline void blend(float intensity, uint8_t channel)
			{
				auto p1 = data + channel;
				*p1 = static_cast<uint8_t>((*p1 + ((0xFF - *p1) >> 1)) * intensity);
			}
			inline void blendOtherChannels(uint8_t channel)
			{
				auto p1 = data + channel + 1 % 2;
				auto p2 = data + channel - 1 % 2;
				*p1 = *p1 + ((0xFF - *p1) >> 1);
				*p2 = *p2 + ((0xFF - *p2) >> 1);
			}
			inline void blend(uint8_t channel)
			{
				auto p1 = data + channel;
				*p1 = *p1 + ((0xFF - *p1) >> 1);
			}
		};

		enum class ComponentOrder
		{
			RGB,
			BGR,
			ARGB,
			RGBA,
			BGRA,
			#ifdef CPL_BIGENDIAN
			Native = ARGB,
			#else
			Native = BGRA,
			#endif
			OpenGL = RGBA
		};

		template<ComponentOrder>
		union PixelData;

		template<>
		union PixelData<ComponentOrder::Native>
		{
			typedef std::uint8_t ComponentType;
			struct
			{
				ComponentType b, g, r, a;
			};
			uint32_t p;
			ComponentType data[4];
		};

		template<>
		union PixelData<ComponentOrder::OpenGL>
		{
			typedef std::uint8_t ComponentType;
			struct
			{
				ComponentType r, g, b, a;
			};
			uint32_t p;
			ComponentType data[4];
		};

		inline GLenum orderToGlFormat(ComponentOrder order)
		{
			switch (order)
			{
			case ComponentOrder::RGB: return GL_RGB;
			case ComponentOrder::BGR: return GL_BGR_EXT;
			case ComponentOrder::RGBA: return GL_RGBA;
			case ComponentOrder::BGRA: return GL_BGRA_EXT;
			}

			CPL_RUNTIME_EXCEPTION("unknown pixel mapping order");
		}

		/// <summary>
		/// Unpremultiplied 32-bit ARGB pixel consisting of unsigned bytes, in correct endianness.
		/// No payload.
		/// </summary>
		template<ComponentOrder order>
		struct UPixel
		{
			typedef typename PixelData<order>::ComponentType ComponentType;
			PixelData<order> pixel;

			UPixel()
			{
			}

			static constexpr UPixel rounded(GLfloat r, GLfloat g, GLfloat b, GLfloat a) noexcept
			{
				UPixel ret;
				ret.pixel.a = static_cast<ComponentType>(a * std::numeric_limits<ComponentType>::max() + 0.5f);
				ret.pixel.r = static_cast<ComponentType>(r * std::numeric_limits<ComponentType>::max() + 0.5f);
				ret.pixel.g = static_cast<ComponentType>(g * std::numeric_limits<ComponentType>::max() + 0.5f);
				ret.pixel.b = static_cast<ComponentType>(b * std::numeric_limits<ComponentType>::max() + 0.5f);

				return ret;
			}

			UPixel(std::uint32_t pixel) noexcept
			{
				this->pixel.p = pixel;
			}

			UPixel(std::uint8_t a, std::uint8_t r, std::uint8_t b, std::uint8_t g) noexcept
			{
				pixel.a = a;
				pixel.r = r;
				pixel.g = g;
				pixel.b = b;
			}

			#ifdef CPL_JUCE
			UPixel(juce::PixelARGB pa)
			{
				pa.unpremultiply();
				pixel.a = pa.getAlpha();
				pixel.r = pa.getRed();
				pixel.g = pa.getGreen();
				pixel.b = pa.getBlue();
			}

			UPixel(const juce::Colour & c)
				: UPixel(c.getPixelARGB())
			{

			}
			#endif
			/// <summary>
			/// Saturated addition
			/// </summary>
			/// <param name="other"></param>
			/// <returns></returns>
			UPixel operator + (const UPixel & other) const noexcept
			{

				auto newp = _mm_adds_epu8(_mm_set1_epi32(pixel.p), _mm_set1_epi32(other.pixel.p));
				#ifdef CPL_MSVC
				return newp.m128i_u32[0];
				#else
				return newp[0];
				#endif
			}

			/// <summary>
			/// Saturated addition
			/// </summary>
			/// <param name="other"></param>
			/// <returns></returns>
			UPixel & operator += (const UPixel & other) noexcept
			{

				auto newp = _mm_adds_epu8(_mm_set1_epi32(pixel.p), _mm_set1_epi32(other.pixel.p));

				#ifdef CPL_MSVC
				pixel.p = newp.m128i_u32[0];
				#else
				pixel.p = newp[0];
				#endif
				return *this;
			}

			UPixel operator * (float scale) const noexcept
			{
				UPixel ret(*this);

				ret.pixel.a = static_cast<ComponentType>(scale * ret.pixel.a);
				ret.pixel.r = static_cast<ComponentType>(scale * ret.pixel.r);
				ret.pixel.g = static_cast<ComponentType>(scale * ret.pixel.g);
				ret.pixel.b = static_cast<ComponentType>(scale * ret.pixel.b);

				return ret;
			}

			template<typename T, ComponentOrder otherOrder>
			typename std::enable_if<std::is_floating_point<T>::value, UPixel>::type lerp(UPixel<otherOrder> other, T value)
			{
				UPixel ret(*this);

				ComponentType scale = static_cast<ComponentType>(std::numeric_limits<ComponentType>::max() * value + static_cast<T>(0.5));
				ComponentType invScale = std::numeric_limits<ComponentType>::max() - scale;
				ret.pixel.a = (0x80 + pixel.a * invScale + other.pixel.a * scale) >> 8;
				ret.pixel.r = (0x80 + pixel.r * invScale + other.pixel.r * scale) >> 8;
				ret.pixel.g = (0x80 + pixel.g * invScale + other.pixel.g * scale) >> 8;
				ret.pixel.b = (0x80 + pixel.b * invScale + other.pixel.b * scale) >> 8;

				return ret;
			}

			static GLenum glFormat() noexcept
			{
				return orderToGlFormat(order);
			}

			#ifdef CPL_JUCE
			juce::Colour toJuceColour() const noexcept
			{
				return juce::Colour(pixel.r, pixel.g, pixel.b, pixel.a);
			}
			#endif
		};


		template<ComponentOrder to, ComponentOrder from>
		inline UPixel<to> component_cast(const UPixel<from> & other)
		{
			UPixel<to> ret;
			ret.pixel.a = other.pixel.a;
			ret.pixel.r = other.pixel.r;
			ret.pixel.g = other.pixel.g;
			ret.pixel.b = other.pixel.b;
			return ret;
		}

	}; // Graphics
}; // cpl
#endif