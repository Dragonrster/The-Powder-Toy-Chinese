#include "PreviewView.h"

#include "simulation/SaveRenderer.h"

#include "common/Localization.h"
#include "client/Client.h"
#include "client/SaveInfo.h"
#include "client/GameSave.h"
#include "client/http/AddCommentRequest.h"
#include "client/http/ReportSaveRequest.h"

#include "gui/dialogues/TextPrompt.h"
#include "gui/profile/ProfileActivity.h"
#include "gui/interface/ScrollPanel.h"
#include "gui/interface/AvatarButton.h"
#include "gui/preview/PreviewController.h"
#include "gui/preview/PreviewModel.h"
#include "gui/interface/Button.h"
#include "gui/interface/CopyTextButton.h"
#include "gui/interface/Label.h"
#include "gui/interface/Textbox.h"
#include "gui/interface/Engine.h"
#include "gui/dialogues/ErrorMessage.h"
#include "gui/dialogues/InformationMessage.h"
#include "gui/interface/Point.h"
#include "gui/interface/Window.h"
#include "gui/Style.h"

#include "common/tpt-rand.h"
#include "common/platform/Platform.h"
#include "Format.h"
#include "Misc.h"

#include "graphics/VideoBuffer.h"
#include "SimulationConfig.h"
#include <SDL.h>

