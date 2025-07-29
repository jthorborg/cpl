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

	file:SubpixelRendering.h

		Utilities and types needed for rendering subpixel graphics.

 *************************************************************************************/

#ifndef _COPENGLIMAGE_H
#define _COPENGLIMAGE_H
#include "../Common.h"
#include "OpenGLRendering.h"
#include "../Mathext.h"

namespace cpl
{
	namespace OpenGLRendering
	{
		class COpenGLImage
		{
			static const GLenum oglFormat = GL_RGBA;
			static const juce::Image::PixelFormat juceFormat = juce::Image::PixelFormat::ARGB;

		public:

			class OpenGLImageDrawer;
			friend class OpenGLImageDrawer;

			class OpenGLImageDrawer : public COpenGLStack::Rasterizer
			{

			public:

				OpenGLImageDrawer(COpenGLImage & img, COpenGLStack & s)
					: Rasterizer(s), image(img)
				{
					s.enable(GL_TEXTURE_2D);

					glPushMatrix();

					glTranslatef(-1, -1, 0);
					glScalef(2.0 / (image.width), 2.0 / (image.height), 1.0);

					glGetIntegerv(GL_MATRIX_MODE, &matrixMode);
					glMatrixMode(GL_TEXTURE);
					glPushMatrix();
					glScalef(1.0 / image.textureWidth, 1.0 / image.textureHeight, 1.0);


					img.bind();
					glBegin(GL_QUADS);
					glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
				}

				inline void setColour(const juce::Colour & colour)
				{
					glColor4f(colour.getFloatRed(), colour.getFloatGreen(), colour.getFloatBlue(), colour.getFloatAlpha());
				}

				/// <summary>
				/// Draws the image pixel-perfect at x and y, with x-offset being a value between 0.
				/// Values over 0 wraps x around, virtually using the image as a circular buffer.
				/// </summary>
				inline void drawCircular(float xoffset)
				{
					return drawCircular(static_cast<std::size_t>(xoffset * image.width));
				}

				/// <summary>
				/// Draws the image pixel-perfect at x and y, with x-offset being a value between 0 and the image size.
				/// Values over image size wraps x around, virtually using the image as a circular buffer.
				/// </summary>
				inline void drawCircular(std::size_t xoffset)
				{
					GLint
						height = static_cast<GLint>(image.height),
						width = static_cast<GLint>(image.width),
						offset = static_cast<GLint>(xoffset);

					glTexCoord2i(0, 0);				glVertex2i(width - offset, 0);
					glTexCoord2i(0, height);		glVertex2i(width - offset, height);
					glTexCoord2i(offset, height);	glVertex2i(width, height);
					glTexCoord2i(offset, 0);		glVertex2i(width, 0);

					glTexCoord2i(offset, 0);		glVertex2i(0, 0);
					glTexCoord2i(offset, height);	glVertex2i(0, height);
					glTexCoord2i(width, height);	glVertex2i(width - offset, height);
					glTexCoord2i(width, 0);			glVertex2i(width - offset, 0);


				}

				~OpenGLImageDrawer()
				{
					glEnd();
					image.unbind();
					glPopMatrix();
					glMatrixMode(matrixMode);

					glPopMatrix();
				}

			private:
				int matrixMode;
				COpenGLImage & image;
			};

			COpenGLImage()
				:
				preserveAcrossContexts(false), height(), width(), textureHeight(), textureWidth(), textureID(),
				fillColour(juce::Colours::black)
			{

			}

			COpenGLImage(std::size_t w, std::size_t h)
				: preserveAcrossContexts(false)
			{
				resize(w, h, false);
			}

			std::size_t getWidth() const noexcept { return width; }
			std::size_t getHeight() const noexcept { return height; }

