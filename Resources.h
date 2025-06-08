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

	 file:Resources.h

		Manager for runtime resources, classes for wrapping them, etc.

 *************************************************************************************/

#ifndef CPL_RESOURCES_H
#define CPL_RESOURCES_H

#include "Common.h"
#include <mutex>
#include <memory>
#include "CMutex.h"
#include "Utility.h"
#include <map>


namespace cpl
{


	/*********************************************************************************************

		RAII wrapper around images, loaded at runtime

	*********************************************************************************************/
	class CImage
	{
		std::string path;
		juce::Image internalImage;
		std::unique_ptr<juce::Drawable> drawableImage;

	public:
		CImage(std::string inPath);
		CImage();
		void setPath(std::string inPath);
		bool load();
		juce::Image & getImage();
		juce::Drawable * getDrawable();
		virtual ~CImage();
	};

	/*********************************************************************************************

		Manages all resources used by this program, statically

	*********************************************************************************************/
	class CResourceManager
		:
		public juce::DeletedAtShutdown,
		public Utility::CNoncopyable
	{
	public:
		typedef std::unique_ptr<juce::Drawable> OwnedDrawable;
		OwnedDrawable createDrawable(const std::string_view name);
		juce::Image getImage(const std::string_view name);
		static CResourceManager & instance();

	private:

		CImage * loadResource(const std::string_view name);
		CImage defaultImage;
		static std::atomic<CResourceManager *> internalResourceInstance;
		std::map<std::string, CImage, std::less<>> resources;
		~CResourceManager();
		CResourceManager();
		static std::mutex loadMutex;

	};

	class CVectorResource
	{
	public:

		CVectorResource()
			: oldColour(juce::Colours::black)
		{

		}

		CVectorResource(const std::string_view name)
			: oldColour(juce::Colours::black)
		{
			oldColour = juce::Colours::black;
			associate(name);
		}

		/// <summary>
		/// Does nothing it the fill colour is the same as the current one
		/// </summary>
		void changeFillColour(juce::Colour newColour)
		{
			if (svg.get() && newColour != oldColour)
			{
				svg->replaceColour(oldColour, newColour);
				oldColour = newColour;
			}
		}

		template<typename T>
		void renderImage(juce::Rectangle<T> size, juce::Colour colour = juce::Colour(0, 0, 0), float opacity = 1.f)
		{
			if (!svg.get())
				return;
			// images has to be at least 1 pixel.
			if (size.getWidth() < 1 || size.getHeight() < 1)
				return;

			auto intSize = size.template toType<int>();

			if (intSize != image.getBounds())
				image = juce::Image(image.ARGB, intSize.getWidth(), intSize.getHeight(), true);

			changeFillColour(colour);

			juce::Graphics g(image);

			svg->drawWithin(g, size.withPosition(0, 0).toFloat(), juce::RectanglePlacement::centred, opacity);
		}

		static juce::Image renderSVGToImage(const std::string_view path, juce::Rectangle<int> size, juce::Colour colour = juce::Colour(0, 0, 0), float opacity = 1.f)
		{
			auto resource = CResourceManager::instance().createDrawable(path);

			if (resource.get())
			{
				juce::Image image(juce::Image::ARGB, size.getWidth(), size.getHeight(), true);
				juce::Graphics g(image);
				resource->replaceColour(juce::Colours::black, colour);
				resource->drawWithin(g, size.withPosition(0, 0).toFloat(), juce::RectanglePlacement::centred, opacity);
				return image;
			}
			return {};
		}

		juce::Image & getImage() { return image; }

		juce::Drawable * getDrawable() { return svg.get(); }

		bool associate(std::string_view path)
		{
			svg = CResourceManager::instance().createDrawable(path);
			return !!svg.get();
		}

	private:

		juce::Colour oldColour;
		CResourceManager::OwnedDrawable svg;
		juce::Image image;
	};

};
#endif