PreviewView::PreviewView(std::unique_ptr<VideoBuffer> newSavePreview):
	ui::Window(ui::Point(-1, -1), ui::Point((XRES/2)+210, (YRES/2)+150)),
	submitCommentButton(nullptr),
	addCommentBox(nullptr),
	commentWarningLabel(nullptr),
	userIsAuthor(false),
	doOpen(false),
	doError(false),
	doErrorMessage(""),
	showAvatars(true),
	prevPage(false),
	commentBoxHeight(20),
	commentHelpText(false)
{
	if (newSavePreview)
	{
		newSavePreview->Resize(RES / 2, true);
		savePreview = std::move(newSavePreview);
	}
	showAvatars = ui::Engine::Ref().ShowAvatars;

	auto user = Client::Ref().GetAuthUser();

	favButton = new ui::Button(ui::Point(50, Size.Y-19), ui::Point(51, 19), Localization::Ref().Tr("preview.fav"));
	favButton->Appearance.HorizontalAlign = ui::Appearance::AlignLeft;
	favButton->Appearance.VerticalAlign = ui::Appearance::AlignMiddle;
	favButton->SetTogglable(true);
	favButton->SetIcon(IconFavourite);
	favButton->SetActionCallback({ [this] {
		favButton->SetToggleState(true);
		favButton->Appearance.BackgroundPulse = true;
		c->FavouriteSave();
	} });
	favButton->Enabled = bool(user);
	AddComponent(favButton);

	reportButton = new ui::Button(ui::Point(100, Size.Y-19), ui::Point(51, 19), Localization::Ref().Tr("preview.report"));
	reportButton->Appearance.HorizontalAlign = ui::Appearance::AlignLeft;
	reportButton->Appearance.VerticalAlign = ui::Appearance::AlignMiddle;
	reportButton->SetIcon(IconReport);
	reportButton->SetActionCallback({ [this] {
		new TextPrompt(Localization::Ref().Tr("preview.report_title"), Localization::Ref().Tr("preview.report_message"), "", Localization::Ref().Tr("preview.report_placeholder"), true, { [this](String const &resultText) {
			if (reportSaveRequest)
			{
				return;
			}
			reportSaveRequest = std::make_unique<http::ReportSaveRequest>(c->SaveID(), resultText);
			reportSaveRequest->Start();
		} });
	} });
	reportButton->Enabled = bool(user);
	AddComponent(reportButton);

	openButton = new ui::Button(ui::Point(0, Size.Y-19), ui::Point(51, 19), Localization::Ref().Tr("preview.open"));
	openButton->Appearance.HorizontalAlign = ui::Appearance::AlignLeft;
	openButton->Appearance.VerticalAlign = ui::Appearance::AlignMiddle;
	openButton->SetIcon(IconOpen);
	openButton->SetActionCallback({ [this] { c->DoOpen(); } });
	AddComponent(openButton);

	browserOpenButton = new ui::Button(ui::Point((XRES/2)-107, Size.Y-19), ui::Point(108, 19), Localization::Ref().Tr("preview.open_in_browser"));
	browserOpenButton->Appearance.HorizontalAlign = ui::Appearance::AlignLeft;
	browserOpenButton->Appearance.VerticalAlign = ui::Appearance::AlignMiddle;
	browserOpenButton->SetIcon(IconOpen);
	browserOpenButton->SetActionCallback({ [this] { c->OpenInBrowser(); } });
	AddComponent(browserOpenButton);

	loadErrorButton = new ui::Button({ 0, 0 }, ui::Point(148, 19), Localization::Ref().Tr("gametools.error_loading_save"));
	loadErrorButton->Appearance.HorizontalAlign = ui::Appearance::AlignCentre;
	loadErrorButton->Appearance.VerticalAlign = ui::Appearance::AlignMiddle;
	loadErrorButton->SetIcon(IconDelete);
	loadErrorButton->SetActionCallback({ [this] { ShowLoadError(); } });
	loadErrorButton->Visible = false;
	AddComponent(loadErrorButton);

	missingElementsButton = new ui::Button({ 0, 0 }, ui::Point(148, 19), Localization::Ref().Tr("preview.missing_elements_button"));
	missingElementsButton->Appearance.HorizontalAlign = ui::Appearance::AlignCentre;
	missingElementsButton->Appearance.VerticalAlign = ui::Appearance::AlignMiddle;
	missingElementsButton->SetIcon(IconReport);
	missingElementsButton->SetActionCallback({ [this] { ShowMissingCustomElements(); } });
	missingElementsButton->Visible = false;
	AddComponent(missingElementsButton);

	if(showAvatars)
		saveNameLabel = new ui::Label(ui::Point(39, (YRES/2)+4), ui::Point(265, 16), "");
	else
		saveNameLabel = new ui::Label(ui::Point(5, (YRES/2)+4), ui::Point(300, 16), "");
	saveNameLabel->Appearance.HorizontalAlign = ui::Appearance::AlignLeft;
	saveNameLabel->Appearance.VerticalAlign = ui::Appearance::AlignMiddle;
	AddComponent(saveNameLabel);

	if(showAvatars)
		saveDescriptionLabel = new ui::Label(ui::Point(5, (YRES/2)+4+15+21), ui::Point((XRES/2)-10, Size.Y-((YRES/2)+4+15+17)-25), "");
	else
		saveDescriptionLabel = new ui::Label(ui::Point(5, (YRES/2)+4+15+19), ui::Point((XRES/2)-10, Size.Y-((YRES/2)+4+15+17)-23), "");
	saveDescriptionLabel->SetMultiline(true);
	saveDescriptionLabel->Appearance.HorizontalAlign = ui::Appearance::AlignLeft;
	saveDescriptionLabel->Appearance.VerticalAlign = ui::Appearance::AlignTop;
	saveDescriptionLabel->SetTextColour(ui::Colour(180, 180, 180));
	AddComponent(saveDescriptionLabel);

	if(showAvatars)
		authorDateLabel = new ui::Label(ui::Point(39, (YRES/2)+4+15), ui::Point(200, 16), "");
	else
		authorDateLabel = new ui::Label(ui::Point(5, (YRES/2)+4+15), ui::Point(220, 16), "");
	authorDateLabel->Appearance.HorizontalAlign = ui::Appearance::AlignLeft;
	authorDateLabel->Appearance.VerticalAlign = ui::Appearance::AlignMiddle;
	AddComponent(authorDateLabel);

	if(showAvatars)
	{
		avatarButton = new ui::AvatarButton(ui::Point(4, (YRES/2)+4), ui::Point(34, 34), "");
		avatarButton->SetActionCallback({ [this] {
			if (avatarButton->GetUsername().size() > 0)
			{
				new ProfileActivity(avatarButton->GetUsername());
			}
		} });
		AddComponent(avatarButton);
	}

	viewsLabel = new ui::Label(ui::Point((XRES/2)-88, (YRES/2)+4+15), ui::Point(88, 16), "");
	viewsLabel->Appearance.HorizontalAlign = ui::Appearance::AlignRight;
	viewsLabel->Appearance.VerticalAlign = ui::Appearance::AlignMiddle;
	AddComponent(viewsLabel);

	pageInfo = new ui::Label(ui::Point((XRES/2) + 85, Size.Y+1), ui::Point(70, 16), String::Build(Localization::Ref().Tr("preview.page_prefix"), 1, Localization::Ref().Tr("preview.page_of"), 1));
	pageInfo->Appearance.HorizontalAlign = ui::Appearance::AlignCentre;
	AddComponent(pageInfo);

	commentsPanel = new ui::ScrollPanel(ui::Point((XRES/2)+1, 1), ui::Point((Size.X-(XRES/2))-2, Size.Y-commentBoxHeight));
	AddComponent(commentsPanel);

	swearWords.insert("fuck");
	swearWords.insert("bitch");
	swearWords.insert("shit ");
	swearWords.insert("asshole");
	swearWords.insert("dick");
	swearWords.insert("cunt");
	swearWords.insert(" nigger");
	swearWords.insert("faggot");
	swearWords.insert("dumbass");
}

