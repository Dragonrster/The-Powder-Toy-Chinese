#include "CreateSign.h"
#include "common/Translations.h"
#include "interface/Label.h"
#include "interface/Button.h"
#include "interface/Textbox.h"

CreateSign::CreateSign(int signID, Point pos):
	ui::Window(Point(CENTERED, CENTERED), Point(250, 100)),
	signID(signID),
	theSign(Sign("", pos.X, pos.Y, Sign::Middle))
{
	if (signID == -1)
		newSignLabel = new Label(Point(5, 3), Point(Label::AUTOSIZE, Label::AUTOSIZE), TrUI("New sign:"));
	else
		newSignLabel = new Label(Point(5, 3), Point(Label::AUTOSIZE, Label::AUTOSIZE), TrUI("Edit sign:"));
	newSignLabel->SetColor(COLRGB(140, 140, 255));
	this->AddComponent(newSignLabel);

	signTextbox = new Textbox(newSignLabel->Below(Point(0, 2)), Point(this->size.X-10, Textbox::AUTOSIZE), "");
	signTextbox->SetCharacterLimit(45);
	this->AddComponent(signTextbox);
#ifndef TOUCHUI
	FocusComponent(signTextbox);
#endif

	pointerLabel = new Label(signTextbox->Below(Point(0, 4)), Point(Label::AUTOSIZE, Label::AUTOSIZE), TrUI("Pointer:"));
	this->AddComponent(pointerLabel);

	leftJuButton = new Button(pointerLabel->Right(Point(3, 0)), Point(Button::AUTOSIZE, Button::AUTOSIZE), "\xA0 Left");
	leftJuButton->SetCallback([&](int mb) { this->SetJustification(Sign::Left); });
	this->AddComponent(leftJuButton);

	middleJuButton = new Button(leftJuButton->Right(Point(5, 0)), Point(Button::AUTOSIZE, Button::AUTOSIZE), "\x9E Middle");
	middleJuButton->SetCallback([&](int mb) { this->SetJustification(Sign::Middle); });
	this->AddComponent(middleJuButton);

	rightJuButton = new Button(middleJuButton->Right(Point(5, 0)), Point(Button::AUTOSIZE, Button::AUTOSIZE), "\x9F Right");
	rightJuButton->SetCallback([&](int mb) { this->SetJustification(Sign::Right); });
	this->AddComponent(rightJuButton);

	noneJuButton = new Button(rightJuButton->Right(Point(5, 0)), Point(Button::AUTOSIZE, Button::AUTOSIZE), "\x9D None");
	noneJuButton->SetCallback([&](int mb) { this->SetJustification(Sign::NoJustification); });
	this->AddComponent(noneJuButton);

	moveButton = new Button(leftJuButton->Below(Point(0, 4)), leftJuButton->GetSize(), TrUI("Move"));
	moveButton->SetCallback([&](int mb) { this->MoveSign(); });
	moveButton->SetCloseButton(true);
	this->AddComponent(moveButton);

	deleteButton = new Button(middleJuButton->Below(Point(0, 4)), middleJuButton->GetSize(), "\x85 Delete");
	deleteButton->SetCallback([&](int mb) { this->DeleteSign(); });
	deleteButton->SetCloseButton(true);
	this->AddComponent(deleteButton);

	okButton = new Button(Point(0, this->size.Y-15), Point(this->size.X+1, 15), TrUI("OK"));
	okButton->SetCallback([&](int mb) { this->SaveSign(); });
	okButton->SetCloseButton(true);
	this->AddComponent(okButton);

	if (signID == -1)
	{
		moveButton->SetEnabled(false);
		deleteButton->SetEnabled(false);
		SetJustification(Sign::Middle);
	}
	else
	{
		theSign = signs[signID];
		signTextbox->SetText(theSign.GetText());
		SetJustification(theSign.GetJustification());
	}
}

void CreateSign::OnKeyPress(int key, int scan, bool repeat, bool shift, bool ctrl, bool alt)
{
	if (key == SDLK_RETURN)
	{
		SaveSign();
		Close(ui::Confirmed);
	}
}

void CreateSign::SetJustification(Sign::Justification ju)
{
	theSign.SetJustification(ju);
	leftJuButton->SetState(ju == 0 ? Button::INVERTED : Button::NORMAL);
	middleJuButton->SetState(ju == 1 ? Button::INVERTED : Button::NORMAL);
	rightJuButton->SetState(ju == 2 ? Button::INVERTED : Button::NORMAL);
	noneJuButton->SetState(ju == 3 ? Button::INVERTED : Button::NORMAL);
}

void CreateSign::MoveSign()
{
	MSIGN = signID;
	SaveSign();
}

void CreateSign::DeleteSign()
{
	signTextbox->SetText("");
	SaveSign();
}

void CreateSign::SaveSign()
{
	if (signTextbox->GetText().empty())
	{
		if (signID != -1)
		{
			signs.erase(signs.begin() + signID);
		}
	}
	else
	{
		theSign.SetText(signTextbox->GetText());
		if (signID == -1)
			signs.push_back(theSign);
		else
			signs[signID] = theSign;
	}
}
