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
 
	file:OpenGLRasterizers.h
 
		Some rasterizer primitives

 *************************************************************************************/

#ifndef CPL_OPENGLRASTERIZERS_H
	#define CPL_OPENGLRASTERIZERS_H
	#include "../Common.h"
	#include "OpenGLRendering.h"

	namespace cpl
	{
		namespace OpenGLRendering
		{

			class ImageDrawer
			:
				public COpenGLStack::Rasterizer
			{
			public:
				ImageDrawer(COpenGLStack & parentStack, const juce::OpenGLTexture & texture)
					: Rasterizer(parentStack)
				{
					texture.bind();
					glBegin(GL_QUADS);
					setColour(juce::Colours::white);
				}

				inline void drawAt(OpenGLRendering::Vertex x1, OpenGLRendering::Vertex y1, OpenGLRendering::Vertex x2, OpenGLRendering::Vertex y2)
				{
					glTexCoord2f(0.0f, 0.0f); glVertex3f(0.0f, 0.0f, 0.0f);
					glTexCoord2f(0.0f, 1.0f); glVertex3f(0.0f, 1.0f, 0.0f);
					glTexCoord2f(1.0f, 1.0f); glVertex3f(1.0f, 1.0f, 0.0f);
					glTexCoord2f(1.0f, 0.0f); glVertex3f(1.0f, 0.0f, 0.0f);
				}

				inline void setColour(const juce::Colour & colour)
				{
					glColor4f(colour.getFloatRed(), colour.getFloatGreen(), colour.getFloatBlue(), colour.getFloatAlpha());
				}

				inline void drawAt(juce::Rectangle<OpenGLRendering::Vertex> area)
				{
					glTexCoord2f(0, 0); glVertex3f(area.getX(), area.getY(), 0);
					glTexCoord2f(0, 1); glVertex3f(area.getX(), area.getY() + area.getHeight(), 0);
					glTexCoord2f(1, 1); glVertex3f(area.getX() + area.getWidth(), area.getY() + area.getHeight(), 0);
					glTexCoord2f(1, 0); glVertex3f(area.getX() + area.getWidth(), area.getY(), 0);
				}

				~ImageDrawer()
				{
					glEnd();
					glBindTexture(GL_TEXTURE_2D, 0);
				}
			protected:
				
			};

			template<std::size_t vertexBufferSize = 1024>			
				class PrimitiveDrawer
				:
					public COpenGLStack::Rasterizer
				{

				public:

					static_assert((vertexBufferSize & (vertexBufferSize - 1)) == 0, "Vertex buffer size of primitive drawer must be a power of two");

					PrimitiveDrawer(COpenGLStack & parentStack, GLFeatureType primitive)
						: Rasterizer(parentStack)
						, primitive(primitive)
						, vertexIndex(0)
						, colourIndex(0)
						, vertexPointer(reinterpret_cast<float *>(&vertices))
						, colourPointer(reinterpret_cast<std::uint8_t *>(&colours))
					{
						glEnableClientState(GL_COLOR_ARRAY);
						glEnableClientState(GL_VERTEX_ARRAY);
					}

					inline void addVertex(OpenGLRendering::Vertex x, OpenGLRendering::Vertex y, OpenGLRendering::Vertex z)
					{
						if (vertexIndex == vertexBufferSize)
							rasterizeBuffers();

						auto base = vertexPointer + vertexIndex * vertexStride;

						base[0] = x;
						base[1] = y;
						base[2] = z;

						vertexIndex++;
					}

					inline void addColour(ColourType r, ColourType g, ColourType b, ColourType a = (ColourType)1) noexcept
					{
						addColour(static_cast<std::uint8_t>(r / 255.f), static_cast<std::uint8_t>(g / 255.f), static_cast<std::uint8_t>(b / 255.f), static_cast<std::uint8_t>(a / 255.f));
					}

					inline void addColour(const juce::Colour & c) noexcept
					{
						addColour(c.getRed(), c.getGreen(), c.getBlue(), c.getAlpha());
					}

					template<cpl::GraphicsND::ComponentOrder order>
					inline void addColour(cpl::GraphicsND::UPixel<order> colour) noexcept
					{
						addColour(colour.pixel.r, colour.pixel.g, colour.pixel.b, colour.pixel.a);
					}

					inline void addColour(std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a) noexcept
					{
						colourIndex &= vertexBufferSize - 1;

						auto base = colourPointer + colourIndex * colourStride;

						base[0] = r;
						base[1] = g;
						base[2] = b;
						base[3] = a;

						colourIndex++;
					}

					~PrimitiveDrawer()
					{
						rasterizeBuffers();
						glDisableClientState(GL_COLOR_ARRAY);
						glDisableClientState(GL_VERTEX_ARRAY);
					}

					void rasterizeBuffers()
					{
						if (vertexIndex == 0)
						{
							colourIndex = 0;
							return;
						}

						if (colourIndex != vertexIndex)
						{
							const auto currentIndex = ((colourIndex - 1) & (vertexBufferSize - 1)) * colourStride;
							const auto r = colourPointer[currentIndex];
							const auto g = colourPointer[currentIndex + 1];
							const auto b = colourPointer[currentIndex + 2];
							const auto a = colourPointer[currentIndex + 3];

							for (; colourIndex < vertexIndex; ++colourIndex)
							{
								const auto index = colourPointer + colourIndex * colourStride;
								colourPointer[currentIndex] = r;
								colourPointer[currentIndex + 1] = g;
								colourPointer[currentIndex + 2] = b;
								colourPointer[currentIndex + 3] = a;
							}
						}

						glColorPointer(4, GL_UNSIGNED_BYTE, 0, colourPointer);
						glVertexPointer(3, GL_FLOAT, 0, vertexPointer);

						glDrawArrays(primitive, 0, vertexIndex);

						colourIndex = vertexIndex = 0;
					}

				protected:
					static constexpr std::size_t colourStride = 4;
					static constexpr std::size_t vertexStride = 3;

					GLFeatureType primitive;
					std::size_t vertexIndex, colourIndex;
					float * vertexPointer;
					std::uint8_t * colourPointer;
					typename std::aligned_storage<vertexBufferSize * sizeof(float) * 3, 32>::type vertices;
					typename std::aligned_storage<vertexBufferSize * sizeof(char) * 4, 32>::type colours;

					//CPL_ALIGNAS(32) Vertex vertices[vertexBufferSize * dimensions];
				};


			template<std::size_t vertexBufferSize = 128>
				class RectangleDrawer2D
				:
					public COpenGLStack::Rasterizer,
					public juce::Rectangle<GLfloat>
				{
				public:

					RectangleDrawer2D(COpenGLStack & parentStack)
						: Rasterizer(parentStack)
					{
						glGetFloatv(GL_LINE_WIDTH, &oldLineSize);
					}


					~RectangleDrawer2D()
					{
						glLineWidth(oldLineSize);
					}
					inline void renderOutline(GLfloat outlineSize = 1.f)
					{
						glLineWidth(outlineSize);
						glBegin(GL_LINE_LOOP);
						applyColour();
						glVertex2f(getX(), getY());
						glVertex2f(getX() + getWidth(), getY());
						glVertex2f(getX() + getWidth(), getY() + getHeight());
						glVertex2f(getX(), getY() + getHeight());

						glEnd();
					}

					inline void fill()
					{
						glBegin(GL_POLYGON);
						applyColour();
						glVertex2f(getX(), getY());
						glVertex2f(getX() + getWidth(), getY());
						glVertex2f(getX() + getWidth(), getY() + getHeight());
						glVertex2f(getX(), getY() + getHeight());
						glEnd();
					}

					inline void setColour(const juce::Colour & c)
					{
						red = c.getFloatRed(); green = c.getFloatGreen(); blue = c.getFloatBlue(); alpha = c.getFloatAlpha();
					}
					inline void setColour(ColourType r, ColourType g, ColourType b, ColourType a = (ColourType)1)
					{
						red = r; green = g; blue = b; alpha = a;
					}
				private: 
					inline void applyColour()
					{
						glColor4f(red, green, blue, alpha);
					}
					GLfloat oldLineSize;
					GLfloat red, green, blue, alpha;
				};

			template<std::size_t vertexBufferSize = 1024>			
				class ConnectedLineDrawer
				:
					public COpenGLStack::Rasterizer
				{

				public:

					ConnectedLineDrawer(COpenGLStack & parentStack)
						: Rasterizer(parentStack), vertexPointer(0)
					{
						glBegin(GL_LINE_STRIP);
					}

					inline void addVertex(OpenGLRendering::Vertex x, OpenGLRendering::Vertex y, OpenGLRendering::Vertex z)
					{
						glVertex3f(x, y, z);
					}

					~ConnectedLineDrawer()
					{
						rasterizeBuffer();
						glEnd();
					}

					void rasterizeBuffer()
					{


					}

				protected:
					std::size_t vertexPointer;
					//CPL_ALIGNAS(32) Vertex vertices[vertexBufferSize * dimensions];
				};


		}; // {} rendering
	}; // {} cpl
#endif