void PreviewView::AttachController(PreviewController * controller)
{
	c = controller;

	String copyHint = Localization::Ref().Tr("dialog.click_to_copy");
	int textWidth = Graphics::TextSize(copyHint).X - 1;
	saveIDLabel = new ui::Label(ui::Point((Size.X-textWidth-20)/2, Size.Y+5), ui::Point(textWidth+20, 16), copyHint);
	saveIDLabel->SetTextColour(ui::Colour(150, 150, 150));
	saveIDLabel->Appearance.HorizontalAlign = ui::Appearance::AlignCentre;
	AddComponent(saveIDLabel);

	textWidth = Graphics::TextSize(String::Build(c->SaveID())).X - 1;
	saveIDLabel2 = new ui::Label(ui::Point((Size.X-textWidth-20)/2-37, Size.Y+22), ui::Point(40, 16), Localization::Ref().Tr("preview.save_id_label"));
	AddComponent(saveIDLabel2);

	saveIDButton = new ui::CopyTextButton(ui::Point((Size.X-textWidth-10)/2, Size.Y+20), ui::Point(textWidth+10, 18), String::Build(c->SaveID()), saveIDLabel);
	AddComponent(saveIDButton);
}

void PreviewView::commentBoxAutoHeight()
{
	if(!addCommentBox)
		return;
	int textWidth = Graphics::TextSize(addCommentBox->GetText().c_str()).X - 1;
	if (commentHelpText || textWidth+15 > Size.X-(XRES/2)-48)
	{
		addCommentBox->Appearance.VerticalAlign = ui::Appearance::AlignTop;

		int oldSize = addCommentBox->Size.Y;
		addCommentBox->AutoHeight();
		int newSize = addCommentBox->Size.Y+2;
		addCommentBox->Size.Y = oldSize;

		commentBoxHeight = newSize+22;
		commentBoxPositionX.SetTarget((XRES/2)+4);
		commentBoxPositionY.SetTarget(float(Size.Y-(newSize+21)));
		commentBoxSizeX.SetTarget(float(Size.X-(XRES/2)-8));
		commentBoxSizeY.SetTarget(float(newSize));

		if (commentWarningLabel && commentHelpText && !commentWarningLabel->Visible && addCommentBox->Position.Y+addCommentBox->Size.Y < Size.Y-14)
		{
			commentWarningLabel->Visible = true;
		}
	}
	else
	{
		commentBoxHeight = 20;
		addCommentBox->Appearance.VerticalAlign = ui::Appearance::AlignMiddle;

		commentBoxPositionX.SetTarget((XRES/2)+4);
		commentBoxPositionY.SetTarget(float(Size.Y-19));
		commentBoxSizeX.SetTarget(float(Size.X-(XRES/2)-48));
		commentBoxSizeY.SetTarget(17);

		if (commentWarningLabel && commentWarningLabel->Visible)
		{
			commentWarningLabel->Visible = false;
		}
	}
}

bool PreviewView::CheckSwearing(String text)
{
	for (std::set<String>::iterator iter = swearWords.begin(), end = swearWords.end(); iter != end; iter++)
		if (text.Contains(*iter))
			return true;
	return false;
}

