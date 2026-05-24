#include "TextPrompt.h"
#include "common/Point.h"
#include "common/Translations.h"
#include "interface/Label.h"
#include "interface/Button.h"
#include "interface/Textbox.h"

TextPrompt::TextPrompt(std::string title, std::string prompt, std::string text, std::string shadow):
	ui::Window(Point(CENTERED, CENTERED), Point(250, 55))
{
#ifndef TOUCHUI
	int buttonHeight = 15;
#else
	int buttonHeight = 25;
#endif

	Label *titleLabel = new Label(Point(5, 3), Point(Label::AUTOSIZE, Label::AUTOSIZE), title);
	titleLabel->SetColor(COLRGB(140, 140, 255));
	this->AddComponent(titleLabel);

	Label *promptLabel = new Label(titleLabel->Below(Point(0, 0)), Point(240, Label::AUTOSIZE), prompt, true);
	this->AddComponent(promptLabel);

	inputTextbox = new Textbox(promptLabel->Below(Point(0, 0)), Point(240, Textbox::AUTOSIZE), text);
	inputTextbox->SetPlaceholder(shadow);
	this->AddComponent(inputTextbox);

	this->Resize(Point(CENTERED, CENTERED), Point(250, promptLabel->GetSize().Y + inputTextbox->GetSize().Y + 24 + buttonHeight));

	Button *okButton = new Button(Point(0, this->size.Y - buttonHeight), Point(this->size.X, buttonHeight), TrUI("OK"));
	okButton->SetTextColor(COLRGB(140, 140, 255));
	okButton->SetConfirmButton(true);
	this->AddComponent(okButton);
}

void TextPrompt::OnExit(ui::DeleteReason deleteReason)
{
	if (callback.input)
	{
		if (deleteReason == ui::Confirmed)
			callback.input(inputTextbox->GetText());
		else
			callback.input(std::nullopt);
	}
}

void TextPrompt::OnKeyPress(int key, int scan, bool repeat, bool shift, bool ctrl, bool alt)
{
	if (key == SDLK_RETURN)
		this->Close(ui::Confirmed);
}
