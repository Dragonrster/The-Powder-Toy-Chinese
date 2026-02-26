#include "SaveIDMessage.h"
#include "common/Localization.h"
#include "gui/Style.h"
#include "graphics/Graphics.h"
#include "gui/interface/Button.h"
#include "gui/interface/CopyTextButton.h"
#include "gui/interface/Label.h"
#include "Format.h"
#include "SimulationConfig.h"

SaveIDMessage::SaveIDMessage(int id):
	ui::Window(ui::Point((XRES-244)/2, (YRES-90)/2), ui::Point(244, 90))
{
	String saveIdTitle = Localization::Ref().Tr("dialog.save_id");
	int textWidth = Graphics::TextSize(saveIdTitle).X - 1;
	ui::Label * titleLabel = new ui::Label(ui::Point(4, 5), ui::Point(textWidth+20, 16), saveIdTitle);
	titleLabel->SetTextColour(style::Colour::InformationTitle);
	titleLabel->Appearance.HorizontalAlign = ui::Appearance::AlignLeft;
	titleLabel->Appearance.VerticalAlign = ui::Appearance::AlignMiddle;
	AddComponent(titleLabel);

	String savedMsg = Localization::Ref().Tr("dialog.saved_successfully");
	textWidth = Graphics::TextSize(savedMsg).X - 1;
	ui::Label * messageLabel = new ui::Label(ui::Point(4, 24), ui::Point(textWidth+20, 16), savedMsg);
	messageLabel->Appearance.HorizontalAlign = ui::Appearance::AlignLeft;
	messageLabel->Appearance.VerticalAlign = ui::Appearance::AlignTop;
	AddComponent(messageLabel);

	String copyHint = Localization::Ref().Tr("dialog.click_to_copy");
	textWidth = Graphics::TextSize(copyHint).X - 1;
	ui::Label * copyTextLabel = new ui::Label(ui::Point((Size.X-textWidth-20)/2, 35), ui::Point(textWidth+20, 16), copyHint);
	copyTextLabel->SetTextColour(ui::Colour(150, 150, 150));
	copyTextLabel->Appearance.HorizontalAlign = ui::Appearance::AlignCentre;
	AddComponent(copyTextLabel);

	textWidth = Graphics::TextSize(String::Build(id)).X - 1;
	ui::CopyTextButton * copyTextButton = new ui::CopyTextButton(ui::Point((Size.X-textWidth-10)/2, 50), ui::Point(textWidth+10, 18), String::Build(id), copyTextLabel);
	AddComponent(copyTextButton);

	ui::Button * okayButton = new ui::Button(ui::Point(0, Size.Y-16), ui::Point(Size.X, 16), Localization::Ref().Tr("dialog.ok"));
	okayButton->Appearance.HorizontalAlign = ui::Appearance::AlignLeft;
	okayButton->Appearance.VerticalAlign = ui::Appearance::AlignMiddle;
	okayButton->SetActionCallback({ [this] {
		CloseActiveWindow();
		SelfDestruct();
	} });
	AddComponent(okayButton);
	// This button has multiple personalities
	SetOkayButton(okayButton);
	SetCancelButton(okayButton);

	MakeActiveWindow();
}

void SaveIDMessage::OnDraw()
{
	Graphics * g = GetGraphics();

	g->DrawFilledRect(RectSized(Position - Vec2{ 1, 1 }, Size + Vec2{ 2, 2 }), 0x000000_rgb);
	g->DrawRect(RectSized(Position, Size), 0xC8C8C8_rgb);
}

void SaveIDMessage::OnTryExit(ExitMethod method)
{
	CloseActiveWindow();
	SelfDestruct();
}
