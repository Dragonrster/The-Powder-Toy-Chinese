#include "PropWindow.h"
#include <algorithm>
#include <sstream>
#include <string>
#include <vector>
#include "legacy_console.h" // For console_parse_type
#include "misc.h" // For GetToolFromIdentifier
#include "common/Format.h"
#include "common/Translations.h"
#include "interface/Button.h"
#include "interface/Dropdown.h"
#include "interface/Engine.h"
#include "interface/Label.h"
#include "interface/Textbox.h"
#include "simulation/Particle.h"
#include "simulation/Simulation.h" // For console_parse_type
#include "simulation/Tool.h"

#include "gui/dialogs/ErrorPrompt.h"
#include "simulation/elements/LIFE.h"

PropWindow::PropWindow():
	ui::Window(Point(CENTERED, CENTERED), Point(175, 80))
{
#ifndef TOUCHUI
	int buttonHeight = 15;
#else
	int buttonHeight = 25;
	this->Resize(Point(CENTERED, CENTERED), this->GetSize() + Point(0, 25));
#endif
	propTool = dynamic_cast<PropTool*>(GetToolFromIdentifier("DEFAULT_UI_PROPERTY"));

	Label *editPropertyLabel = new Label(Point(5, 3), Point(Label::AUTOSIZE, Label::AUTOSIZE), TrUI("Edit Property:"));
	editPropertyLabel->SetColor(COLRGB(140, 140, 255));
	this->AddComponent(editPropertyLabel);

	std::vector<std::string> options;
	properties = particle::GetProperties();
	for (auto const & property : properties)
		options.push_back(property.Name);

	propertyDropdown = new Dropdown(editPropertyLabel->Below(Point(0, 2)), Point(this->size.X-10, Dropdown::AUTOSIZE), options);
	propertyDropdown->SetCallback([&](unsigned int option) { this->OnPropertyChanged(option); });
	this->AddComponent(propertyDropdown);

	valueTextbox = new Textbox(propertyDropdown->Below(Point(0, 5)), Point(this->size.X-10, propertyDropdown->GetSize().Y), "");
	valueTextbox->SetCallback([&]() {
		this->UpdatePropTool(false);
	});
	this->AddComponent(valueTextbox);
#ifndef TOUCHUI
	FocusComponent(valueTextbox);
#endif

	Button *okButton = new Button(Point(0, this->size.Y - buttonHeight), Point(this->size.X, buttonHeight), TrUI("OK"));
	okButton->SetCloseButton(true);
	okButton->SetCallback([&](int) {
		this->UpdatePropTool(true);
	});
	this->AddComponent(okButton);


	LoadFromPropTool();
}

void PropWindow::OnPropertyChanged(unsigned int option)
{
	selectedProperty = option;
}

void PropWindow::DoKeyPress(int key, int scan, bool repeat, bool shift, bool ctrl, bool alt)
{
	if (key == SDLK_UP)
	{
		if (selectedProperty > 0)
		{
			selectedProperty--;
			propertyDropdown->SetSelectedOption(selectedProperty);
			UpdatePropTool(false);
		}
	}
	else if (key == SDLK_DOWN)
	{
		if (selectedProperty < properties.size() - 1)
		{
			selectedProperty++;
			propertyDropdown->SetSelectedOption(selectedProperty);
			UpdatePropTool(false);
		}
	}
	else if (key == SDLK_RETURN)
	{
		UpdatePropTool(true);
	}
	else
	{
		Window::DoKeyPress(key, scan, repeat, shift, ctrl, alt);
	}
}

void PropWindow::UpdatePropTool(bool close)
{
	propTool->prop = properties[selectedProperty];
	std::string value = valueTextbox->GetText();
	if (!value.empty())
	{
		bool isParsed = ParseValue(value, close);
		if (close)
		{
			if (isParsed)
			{
				propTool->propValue = tempValue;
			}
			propTool->invalidState = !isParsed;
			Close(ui::Confirmed);
		}
		valueTextbox->SetColor(isParsed ? COLRGB(255, 255, 255) : COLRGB(255, 0, 0));
	}
	else
	{
		propTool->invalidState = true;
	}
}

void PropWindow::LoadFromPropTool()
{
	// Parse the property name out of propTool->prop
	std::string propName = propTool->prop.Name;
	auto prop = std::find_if(properties.begin(), properties.end(), [&propName](StructProperty const &p) {
		return p.Name == propName;
	});
	selectedProperty = prop == properties.end() ? 0 : prop - properties.begin();
	propertyDropdown->SetSelectedOption(selectedProperty);

	// If invalid or never set before, don't parse the value
	if (propTool->invalidState)
		return;

	// Parse the value out of propTool->propValue
	std::string value;
	if (prop->Name == "temp")
		value = Format::TemperatureToString(propTool->propValue.Float, globalSim->temperatureScale);
	else if (prop->Type == StructProperty::Float)
		value = Format::NumberToString<float>(propTool->propValue.Float);
	else if (prop->Type == StructProperty::UInteger)
		value = Format::NumberToString<unsigned int>(propTool->propValue.UInteger);
	else
		value = Format::NumberToString<int>(propTool->propValue.Integer);
	valueTextbox->SetText(value);
	valueTextbox->SelectAll();
}

