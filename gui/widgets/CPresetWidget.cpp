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

	file:CPresetWidget.cpp

		Source code for CPresetWidget.h

*************************************************************************************/

#include "CPresetWidget.h"
#include "../../CPresetManager.h"

namespace cpl
{
	CPresetWidget::CPresetWidget(SafeSerializableObject * content, const std::string_view uName, Setup s)
		: CBaseControl(this)
		, name(uName)
		, parent(content)
		, ext(std::string(uName) + "." + programInfo.programAbbr)
		, layoutSetup(s)
		, version(programInfo.version)

	{
		initControls();
		enableTooltip();
		updatePresetList();

		addAndMakeVisible(layout);
	}

	std::string CPresetWidget::presetWithoutExtension(juce::File preset)
	{
		if (preset.existsAsFile() && preset.hasFileExtension(ext.c_str()))
		{
			auto fname = preset.getFileName();
			auto end = fname.lastIndexOfIgnoreCase(ext.c_str());

			auto str = fname.toStdString();
			return{str.begin(), str.begin() + (end == -1 ? fname.length() : (end - 1))};
		}
		return "";
	}

	std::string CPresetWidget::fullPathToPreset(const std::string_view name)
	{
		return CPresetManager::instance().getPresetDirectory() + std::string(name) + "." + ext;
	}

	void CPresetWidget::valueChanged(const CBaseControl * c)
	{
		if (c == &ksavePreset)
		{
			SerializerType serializer(name);
			serializer.getArchiver().setMasterVersion(version);
			parent->serializeObject(serializer.getArchiver(), serializer.getArchiver().getLocalVersion());
			auto dialog = CPresetManager::instance().savePresetAs(serializer, name, 
				[this](const juce::File& savedFile) {
					// update list anyway; user may delete files in dialog etc.
					updatePresetList();
					setDisplayedPreset(savedFile);
				});
			// Dialog is now handled asynchronously

		}
		else if (c == &kloadPreset)
		{
			SerializerType serializer(name);
			auto dialog = CPresetManager::instance().loadPresetAs(serializer, name,
				[this](const juce::File& loadedFile, CCheckedSerializer& loadedData) {
					updatePresetList();
					parent->deserializeObject(loadedData.getBuilder(), loadedData.getBuilder().getLocalVersion());
					setDisplayedPreset(loadedFile);
				});
			// Dialog is now handled asynchronously
		}
		else if (c == &ksaveDefault)
		{
			SerializerType serializer(name);
			serializer.getArchiver().setMasterVersion(version);
			parent->serializeObject(serializer.getArchiver(), serializer.getArchiver().getLocalVersion());
			juce::File location;
			bool result = CPresetManager::instance().savePreset(fullPathToPreset("default"), serializer, location);
			// update list anyway; user may delete files in dialog etc.
			updatePresetList();
			if (result)
			{
				setDisplayedPreset(location);
			}

		}
		else if (c == &kloadDefault)
		{
			SerializerType serializer(name);
			juce::File location;
			bool result = CPresetManager::instance().loadPreset(fullPathToPreset("default"), serializer, location);
			updatePresetList();
			if (result)
			{
				parent->deserializeObject(serializer.getBuilder(), serializer.getBuilder().getLocalVersion());
				setDisplayedPreset(location);
			}
		}
		else if (c == &kpresetList)
		{
			auto presetName = kpresetList.valueFor(kpresetList.getZeroBasedSelIndex());

			if (presetName.size())
			{
				juce::File location;
				SerializerType serializer(name);
				if (CPresetManager::instance().loadPreset(fullPathToPreset(presetName), serializer, location))
				{
					parent->deserializeObject(serializer.getBuilder(), serializer.getBuilder().getLocalVersion());
					setSelectedPreset(location);
				}
			}


		}
	}

	bool CPresetWidget::loadDefaultPreset()
	{
		if (layoutSetup & WithDefault)
		{
			return setSelectedPreset((juce::File)fullPathToPreset("default"));
		}

		return false;
	}

	void CPresetWidget::baseControlValueChanged()
	{
	}

	void CPresetWidget::onObjectDestruction(const ObjectProxy & object)
	{
	}

	const std::string & CPresetWidget::getName() const noexcept
	{
		return name;
	}

	void CPresetWidget::setDisplayedPreset(juce::File location)
	{
		std::string newValue = presetWithoutExtension(location);
		kpresetList.bInterpretAndSet(newValue, true, true);
	}

	bool CPresetWidget::setSelectedPreset(juce::File location)
	{
		std::string newValue = presetWithoutExtension(location);
		return kpresetList.bInterpretAndSet(newValue, false, true);
	}

	const std::vector<std::string>& CPresetWidget::getPresets()
	{
		// TODO: insert return statement here
		CPL_NOTIMPLEMENTED_EXCEPTION();
	}

	void CPresetWidget::updatePresetList()
	{
		auto & presetList = CPresetManager::instance().getPresets();
		std::vector<std::string> shortList;
		for (auto & preset : presetList)
		{
			auto name = presetWithoutExtension(preset);
			if (name.length())
				shortList.push_back(name);
		}

		kpresetList.setValues(shortList);

	}

	void CPresetWidget::setEmulatedVersion(cpl::Version newVersion)
	{
		version = newVersion;
	}

	void CPresetWidget::initControls()
	{

		kloadPreset.bAddChangeListener(this);
		ksavePreset.bAddChangeListener(this);
		kpresetList.bAddChangeListener(this);
		kloadDefault.bAddChangeListener(this);
		ksaveDefault.bAddChangeListener(this);


		kloadPreset.setSingleText("Load preset...");
		ksavePreset.setSingleText("Save current...");
		kloadDefault.setSingleText("Load default");
		ksaveDefault.setSingleText("Save as default");
		kpresetList.bSetTitle("Preset list");


		bSetDescription("The preset widget allows you to save and load the state of the current local parent view.");
		kloadPreset.bSetDescription("Load a preset from a location.");
		ksavePreset.bSetDescription("Save the current state to a location.");
		kloadDefault.bSetDescription("Load the default preset.");
		ksaveDefault.bSetDescription("Save the current state as the default.");

		if (layoutSetup & WithDefault)
		{
			layout.setSpacesAfterLargestElement(false);
			layout.setXSpacing(layout.getXSpacing() * 3);
			layout.addControl(&ksavePreset, 0, false);
			layout.addControl(&ksaveDefault, 1, false);
			layout.addControl(&kloadPreset, 2, false);
			layout.addControl(&kloadDefault, 3, false);
			layout.addControl(&kpresetList, 0, false);
		}
		else
		{
			layout.addControl(&kpresetList, 0, false);
			layout.addControl(&kloadPreset, 1, false);
			layout.addControl(&ksavePreset, 2, false);
		}


		setSize(layout.getSuggestedSize().first, layout.getSuggestedSize().second);
	}
};