void PreviewView::CheckComment()
{
	if (!commentWarningLabel)
		return;
	String text = addCommentBox->GetText().ToLower();
	if (addCommentRequest)
	{
		commentWarningLabel->SetText(Localization::Ref().Tr("preview.comment_submitting"));
		commentHelpText = true;
	}
	else if (!userIsAuthor && (text.Contains("stolen") || text.Contains("copied")))
	{
		if (!commentHelpText)
		{
			if (interfaceRng()%2)
				commentWarningLabel->SetText(Localization::Ref().Tr("preview.comment_stolen_report"));
			else
				commentWarningLabel->SetText(Localization::Ref().Tr("preview.comment_stolen_please"));
			commentHelpText = true;
		}
	}
	else if (userIsAuthor && text.Contains("vote"))
	{
		commentWarningLabel->SetText(Localization::Ref().Tr("preview.comment_no_votes"));
		commentHelpText = true;
	}
	else if (CheckSwearing(text))
	{
		if (!commentHelpText)
		{
			if (interfaceRng()%2)
				commentWarningLabel->SetText(Localization::Ref().Tr("preview.comment_no_swear"));
			else
				commentWarningLabel->SetText(Localization::Ref().Tr("preview.comment_bad_language"));
			commentHelpText = true;
		}
	}
	else
	{
		commentHelpText = false;
		commentWarningLabel->Visible = false;
	}
}

void PreviewView::DoDraw()
{
	Graphics * g = GetGraphics();
	if (!c->GetFromUrl())
	{
		Window::DoDraw();
		for (size_t i = 0; i < commentTextComponents.size(); i++)
		{
			int linePos = commentTextComponents[i]->Position.Y+commentsPanel->ViewportPosition.Y+commentTextComponents[i]->Size.Y+4;
			if (linePos > 0 && linePos < Size.Y-commentBoxHeight)
			g->BlendLine(
					Position + Vec2{ 1+XRES/2, linePos },
					Position + Vec2{ Size.X-2, linePos },
					0xFFFFFF_rgb .WithAlpha(100));
		}
	}
	if (c->GetDoOpen())
	{
		g->BlendFilledRect(RectSized(Position + Size / 2 - Vec2{ 101, 26 }, { 202, 52 }), 0x000000_rgb .WithAlpha(210));
		g->BlendRect(RectSized(Position + Size / 2 - Vec2{ 100, 25 }, Vec2{ 200, 50 }), 0xFFFFFF_rgb .WithAlpha(180));
		{
			String loadingText = Localization::Ref().Tr("preview.loading_save");
			g->BlendText(Position + Vec2{(Size.X/2)-((Graphics::TextSize(loadingText).X - 1)/2), (Size.Y/2)-5}, loadingText, style::Colour::InformationTitle.NoAlpha().WithAlpha(255));
		}
	}
	if (!c->GetFromUrl())
	{
		g->DrawRect(RectSized(Position, Size), 0xFFFFFF_rgb);
	}
}

void PreviewView::OnDraw()
{
	Graphics * g = GetGraphics();

	//Window Background+Outline
	g->DrawFilledRect(RectSized(Position - Vec2{ 1, 1 }, Size + Vec2{ 2, 2 }), 0x000000_rgb);

	//Save preview (top-left)
	if (savePreview)
	{
		g->BlendImage(savePreview->Data(), 0xFF, RectSized(Position + Vec2(1, 1) + (RES / 2 - savePreview->Size()) / 2, savePreview->Size()));
	}
	g->BlendRect(RectSized(Position, RES / 2 + Vec2{ 1, 1 }), 0xFFFFFF_rgb .WithAlpha(100));
	g->DrawLine(Position + Vec2{ XRES/2, 1 }, Position + Vec2{ XRES/2, Size.Y-2 }, 0xC8C8C8_rgb);

	if(votesUp || votesDown)
	{
		float ryf;
		int nyu, nyd;
		int lv = (votesUp>votesDown)?votesUp:votesDown;
		lv = (lv>10)?lv:10;

		if (50>lv)
		{
			ryf = 50.0f/((float)lv);
			nyu = int(votesUp*ryf);
			nyd = int(votesDown*ryf);
		}
		else
		{
			ryf = ((float)lv)/50.0f;
			nyu = int(votesUp/ryf);
			nyd = int(votesDown/ryf);
		}
		nyu = nyu>50?50:nyu;
		nyd = nyd>50?50:nyd;

		g->DrawFilledRect(RectSized(Position + RES / 2 + Vec2{ -56, 3 }, Vec2{ 54, 7 }), 0x006B0A_rgb);
		g->DrawFilledRect(RectSized(Position + RES / 2 + Vec2{ -56, 9 }, Vec2{ 54, 7 }), 0x6B0A00_rgb);
		g->DrawRect(RectSized(Position + Vec2{ (XRES/2)-56, (YRES/2)+3 }, { 54, 7 }), 0x808080_rgb);
		g->DrawRect(RectSized(Position + Vec2{ (XRES/2)-56, (YRES/2)+9 }, { 54, 7 }), 0x808080_rgb);

		g->DrawFilledRect(RectSized(Position + RES / 2 + Vec2{ -4-nyu, 5 }, Vec2{ nyu, 3 }), 0x39BB39_rgb);
		g->DrawFilledRect(RectSized(Position + RES / 2 + Vec2{ -4-nyd, 11 }, Vec2{ nyd, 3 }), 0xBB3939_rgb);
	}
}

