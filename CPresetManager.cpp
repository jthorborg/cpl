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

	file:CPresetManager.cpp
		Implementation of CPresetManager.h

*************************************************************************************/

#include "CPresetManager.h"
#include "Misc.h"
#include <vector>
#include <memory>

namespace cpl
{

	auto presetDirectory = []() { return cpl::Misc::DirectoryPath() + "/presets/"; };


	CPresetManager & CPresetManager::instance()
	{
		static CPresetManager ins;
		return ins;
	}

	std::string CPresetManager::getPresetDirectory() const noexcept
	{
		return ::cpl::presetDirectory();
	}

	CPresetManager::DialogState CPresetManager::savePresetAs(CCheckedSerializer archive, const std::string & uniqueExt, FileSavedCallback callback)
	{
		// should we really save empty files?
		if (archive.isEmpty())
			return nullptr;

		std::string extension = uniqueExt.length() ? uniqueExt + "." + programInfo.programAbbr : programInfo.programAbbr;

		auto fileChooser = std::make_unique<juce::FileChooser>(programInfo.name + ": Save preset to a file...",
			juce::File(presetDirectory()),
			#ifdef CPL_UNIXC
			// native dialogs hangs programs on the distros I've tried
			"*." + extension, false);
		#else
			"*." + extension);
		#endif

		fileChooser->launchAsync(
			juce::FileBrowserComponent::saveMode,
			[this, archive, callback, extension](const juce::FileChooser& chooser)
			{
				auto result = chooser.getResult();
				if (result.existsAsFile() || result.getParentDirectory().exists())

				{
					juce::String dotExt = "." + extension;
					bool hasExt = false;
					juce::String tempString = result.getFullPathName();
					juce::String finalString = tempString;

					// OS X handles multiple endings by duplicating them.. litterally.
					// how nice.
					while (tempString.endsWith(dotExt))
					{
						hasExt = true;
						finalString = tempString;
						tempString = tempString.dropLastCharacters(dotExt.length());
					}

					std::string path =
						hasExt ?
						finalString.toStdString() :
						result.withFileExtension(extension.c_str()).getFullPathName().toStdString();

					juce::File location = result.getParentDirectory();
					if (savePreset(path, archive, location))
					{
						if (callback)
							callback(result);
					}
				}
			});

		return std::move(fileChooser);
	}
	CPresetManager::DialogState CPresetManager::loadPresetAs(const std::string & uniqueExt, FileLoadedCallback whenDone)

	{

		std::string extension = uniqueExt.length() ? uniqueExt + "." + programInfo.programAbbr : programInfo.programAbbr;

		auto fileChooser = std::make_unique<juce::FileChooser>(programInfo.name + ": Load preset from a file...",
			juce::File(presetDirectory()),
			#ifdef CPL_MAC
			"*." + programInfo.programAbbr); // it just doesn't work..
		#elif defined(CPL_UNIXC)
			// native dialogs hangs programs on the distros I've tried
			"*." + extension, false);
		#else
			"*." + extension);
		#endif

		fileChooser->launchAsync(
			juce::FileBrowserComponent::openMode,
			[this, builder, whenDone, extension](const juce::FileChooser& chooser)
			{
				auto result = chooser.getResult();
				if (result.existsAsFile())
				{
					// the file chooser can only choose file names with the correct extension,
					// no need to check.
					std::string path = result.getFullPathName().toStdString();
					juce::File location = result.getParentDirectory();

					if (!result.getFileName().contains(extension.c_str()))
					{
						// Warning about extension mismatch could be handled here
					}

					CCheckedSerializer loadedData("temp");
					if (loadPreset(path, loadedData.getBuilder(), location))
					{
						if (whenDone)
							whenDone(result, loadedData);
					}
				}
			});

		return std::move(fileChooser);
	}

	// these functions saves/loads directly
	bool CPresetManager::savePreset(const std::string & path, const ISerializerSystem & archive, juce::File & location)
	{
		CExclusiveFile file;

		if (!file.open(path, file.writeMode))
			return false;

		// clear existing file..
		file.remove();

		if (!file.open(path))
			return false;

		auto content = archive.compile(true);

		if (file.write(content.getBlock(), (std::int64_t)content.getSize()))
		{
			location = path;
			return true;
		}

		return false;
	}
	bool CPresetManager::loadPreset(const std::string & path, ISerializerSystem & builder, juce::File & location)
	{
		try
		{
			CExclusiveFile file;

			if (!file.open(path, file.readMode))
				return false;

			std::vector<std::uint8_t> data;
			std::size_t size = (std::size_t)file.getFileSize();
			data.resize(size);

			if (!file.read(data.data(), size))
				return false;

			builder.clear();
			if (builder.build(WeakContentWrapper(data.data(), size)))
			{
				location = path;
				return true;
			}
		}
		catch (const std::exception & e)
		{
			Misc::MsgBox("Exception loading preset at " + path + ":\n" + e.what(), programInfo.name, Misc::MsgIcon::iStop);
		}
		return false;
	}
	const std::vector<juce::File> & CPresetManager::getPresets()
	{
		currentPresets.clear();

		juce::DirectoryIterator iter(File(presetDirectory()), false, "*." + programInfo.programAbbr);
		while (iter.next())
		{
			currentPresets.push_back(iter.getFile());
		}

		return currentPresets;
	}
	bool CPresetManager::saveDefaultPreset(const ISerializerSystem & archive, juce::File & location)
	{
		return savePreset(presetDirectory() + "default." + programInfo.programAbbr, archive, location);
	}

	bool CPresetManager::loadDefaultPreset(ISerializerSystem & builder, juce::File & location)
	{
		auto path = presetDirectory() + "default." + programInfo.programAbbr;
		if (!loadPreset(path, builder, location))
		{
			auto answer = cpl::Misc::MsgBox(
				"Error loading default preset at:\n" + path + "\n" + GetLastOSErrorMessage() +
				"\nLoad a different preset?",
				programInfo.name + ": Error loading preset...",
				Misc::MsgIcon::iQuestion | Misc::MsgStyle::sYesNoCancel);
			if (answer == Misc::MsgButton::bYes)
			{
				// Since loadPresetAs is now async, we can't easily use it here
				// This would need a different approach or just return false
				return false;
			}
			else
			{
				return false;
			}

		}
		return true;
	}

	CPresetManager::CPresetManager() {}

	CPresetManager::~CPresetManager() {}

};