template<typename T>
T PropWindow::ParseNumber(const std::string& num, bool isHex, bool &isParsed)
{
	T value;
	std::stringstream parser;
	if (isHex)
		parser << std::hex;
	parser << num;
	parser >> value;
	if (parser.eof())
		isParsed = true;
	return value;
}

bool PropWindow::ParseInteger(const std::string& value, bool isHex)
{
	bool isParsed = false;
	bool isType = properties[selectedProperty].Name == "type";
	if (properties[selectedProperty].Type == StructProperty::UInteger)
	{
		auto val = ParseNumber<unsigned int>(value, isHex, isParsed);
		if (isParsed)
		{
			PropertyValue propValue;
			propValue.UInteger = val;
			tempValue = propValue;
			return true;
		}
	}
	else
	{
		auto val = ParseNumber<int>(value, isHex, isParsed);
		if (isType && (val < 0 || val >= PT_NUM || !globalSim->elements[val].Enabled))
			return false;
		if (isParsed)
		{
			tempValue = { val };
			return true;
		}
	}
	return false;
}

bool PropWindow::ParseFloat(const std::string& value, float* out, bool isTemp)
{
	if (isTemp)
	{
		try
		{
			float val = Format::StringToTemperature(value, globalSim->temperatureScale);
			*out = val;
			return true;
		}
		catch (const std::exception &e)
		{
			return false;
		}
	}

	bool isParsed = false;
	auto val = ParseNumber<float>(value, false, isParsed);
	if (isParsed)
	{
		*out = val;
		return true;
	}
	return false;
}

bool PropWindow::ParseValue(std::string value, bool showError)
{
	// Try to parse a floating point number
	if (properties[selectedProperty].Type == StructProperty::Float)
	{
		PropertyValue propValue;
		bool ret = ParseFloat(value, &propValue.Float, properties[selectedProperty].Name == "temp");
		if (ret)
			tempValue = propValue;
		else if (showError)
			Engine::Ref().ShowWindow(new ErrorPrompt(TrUI("Invalid floating point number")));
		return ret;
	}

	// Check for quotes, in case there's a custom element called "42" or something
	bool inQuotes = false;
	if (value[0] == '"' && value[value.size() - 1] == '"')
	{
		inQuotes = true;
		value = value.substr(1, value.length() - 2);
	}

	if (!inQuotes)
	{
		// Try to parse a hex number
		if ((value[0] == '0' && value[1] == 'x') || value[0] == '#')
		{
			bool isParsed = ParseInteger(value.substr(value[0] == '#' ? 1 : 2), true);
			if (isParsed)
				return true;
		}

		// Attempt to parse as number
		bool isParsed = ParseInteger(value, false);
		if (isParsed)
			return true;
	}

	// For type, ctype, and any other int field, try parsing an element name
	if (properties[selectedProperty].Type == StructProperty::ParticleType || properties[selectedProperty].Type == StructProperty::Integer)
	{
		int elNumber;
		int success = console_parse_type(value.c_str(), &elNumber, nullptr, globalSim);
		if (success)
		{
			tempValue = { elNumber };
			return true;
		}
	}

	if (properties[selectedProperty].Name == "ctype")
	{
		std::string upperValue = Format::ToUpper(value);
		if (value.length() > 1 && (value.at(0) == 'B' || value.at(0) == 'b') && value.find('/') != value.npos)
		{
			int v = ParseGOLString(upperValue);
			if (v != -1)
			{
				tempValue = { v };
				return true;
			}
		}
		for (int i = 0; i < NGOL; i++)
		{
			if (builtinGol[i].name == upperValue)
			{
				tempValue = { i };
				return true;
			}
		}
		for (auto &cgol : static_cast<LIFE_ElementDataContainer&>(*globalSim->elementData[PT_LIFE]).GetCustomGOL())
		{
			if (cgol.nameString == upperValue)
			{
				tempValue = { ID(cgol.rule) };
				return true;
			}
		}
	}

	if (showError)
	{
		if (properties[selectedProperty].Type == StructProperty::ParticleType)
			Engine::Ref().ShowWindow(new ErrorPrompt(TrUI("Invalid element name")));
		else
			Engine::Ref().ShowWindow(new ErrorPrompt(TrUI("Invalid number")));
	}
	return false;
}