void PreviewView::OnTick()
{
	if(addCommentBox)
	{
		addCommentBox->Position.X = commentBoxPositionX;
		addCommentBox->Position.Y = commentBoxPositionY;

		if(addCommentBox->Size.X != commentBoxSizeX)
		{
			addCommentBox->Size.X = commentBoxSizeX;
			addCommentBox->Invalidate();
			commentBoxAutoHeight(); //make sure textbox height is correct after resizes
			addCommentBox->resetCursorPosition(); //make sure cursor is in correct position after resizes
		}
		if(addCommentBox->Size.Y != commentBoxSizeY)
		{
			addCommentBox->Size.Y = commentBoxSizeY;
			addCommentBox->Invalidate();
		}
		commentsPanel->Size.Y = addCommentBox->Position.Y-1;
	}

	c->Update();
	if (doError)
	{
		openButton->Enabled = false;
		loadErrorButton->Visible = true;
		UpdateLoadStatus();
	}

	if (reportSaveRequest && reportSaveRequest->CheckDone())
	{
		try
		{
			reportSaveRequest->Finish();
			c->Exit();
			new InformationMessage(Localization::Ref().Tr("preview.report_submitted_title"), Localization::Ref().Tr("preview.report_submitted_message"), false);
		}
		catch (const http::RequestError &ex)
		{
			new ErrorMessage(Localization::Ref().Tr("preview.error_report_title"), Localization::Ref().Tr("preview.error_report_prefix") + ByteString(ex.what()).FromUtf8());
		}
		reportSaveRequest.reset();
	}
	if (addCommentRequest && addCommentRequest->CheckDone())
	{
		try
		{
			addCommentRequest->Finish();
			addCommentBox->SetText("");
			c->CommentAdded();
		}
		catch (const http::RequestError &ex)
		{
			new ErrorMessage(Localization::Ref().Tr("preview.error_comment_title"), ByteString(ex.what()).FromUtf8());
		}
		isSubmittingComment = false;
		CheckCommentSubmitEnabled();
		commentBoxAutoHeight();
		addCommentRequest.reset();
		CheckComment();
	}
}

void PreviewView::OnTryExit(ExitMethod method)
{
	c->Exit();
}

void PreviewView::OnMouseWheel(int x, int y, int d)
{
	if(commentsPanel->GetScrollLimit() == 1 && d < 0)
		c->NextCommentPage();
	if(commentsPanel->GetScrollLimit() == -1 && d > 0)
	{
		if (c->PrevCommentPage())
			prevPage = true;
	}

}

void PreviewView::OnMouseUp(int x, int y, unsigned int button)
{
	//if mouse is on the scrollbar or farther right, and you are at the top of bottom of a page, change pages
	if (x > Position.X+commentsPanel->Position.X+commentsPanel->Size.X-6)
	{
		if (commentsPanel->GetScrollLimit() == 1)
			c->NextCommentPage();
		if (commentsPanel->GetScrollLimit() == -1)
		{
			if (c->PrevCommentPage())
				prevPage = true;
		}
	}
}

void PreviewView::OnKeyPress(int key, int scan, bool repeat, bool shift, bool ctrl, bool alt)
{
	if (repeat)
		return;
	if ((key == SDLK_KP_ENTER || key == SDLK_RETURN) && (!addCommentBox || !addCommentBox->IsFocused()))
		openButton->DoAction();
}