			bool loadImage(const juce::Image & oldContents)
			{
				if (oldContents.isNull())
					return false;

				/*if (!internalResize(oldContents.getWidth(), oldContents.getHeight()))
					return false;

				const bool flip = true;
				const bool offset = false;
				/*juce::Image newContents(juce::Image::RGB, textureWidth, textureHeight, false);
				{
					juce::Graphics g(newContents);
					g.setOpacity(1.0f);
					g.fillAll(Colours::blue);
					// copy and rescale the subsection onto the new image

					g.drawImage(oldContents,
						0, offset ? textureHeight - height : 0, width, height,
						0, 0, oldContents.getWidth(), oldContents.getHeight());
				}*/

				loadImageInternal(oldContents);

				return true;
			}

			void createEmptyImage()
			{
				if (fillColour.getPixelARGB().getInARGBMemoryOrder() == 0) // background colour is black.
				{
					juce::Image newContents(juceFormat, static_cast<int>(textureWidth), static_cast<int>(textureHeight), true);
					loadImageInternal(newContents);
				}
				else
				{
					juce::Image newContents(juceFormat, static_cast<int>(textureWidth), static_cast<int>(textureHeight), false);
					{
						juce::Graphics g(newContents);
						g.fillAll(fillColour);
					}
					loadImageInternal(newContents);
				}

			}

			/// <summary>
			/// Loads the stored image onto the context, and deletes the memory-mapped image.
			/// Context needs to be active.
			/// </summary>
			void load()
			{
				loadImageInternal(currentContents);
				currentContents = juce::Image();
			}

			/// <summary>
			/// Offloads the texture into a memory-stored image, and deletes it.
			/// Context needs to be active.
			/// </summary>
			bool offload()
			{
				if (hasContent())
				{
					bind();

					if (!transferToMemory())
						return false;
					// release the texture.
					releaseTexture();
					return true;
				}
				return false;
			}

			/// <summary>
			/// Deletes all resources and cleans up textures.
			/// Context needs to be active.
			/// </summary>
			void release()
			{
				currentContents = juce::Image();
				releaseTexture();
			}

			void releaseTexture()
			{
				if (textureID != 0)
				{
					glDeleteTextures(1, &textureID);

					textureID = 0;
					//width = 0;
					//height = 0;
				}
			}




			/// <summary>
			/// 'Zooms' in/out vertically. Needs the active context.
			/// </summary>
			bool scaleTextureVertically(float amount)
			{
				if (!transferToMemory())
					return false;


				float upscale = std::min(1.0f, amount);
				float downscale = std::max(1.0f, amount);
				float destHeight = height * upscale;
				float sourceHeight = height * (2.0f - downscale);
				float sourceY = (height - sourceHeight) * 0.5f;
				float destY = (height - destHeight) * 0.5f;

				juce::Image upload(juceFormat, static_cast<int>(width), static_cast<int>(height), false);
				{
					juce::Graphics g(upload);
					g.fillAll(fillColour);
					g.drawImage(currentContents,
						0, static_cast<int>(destY), static_cast<int>(width), static_cast<int>(destHeight),
						0, static_cast<int>(sourceY), static_cast<int>(width), static_cast<int>(sourceHeight),
						false);
				}

				loadImage(upload);

				return true;
			}

			bool freeLinearVerticalTranslation(cpl::Utility::Bounds<double> oldRect, cpl::Utility::Bounds<double> newRect)
			{
				if (hasContent() && !transferToMemory())
					return false;
				auto oldHeight = std::abs(oldRect.bottom - oldRect.top);

				if (oldHeight == 0.0)
					CPL_RUNTIME_EXCEPTION("Height is 0!");

				auto topDiff = (oldRect.top - newRect.top) / oldHeight;
				auto botDiff = (newRect.bottom - oldRect.bottom) / oldHeight;

				// y-offset into image
				auto sourceTop = std::abs(std::min(0.0, topDiff * height));
				auto destTop = std::abs(std::max(0.0, topDiff * height));
				auto sourceBot = height - std::abs(std::min(0.0, botDiff * height));
				auto destBot = height - std::abs(std::max(0.0, botDiff * height));

				auto destHeight = destBot - destTop;
				auto sourceHeight = sourceBot - sourceTop;


				auto r = [](double in) { return cpl::Math::round<int, double>(in); };
				juce::Image upload(juceFormat, static_cast<int>(width), static_cast<int>(height), false);
				{
					juce::Graphics g(upload);
					g.setImageResamplingQuality(juce::Graphics::ResamplingQuality::mediumResamplingQuality);
					g.setOpacity(1.0f);
					g.fillAll(fillColour);
					g.drawImage(currentContents,
						0, r(destTop), r(width), r(destHeight),
						0, r(sourceTop), r(width), r(sourceHeight),
						false);
				}

				loadImage(upload);

				return true;
			}

