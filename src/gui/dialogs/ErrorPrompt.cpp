#include "ErrorPrompt.h"
#include "common/Point.h"
#include "common/Translations.h"
#include "interface/Label.h"
#include "interface/Button.h"

ErrorPrompt::ErrorPrompt(std::string message, std::string dismiss):
	ui::Window(Point(CENTERED, CENTERED), Point(250, 55))
{
#ifndef TOUCHUI
	int buttonHeight = 15;
#else
	int buttonHeight = 25;
#endif
	Label *titleLabel = new Label(Point(5, 3), Point(Label::AUTOSIZE, Label::AUTOSIZE), TrUI("Error"));
	titleLabel->SetColor(COLRGB(255, 64, 32));
	this->AddComponent(titleLabel);

	Label *messageLabel = new Label(titleLabel->Below(Point(0, 0)), Point(240, Label::AUTOSIZE), message, true);
	this->Resize(Point(CENTERED, CENTERED), Point(250, messageLabel->GetSize().Y + 24 + buttonHeight));
	this->AddComponent(messageLabel);

	Button *okButton = new Button(Point(0, this->size.Y - buttonHeight), Point(this->size.X, buttonHeight), dismiss);
	okButton->SetTextColor(COLRGB(255, 64, 32));
	okButton->SetCloseButton(true);
	this->AddComponent(okButton);
}

void ErrorPrompt::OnExit(ui::DeleteReason deleteReason)
{
	if (callback.dismiss)
		callback.dismiss();
}

void ErrorPrompt::OnKeyPress(int key, int scan, bool repeat, bool shift, bool ctrl, bool alt)
{
	if (key == SDLK_RETURN)
		this->Close(ui::Confirmed);
}