void PreviewView::ShowLoadError()
{
	new ErrorMessage(Localization::Ref().Tr("gametools.error_loading_save"), doErrorMessage, {});
}

void PreviewView::ShowMissingCustomElements()
{
	StringBuilder sb;
	sb << Localization::Ref().Tr("preview.missing_elements_intro");
	auto remainingIds = missingElements.ids;
	if (missingElements.identifiers.size())
	{
		sb << Localization::Ref().Tr("preview.missing_elements_identifiers_header");
		for (auto &[ identifier, id ] : missingElements.identifiers)
		{
			sb << "\n - " << identifier.FromUtf8();
			remainingIds.erase(id);
		}
	}
	if (remainingIds.size())
	{
		sb << Localization::Ref().Tr("preview.missing_elements_ids_header");
		for (auto id : remainingIds)
		{
			sb << "\n - " << id;
		}
	}
	new InformationMessage(Localization::Ref().Tr("preview.missing_elements_title"), sb.Build(), true);
}

void PreviewView::UpdateLoadStatus()
{
	auto y = YRES / 2 - 22;
	auto showButton = [&y](ui::Button *button) {
		if (button->Visible)
		{
			button->Position = { XRES / 2 - button->Size.X - 3, y };
			y -= button->Size.Y + 3;
		}
	};
	showButton(missingElementsButton);
	showButton(loadErrorButton);
}

void PreviewView::NotifySaveChanged(PreviewModel * sender)
{
	favButton->Appearance.BackgroundPulse = false;
	auto *save = sender->GetSaveInfo();
	if(save)
	{
		votesUp = save->votesUp;
		votesDown = save->votesDown;
		saveNameLabel->SetText(save->name);
		String dateType;
		if (save->updatedDate == save->createdDate)
			dateType = Localization::Ref().Tr("preview.created");
		else
			dateType = Localization::Ref().Tr("preview.updated");
		if (showAvatars)
		{
			avatarButton->SetUsername(save->userName);
			authorDateLabel->SetText("\bw" + save->userName.FromUtf8() + " \bg" + dateType + " \bw" + format::UnixtimeToDateMini(save->updatedDate).FromAscii());
		}
		else
		{
			authorDateLabel->SetText("\bg" + Localization::Ref().Tr("preview.author") + "\bw " + save->userName.FromUtf8() + " \bg" + dateType + " \bw" + format::UnixtimeToDateMini(save->updatedDate).FromAscii());
		}
		auto user = Client::Ref().GetAuthUser();
		if (user && save->userName == user->Username)
			userIsAuthor = true;
		else
			userIsAuthor = false;
		viewsLabel->SetText(String::Build("\bg", Localization::Ref().Tr("preview.views"), "\bw ", save->Views));
		saveDescriptionLabel->SetText(save->Description);
		if(save->Favourite)
		{
			favButton->Enabled = true;
			favButton->SetToggleState(true);
		}
		else if (user)
		{
			favButton->Enabled = true;
			favButton->SetToggleState(false);
		}
		else
		{
			favButton->SetToggleState(false);
			favButton->Enabled = false;
		}

		if(save->GetGameSave())
		{
			missingElements = save->GetGameSave()->missingElements;
			RendererSettings rendererSettings;
			rendererSettings.decorationLevel = RendererSettings::decorationAntiClickbait;
			savePreview = SaveRenderer::Ref().Render(save->GetGameSave(), true, rendererSettings);
			if (savePreview)
				savePreview->ResizeToFit(RES / 2, true);
			missingElementsButton->Visible = missingElements;
			UpdateLoadStatus();
		}
		else if (!sender->GetCanOpen())
			openButton->Enabled = false;
	}
	else
	{
		votesUp = 0;
		votesDown = 0;
		saveNameLabel->SetText("");
		authorDateLabel->SetText("");
		saveDescriptionLabel->SetText("");
		favButton->SetToggleState(false);
		favButton->Enabled = false;
		if (!sender->GetCanOpen())
			openButton->Enabled = false;
	}
}