			virtual bool resize(std::size_t newWidth, std::size_t newHeight, bool copyOldContents = true)
			{
				std::size_t newActualWidth(newWidth), newActualHeight(newHeight);

				if (textureID != 0 && (newWidth == width && newHeight == height))
					return true;

				if (newWidth == 0 || newHeight == 0)
					return false;

				if (!juce::isPowerOfTwo(newWidth) || !juce::isPowerOfTwo(newHeight))
				{
					newActualWidth = Math::nextPow2Inc(newWidth);
					newActualHeight = Math::nextPow2Inc(newHeight);
				}

				/*// maybe too large?
				if (!text.isValidSize(newActualWidth, newActualHeight))
					return false;*/

				if (CPL_DEBUGCHECKGL())
					return false;

				if (copyOldContents && textureID != 0)
				{
					// copy the contents to memory.
					transferToMemory();
				}
				if (CPL_DEBUGCHECKGL())
					return false;

				// changes sizes, so the new load will rescale it.
				width = newWidth; height = newHeight; textureWidth = newActualWidth; textureHeight = newActualHeight;

				// copy new image
				if (currentContents.isNull())
					createEmptyImage();
				else
					loadImage(currentContents);

				return glGetError() == GL_NO_ERROR;
			}

			/// <summary>
			/// Copies the array of RGB pixels as unsigned bytes into the x-specified column of the texture.
			/// The input vector shall support []-operator with contigous access, and must have a length of
			/// height() * 3 unsigned chars.
			/// </summary>
			template<typename PixelVector, bool doBind = true>
			inline void updateSingleColumn(int x, PixelVector & pixels, GLenum format = GL_RGB)
			{
				if (doBind)
					bind();

				CPL_DEBUGCHECKGL();

				glTexSubImage2D(GL_TEXTURE_2D, 0, x, 0,
					1, static_cast<int>(height), format, GL_UNSIGNED_BYTE, &pixels[0]);

				CPL_DEBUGCHECKGL();

				if (doBind)
					unbind();
			}

			GLuint getTextureID()
			{
				return textureID;
			}

			void bind()
			{
				if (textureID == 0)
				{
					CPL_RUNTIME_EXCEPTION("Invalid texture.");
				}
				else
				{
					glBindTexture(GL_TEXTURE_2D, textureID);
				}
			}

			void unbind()
			{
				if (textureID == 0)
				{
					CPL_RUNTIME_EXCEPTION("Invalid texture.");
				}
				else
				{
					glBindTexture(GL_TEXTURE_2D, 0);
				}
			}

			/// <summary>
			/// Sets the colour to fill areas with no content.
			/// </summary>
			/// <param name="c"></param>
			/// <returns></returns>
			void setFillColour(juce::Colour c) noexcept
			{
				using namespace cpl::GraphicsND;

				UPixel<ComponentOrder::Native> host(c);


				fillColour = component_cast<ComponentOrder::OpenGL>(host).toJuceColour();
			}

			bool hasContent() const noexcept
			{
				return textureID != 0;
			}

		protected:


