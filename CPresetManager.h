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

	file:CPresetManager.h
		Manages and saves presets, suitable for audio-plugins.

*************************************************************************************/

#ifndef CPL_CPRESETMANAGER_H
#define CPL_CPRESETMANAGER_H

#include "Common.h"
#include "state/CSerializer.h"
#include "CExclusiveFile.h"
#include <vector>
#include <string>
#include <functional>
#include <memory>

namespace cpl
{
	class CPresetManager
	{
	public:

		typedef std::unique_ptr<juce::FileChooser> DialogState;

		static CPresetManager & instance();

		typedef std::function<void(const juce::File&)> FileSavedCallback;
		typedef std::function<void(const juce::File&, const CCheckedSerializer&)> FileLoadedCallback;

		// these functions pops up file selectors
		DialogState savePresetAs(CCheckedSerializer archive, const std::string & uniqueExt = "", FileSavedCallback callback = {});
		DialogState loadPresetAs(CCheckedSerializer builder, const std::string & uniqueExt = "", FileLoadedCallback whenDone = {});

		// these functions saves/loads directly
		bool savePreset(const std::string & name, const ISerializerSystem & serializer, juce::File & location);
		bool loadPreset(const std::string & name, ISerializerSystem & serializer, juce::File & location);
		const std::vector<juce::File> & getPresets();
		bool saveDefaultPreset(const ISerializerSystem & serializer, juce::File & location);
		bool loadDefaultPreset(ISerializerSystem & serializer, juce::File & location);
		std::string getPresetDirectory() const noexcept;
		juce::File getCurrentPreset();

	private:
		std::vector<juce::File> currentPresets;
		CPresetManager();
		~CPresetManager();
	};
};

#endif