void PreviewView::submitComment()
{
	if (addCommentBox)
	{
		String comment = addCommentBox->GetText();
		if (comment.length() == 0)
		{
			c->RefreshComments();
			isRefreshingComments = true;
		}
		else if (comment.length() < 4)
		{
			new ErrorMessage(Localization::Ref().Tr("common.error"), Localization::Ref().Tr("preview.error_comment_too_short"));
		}
		else
		{
			isSubmittingComment = true;
			FocusComponent(nullptr);

			addCommentRequest = std::make_unique<http::AddCommentRequest>(c->SaveID(), comment);
			addCommentRequest->Start();

			CheckComment();
		}

		CheckCommentSubmitEnabled();
	}
}

void PreviewView::CheckCommentSubmitEnabled()
{
	if (submitCommentButton)
		submitCommentButton->Enabled = !isRefreshingComments && !isSubmittingComment;
}

void PreviewView::NotifyCommentBoxEnabledChanged(PreviewModel * sender)
{
	if(addCommentBox)
	{
		RemoveComponent(addCommentBox);
		delete addCommentBox;
		addCommentBox = nullptr;
	}
	if(submitCommentButton)
	{
		RemoveComponent(submitCommentButton);
		delete submitCommentButton;
		submitCommentButton = nullptr;
	}
	if(sender->GetCommentBoxEnabled())
	{
		addCommentBox = new ui::Textbox(ui::Point((XRES/2)+4, Size.Y-19), ui::Point(Size.X-(XRES/2)-48, 17), "", Localization::Ref().Tr("preview.add_comment_placeholder"));
		commentBoxPositionX.SetTarget(float(addCommentBox->Position.X));
		commentBoxPositionX.SetValue(float(addCommentBox->Position.X));
		commentBoxPositionY.SetTarget(float(addCommentBox->Position.Y));
		commentBoxPositionY.SetValue(float(addCommentBox->Position.Y));
		commentBoxSizeX.SetTarget(float(addCommentBox->Size.X));
		commentBoxSizeX.SetValue(float(addCommentBox->Size.X));
		commentBoxSizeY.SetTarget(float(addCommentBox->Size.Y));
		commentBoxSizeY.SetValue(float(addCommentBox->Size.Y));
		addCommentBox->SetActionCallback({ [this] {
			CheckComment();
			commentBoxAutoHeight();
		} });
		addCommentBox->Appearance.HorizontalAlign = ui::Appearance::AlignLeft;
		addCommentBox->SetMultiline(true);
		addCommentBox->SetLimit(1000);
		AddComponent(addCommentBox);
		submitCommentButton = new ui::Button(ui::Point(Size.X-40, Size.Y-19), ui::Point(40, 19), Localization::Ref().Tr("preview.submit"));
		submitCommentButton->SetActionCallback({ [this] { submitComment(); } });
		AddComponent(submitCommentButton);

		commentWarningLabel = new ui::Label(ui::Point((XRES/2)+4, Size.Y-19), ui::Point(Size.X-(XRES/2)-48, 16), Localization::Ref().Tr("preview.comment_warning_bug"));
		commentWarningLabel->Appearance.HorizontalAlign = ui::Appearance::AlignLeft;
		commentWarningLabel->SetTextColour(ui::Colour(255, 0, 0));
		commentWarningLabel->Visible = false;
		AddComponent(commentWarningLabel);
	}
	else
	{
		submitCommentButton = new ui::Button(ui::Point(XRES/2, Size.Y-19), ui::Point(Size.X-(XRES/2), 19), Localization::Ref().Tr("preview.login_to_comment"));
		submitCommentButton->SetActionCallback({ [this] { c->ShowLogin(); } });
		AddComponent(submitCommentButton);
	}
}

void PreviewView::SaveLoadingError(String errorMessage)
{
	doError = true;
	doErrorMessage = errorMessage;
	Platform::MarkPresentable();
}

void PreviewView::NotifyCommentsPageChanged(PreviewModel * sender)
{
	pageInfo->SetText(String::Build(Localization::Ref().Tr("preview.page_prefix"), sender->GetCommentsPageNum(), Localization::Ref().Tr("preview.page_of"), sender->GetCommentsPageCount()));
}