			virtual void loadImageInternal(const juce::Image & oldContents)
			{
				if (oldContents.isNull())
					return;
				if (!oldContents.isARGB())
					CPL_RUNTIME_EXCEPTION("Image to-be-loaded isn't ARGB!");

				std::size_t imgWidth = oldContents.getWidth();
				std::size_t imgHeight = oldContents.getHeight();


				// can copy image without problems.
				if (imgWidth == width && imgHeight == height)
				{
					juce::Image::BitmapData bmp(oldContents, juce::Image::BitmapData::readOnly);
					transferToOpenGL(static_cast<int>(width), static_cast<int>(height), bmp.data, oglFormat);
				}
				else
				{
					// needs to be rescaled.
					juce::Image rescaled(juceFormat, static_cast<int>(width), static_cast<int>(height), false);
					{
						juce::Graphics g(rescaled);
						g.setOpacity(1.0f);
						g.fillAll(fillColour);
						g.setImageResamplingQuality(juce::Graphics::mediumResamplingQuality);
						g.drawImage(oldContents,
							0, 0, static_cast<int>(width), static_cast<int>(height),
							0, 0, static_cast<int>(imgWidth), static_cast<int>(imgHeight),
							false);
					}
					juce::Image::BitmapData bmp(rescaled, juce::Image::BitmapData::readOnly);
					transferToOpenGL(static_cast<int>(width), static_cast<int>(height), bmp.data, oglFormat);
				}
			}

			virtual bool transferToMemory()
			{
				// copy texture into memory



				bind();

				juce::Image offloaded(juceFormat, static_cast<int>(textureWidth), static_cast<int>(textureHeight), false);
				{
					juce::Image::BitmapData data(offloaded, juce::Image::BitmapData::readWrite);

					CPL_DEBUGCHECKGL();

					//glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

					Texture::Copy2DTextureToMemory(textureID, data.data, textureWidth * textureHeight, oglFormat, GL_UNSIGNED_BYTE);

					if (CPL_DEBUGCHECKGL())
						return false;
				}

				// chop off irrelvant sections.
				// can use a memory copy operation here instead, but this is fail safe.
				// can also directly assign if width == actualWidth and height == actualHeight
				currentContents = juce::Image(juceFormat, static_cast<int>(width), static_cast<int>(height), false);
				{
					#pragma message cwarn("Needs to take the circular position into account, such that images doesn't wrap around. This requires this class to know about the circular position, though.")
					juce::Graphics g(currentContents);
					g.setOpacity(1.0f);
					g.fillAll(fillColour);
					g.drawImage(offloaded,
						0, 0, static_cast<int>(width), static_cast<int>(height),
						0, 0, static_cast<int>(width), static_cast<int>(height),
						false);
				}
				return true;
			}

			virtual void transferToOpenGL(const int w, const int h, const void* pixels, GLenum type)
			{

				bool topLeft = false;

				if (textureID == 0)
				{
					CPL_DEBUGCHECKGL();
					glGenTextures(1, &textureID);
					glBindTexture(GL_TEXTURE_2D, textureID);
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
					CPL_DEBUGCHECKGL();
				}
				else
				{
					glBindTexture(GL_TEXTURE_2D, textureID);
					CPL_DEBUGCHECKGL();
				}

				//glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
				CPL_DEBUGCHECKGL();

				auto nwidth = cpl::Math::nextPow2Inc(w);
				auto nheight = cpl::Math::nextPow2Inc(h);


				if (nwidth != w || nheight != h)
				{
					auto bounds = Texture::GetBounds(textureID);
					if (bounds.first != nwidth || bounds.second != nheight)
					{
						// resize image
						glTexImage2D(GL_TEXTURE_2D, 0, oglFormat,
							nwidth, nheight, 0, type, GL_UNSIGNED_BYTE, nullptr);
					}


					glTexSubImage2D(GL_TEXTURE_2D, 0, 0, topLeft ? static_cast<int>(height - h) : 0, w, h,
						type, GL_UNSIGNED_BYTE, pixels);
					CPL_DEBUGCHECKGL();
				}
				else
				{
					glTexImage2D(GL_TEXTURE_2D, 0, oglFormat,
						w, h, 0, type, GL_UNSIGNED_BYTE, pixels);
					CPL_DEBUGCHECKGL();
				}


			}

			// the size of the image
			std::size_t width, height;
			// the actual size, may be larger than width and height (next power of two)
			std::size_t textureWidth, textureHeight;
			bool preserveAcrossContexts;
			//juce::OpenGLTexture text;
			juce::Image currentContents;
			juce::Colour fillColour;
			GLuint textureID;

		};
	};
}; // {} cpl
#endif
