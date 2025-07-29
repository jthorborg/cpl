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

	file:Resources.cpp

		Implementation of Resources.h

*************************************************************************************/

#include "Resources.h"
#include "Misc.h"

namespace cpl
{
	std::mutex CResourceManager::loadMutex;
	std::atomic<CResourceManager *> CResourceManager::internalResourceInstance {nullptr};

	/*********************************************************************************************

	 CImage

	 *********************************************************************************************/
	CImage::CImage(std::string inPath)
		: path(std::move(inPath))
	{

	}

	CImage::CImage()
	{
	}

	void CImage::setPath(std::string inPath)
	{
		path = std::move(inPath);

	}

	bool CImage::load()
	{
		juce::File f(path);

		// handle scalable vector graphics
		if (f.getFileExtension() == ".svg")
		{
			internalImage = {};

			if (auto element = juce::XmlDocument::parse(f); element)
			{
				drawableImage = juce::Drawable::createFromSVG(*element);
				return true;
			}
			else
			{
				drawableImage.reset(new juce::DrawableImage());
			}
		}
		else
		{
			internalImage = juce::ImageFileFormat::loadFrom(f);

			if (internalImage.isValid()) {
				drawableImage.reset(new juce::DrawableImage(internalImage));
				return true;
			}
			else
			{
				// set a default image?
				drawableImage.reset(new juce::DrawableImage());
			}
		}
		return false;
	}

	juce::Image & CImage::getImage()
	{
		return internalImage;

	}

	juce::Drawable * CImage::getDrawable()
	{
		return drawableImage.get();
	}

	CImage::~CImage()
	{
	}
	/*********************************************************************************************

	 CResourceManager

	 *********************************************************************************************/

	CImage * CResourceManager::loadResource(const std::string_view name)
	{
		auto it = resources.find(name);
		if (it != resources.end())
		{
			return &it->second;
		}


		std::string dir = Misc::DirectoryPath() + "/resources/";

		std::string key { name };

		auto & image = resources[key];
		std::string path = (dir + key);
		image.setPath(path);
		if (!image.load())
		{
			Misc::MsgBox(
				"Error loading resource " + path + ":\n" + GetLastOSErrorMessage() + "\n" + 
				"Perhaps you didn't include the folder the plugin arrived in?", 
				programInfo.name + " error!", 
				Misc::MsgIcon::iStop
			);
			CPL_BREAKIFDEBUGGED();
			return nullptr;
		}

		return &image;
	}

	CResourceManager::CResourceManager()
	{
		defaultImage.load();
	};

	CResourceManager::~CResourceManager()
	{
		internalResourceInstance = nullptr;
	}

	std::unique_ptr<juce::Drawable> CResourceManager::createDrawable(const std::string_view name)
	{
		std::lock_guard<std::mutex> lock(loadMutex);
		CImage * resource = loadResource(name);

		if (!resource)
		{
			#ifdef CPL_THROW_ON_NO_RESOURCE
			CPL_RUNTIME_EXCEPTION(std::string("Resource ") + name + " was not found. Compile without CPL_THROW_ON_NO_RESOURCE to remove this exception.");
			#endif
			resource = &defaultImage;
		}

		return std::unique_ptr<juce::Drawable>(resource->getDrawable()->createCopy());
	}

	juce::Image CResourceManager::getImage(const std::string_view name)
	{
		std::lock_guard<std::mutex> lock(loadMutex);
		CImage * resource = loadResource(name);
		if (!resource)
		{
			#ifdef CPL_THROW_ON_NO_RESOURCE
			CPL_RUNTIME_EXCEPTION(std::string("Resource ") + name + " was not found. Compile without CPL_THROW_ON_NO_RESOURCE to remove this exception.");
			#endif
			resource = &defaultImage;
		}

		return resource->getImage();

	}

	CResourceManager & CResourceManager::instance()
	{
		auto instance = internalResourceInstance.load(std::memory_order_acquire);

		if (instance == nullptr)
		{
			std::lock_guard<std::mutex> lock(loadMutex);

			instance = internalResourceInstance.load(std::memory_order_acquire);
			if (instance == nullptr)
			{
				internalResourceInstance.store(new CResourceManager(), std::memory_order_release);
			}
		}

		return *(instance ? instance : internalResourceInstance.load(std::memory_order_acquire));
	}

};