void PreviewView::NotifyCommentsChanged(PreviewModel * sender)
{
	auto commentsPtr = sender->GetComments();

	for (size_t i = 0; i < commentComponents.size(); i++)
	{
		commentsPanel->RemoveChild(commentComponents[i]);
		delete commentComponents[i];
	}
	commentComponents.clear();
	commentTextComponents.clear();
	commentsPanel->InnerSize = ui::Point(0, 0);

	isRefreshingComments = false;
	CheckCommentSubmitEnabled();

	if (commentsPtr)
	{
		auto &comments = *commentsPtr;
		for (size_t i = 0; i < commentComponents.size(); i++)
		{
			commentsPanel->RemoveChild(commentComponents[i]);
			delete commentComponents[i];
		}
		commentComponents.clear();
		commentTextComponents.clear();

		int currentY = 0;//-yOffset;
		ui::Label * tempUsername;
		ui::Label * tempComment;
		ui::AvatarButton * tempAvatar;
		auto user = Client::Ref().GetAuthUser();
		for (size_t i = 0; i < comments.size(); i++)
		{
			if (showAvatars)
			{
				tempAvatar = new ui::AvatarButton(ui::Point(2, currentY+7), ui::Point(26, 26), comments[i].authorName);
				tempAvatar->SetActionCallback({ [tempAvatar] {
					if (tempAvatar->GetUsername().size() > 0)
					{
						new ProfileActivity(tempAvatar->GetUsername());
					}
				} });
				commentComponents.push_back(tempAvatar);
				commentsPanel->AddChild(tempAvatar);
			}

			auto authorNameFormatted = comments[i].authorName.FromUtf8();
			if (comments[i].authorElevation != User::ElevationNone || comments[i].authorName == "jacobot")
			{
				authorNameFormatted = "\bt" + authorNameFormatted;
			}
			else if (comments[i].authorIsBanned)
			{
				authorNameFormatted = "\bg" + authorNameFormatted;
			}
			else if (user && user->Username == comments[i].authorName)
			{
				authorNameFormatted = "\bo" + authorNameFormatted;
			}
			else if (sender->GetSaveInfo() && sender->GetSaveInfo()->GetUserName() == comments[i].authorName)
			{
				authorNameFormatted = "\bl" + authorNameFormatted;
			}
			if (showAvatars)
				tempUsername = new ui::Label(ui::Point(31, currentY+8), ui::Point(Size.X-((XRES/2) + 13 + 26), 14), authorNameFormatted);
			else
				tempUsername = new ui::Label(ui::Point(5, currentY+8), ui::Point(Size.X-((XRES/2) + 13), 14), authorNameFormatted);
			tempUsername->Appearance.HorizontalAlign = ui::Appearance::AlignLeft;
			tempUsername->Appearance.VerticalAlign = ui::Appearance::AlignMiddle;
			currentY += 16;

			commentComponents.push_back(tempUsername);
			commentsPanel->AddChild(tempUsername);

			if (showAvatars)
				tempComment = new ui::Label(ui::Point(31, currentY+5), ui::Point(Size.X-((XRES/2) + 13 + 26), -1), comments[i].content);
			else
				tempComment = new ui::Label(ui::Point(5, currentY+5), ui::Point(Size.X-((XRES/2) + 13), -1), comments[i].content);
			tempComment->SetMultiline(true);
			tempComment->Appearance.HorizontalAlign = ui::Appearance::AlignLeft;
			tempComment->Appearance.VerticalAlign = ui::Appearance::AlignTop;
			tempComment->SetTextColour(ui::Colour(180, 180, 180));
			currentY += tempComment->Size.Y+4;

			commentComponents.push_back(tempComment);
			commentsPanel->AddChild(tempComment);
			commentTextComponents.push_back(tempComment);
		}

		commentsPanel->InnerSize = ui::Point(commentsPanel->Size.X, currentY+4);
		if (prevPage)
		{
			prevPage = false;
			commentsPanel->SetScrollPosition(currentY);
		}
	}
	//else if (sender->GetCommentsLoaded())
	//	ErrorMessage::Blocking("Error loading comments", Client::Ref().GetLastError());
}

PreviewView::~PreviewView()
{
	if(addCommentBox)
	{
		RemoveComponent(addCommentBox);
		delete addCommentBox;
	}
	if(submitCommentButton)
	{
		RemoveComponent(submitCommentButton);
		delete submitCommentButton;
	}
}
