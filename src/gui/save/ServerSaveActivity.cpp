#include "ServerSaveActivity.h"
#include "common/Localization.h"
#include "graphics/Graphics.h"
#include "graphics/VideoBuffer.h"
#include "gui/interface/Label.h"
#include "gui/interface/Textbox.h"
#include "gui/interface/Button.h"
#include "gui/interface/Checkbox.h"
#include "gui/dialogues/ErrorMessage.h"
#include "gui/dialogues/SaveIDMessage.h"
#include "gui/dialogues/ConfirmPrompt.h"
#include "gui/dialogues/InformationMessage.h"
#include "client/Client.h"
#include "client/ThumbnailRendererTask.h"
#include "client/GameSave.h"
#include "client/http/UploadSaveRequest.h"
#include "tasks/Task.h"
#include "gui/Style.h"

class SaveUploadTask: public Task
{
	SaveInfo &save;

	void before() override
	{

	}

	void after() override
	{

	}

	bool doWork() override
	{
		notifyProgress(-1);
		auto uploadSaveRequest = std::make_unique<http::UploadSaveRequest>(save);
		uploadSaveRequest->Start();
		uploadSaveRequest->Wait();
		try
		{
			save.SetID(uploadSaveRequest->Finish());
		}
		catch (const http::RequestError &ex)
		{
			notifyError(ByteString(ex.what()).FromUtf8());
			return false;
		}
		return true;
	}

public:
	SaveUploadTask(SaveInfo &newSave):
		save(newSave)
	{

	}
};

ServerSaveActivity::ServerSaveActivity(std::unique_ptr<SaveInfo> newSave, OnUploaded onUploaded_) :
	WindowActivity(ui::Point(-1, -1), ui::Point(440, 200)),
	thumbnailRenderer(nullptr),
	save(std::move(newSave)),
	onUploaded(onUploaded_),
	saveUploadTask(nullptr)
{
	titleLabel = new ui::Label(ui::Point(4, 5), ui::Point((Size.X/2)-8, 16), "");
	titleLabel->SetTextColour(style::Colour::InformationTitle);
	titleLabel->Appearance.HorizontalAlign = ui::Appearance::AlignLeft;
	titleLabel->Appearance.VerticalAlign = ui::Appearance::AlignMiddle;
	AddComponent(titleLabel);
	CheckName(save->GetName()); //set titleLabel text

	ui::Label * previewLabel = new ui::Label(ui::Point((Size.X/2)+4, 5), ui::Point((Size.X/2)-8, 16), Localization::Ref().Tr("serversave.preview"));
	previewLabel->SetTextColour(style::Colour::InformationTitle);
	previewLabel->Appearance.HorizontalAlign = ui::Appearance::AlignLeft;
	previewLabel->Appearance.VerticalAlign = ui::Appearance::AlignMiddle;
	AddComponent(previewLabel);

	nameField = new ui::Textbox(ui::Point(8, 25), ui::Point((Size.X/2)-16, 16), save->GetName(), Localization::Ref().Tr("serversave.name_placeholder"));
	nameField->Appearance.VerticalAlign = ui::Appearance::AlignMiddle;
	nameField->Appearance.HorizontalAlign = ui::Appearance::AlignLeft;
	nameField->SetActionCallback({ [this] { CheckName(nameField->GetText()); } });
	nameField->SetLimit(50);
	AddComponent(nameField);
	FocusComponent(nameField);

	descriptionField = new ui::Textbox(ui::Point(8, 65), ui::Point((Size.X/2)-16, Size.Y-(65+16+4)), save->GetDescription(), Localization::Ref().Tr("serversave.description_placeholder"));
	descriptionField->SetMultiline(true);
	descriptionField->SetLimit(254);
	descriptionField->Appearance.VerticalAlign = ui::Appearance::AlignTop;
	descriptionField->Appearance.HorizontalAlign = ui::Appearance::AlignLeft;
	AddComponent(descriptionField);

	publishedCheckbox = new ui::Checkbox(ui::Point(8, 45), ui::Point((Size.X/2)-80, 16), Localization::Ref().Tr("serversave.publish"), "");
	auto user = Client::Ref().GetAuthUser();
	if (!(user && user->Username == save->GetUserName()))
	{
		//Save is not owned by the user, disable by default
		publishedCheckbox->SetChecked(false);
	}
	else
	{
		//Save belongs to the current user, use published state already set
		publishedCheckbox->SetChecked(save->GetPublished());
	}
	AddComponent(publishedCheckbox);

	pausedCheckbox = new ui::Checkbox(ui::Point(160, 45), ui::Point(55, 16), Localization::Ref().Tr("serversave.paused"), "");
	pausedCheckbox->SetChecked(save->GetGameSave()->paused);
	AddComponent(pausedCheckbox);

	ui::Button * cancelButton = new ui::Button(ui::Point(0, Size.Y-16), ui::Point((Size.X/2)-75, 16), Localization::Ref().Tr("serversave.cancel"));
	cancelButton->Appearance.HorizontalAlign = ui::Appearance::AlignLeft;
	cancelButton->Appearance.VerticalAlign = ui::Appearance::AlignMiddle;
	cancelButton->Appearance.BorderInactive = ui::Colour(200, 200, 200);
	cancelButton->SetActionCallback({ [this] {
		Exit();
	} });
	AddComponent(cancelButton);
	SetCancelButton(cancelButton);

	ui::Button * okayButton = new ui::Button(ui::Point((Size.X/2)-76, Size.Y-16), ui::Point(76, 16), Localization::Ref().Tr("serversave.save"));
	okayButton->Appearance.HorizontalAlign = ui::Appearance::AlignLeft;
	okayButton->Appearance.VerticalAlign = ui::Appearance::AlignMiddle;
	okayButton->Appearance.TextInactive = style::Colour::InformationTitle;
	okayButton->SetActionCallback({ [this] {
		Save();
	} });
	AddComponent(okayButton);
	SetOkayButton(okayButton);

	ui::Button * PublishingInfoButton = new ui::Button(ui::Point((Size.X*3/4)-75, Size.Y-42), ui::Point(150, 16), Localization::Ref().Tr("serversave.publishing_info"));
	PublishingInfoButton->Appearance.HorizontalAlign = ui::Appearance::AlignCentre;
	PublishingInfoButton->Appearance.VerticalAlign = ui::Appearance::AlignMiddle;
	PublishingInfoButton->Appearance.TextInactive = style::Colour::InformationTitle;
	PublishingInfoButton->SetActionCallback({ [this] {
		ShowPublishingInfo();
	} });
	AddComponent(PublishingInfoButton);

	ui::Button * RulesButton = new ui::Button(ui::Point((Size.X*3/4)-75, Size.Y-22), ui::Point(150, 16), Localization::Ref().Tr("serversave.save_uploading_rules"));
	RulesButton->Appearance.HorizontalAlign = ui::Appearance::AlignCentre;
	RulesButton->Appearance.VerticalAlign = ui::Appearance::AlignMiddle;
	RulesButton->Appearance.TextInactive = style::Colour::InformationTitle;
	RulesButton->SetActionCallback({ [this] {
		ShowRules();
	} });
	AddComponent(RulesButton);

	if (save->GetGameSave())
	{
		thumbnailRenderer = new ThumbnailRendererTask(*save->GetGameSave(), Size / 2 - Vec2(16, 16), RendererSettings::decorationAntiClickbait, true);
		thumbnailRenderer->Start();
	}
}

ServerSaveActivity::ServerSaveActivity(std::unique_ptr<SaveInfo> newSave, bool saveNow, OnUploaded onUploaded_) :
	WindowActivity(ui::Point(-1, -1), ui::Point(200, 50)),
	thumbnailRenderer(nullptr),
	save(std::move(newSave)),
	onUploaded(onUploaded_),
	saveUploadTask(nullptr)
{
	ui::Label * titleLabel = new ui::Label(ui::Point(0, 0), Size, Localization::Ref().Tr("serversave.saving_to_server"));
	titleLabel->SetTextColour(style::Colour::InformationTitle);
	titleLabel->Appearance.HorizontalAlign = ui::Appearance::AlignCentre;
	titleLabel->Appearance.VerticalAlign = ui::Appearance::AlignMiddle;
	AddComponent(titleLabel);

	AddAuthorInfo();

	saveUploadTask = new SaveUploadTask(*this->save);
	saveUploadTask->AddTaskListener(this);
	saveUploadTask->Start();
}

void ServerSaveActivity::NotifyDone(Task * task)
{
	if(!task->GetSuccess())
	{
		Exit();
		new ErrorMessage(Localization::Ref().Tr("serversave.error"), task->GetError());
	}
	else
	{
		if (onUploaded)
		{
			onUploaded(std::move(save));
		}
		Exit();
	}
}

void ServerSaveActivity::Save()
{
	if (!nameField->GetText().length())
	{
		new ErrorMessage(Localization::Ref().Tr("serversave.error"), Localization::Ref().Tr("serversave.specify_name"));
		return;
	}
	auto user = Client::Ref().GetAuthUser();
	if (!(user && user->Username == save->GetUserName()) && publishedCheckbox->GetChecked())
	{
		new ConfirmPrompt(Localization::Ref().Tr("serversave.confirm_publish_title"), Localization::Ref().Tr("serversave.confirm_publish_body_prefix") + save->GetUserName().FromUtf8() + Localization::Ref().Tr("serversave.confirm_publish_body_suffix"), { [this] {
			saveUpload();
		} });
	}
	else
	{
		saveUpload();
	}
}

void ServerSaveActivity::AddAuthorInfo()
{
	Bson serverSaveInfo;
	serverSaveInfo["type"] = "save";
	serverSaveInfo["id"] = save->GetID();
	auto user = Client::Ref().GetAuthUser();
	serverSaveInfo["username"] = user ? user->Username : ByteString("");
	serverSaveInfo["title"] = save->GetName().ToUtf8();
	serverSaveInfo["description"] = save->GetDescription().ToUtf8();
	serverSaveInfo["published"] = (int)save->GetPublished();
	serverSaveInfo["date"] = int64_t(time(nullptr));
	Client::Ref().SaveAuthorInfo(serverSaveInfo);
	{
		auto gameSave = save->TakeGameSave();
		gameSave->authors = serverSaveInfo;
		save->SetGameSave(std::move(gameSave));
	}
}

void ServerSaveActivity::saveUpload()
{
	okayButton->Enabled = false;
	save->SetName(nameField->GetText());
	save->SetDescription(descriptionField->GetText());
	save->SetPublished(publishedCheckbox->GetChecked());
	auto user = Client::Ref().GetAuthUser();
	save->SetUserName(user ? user->Username : ByteString(""));
	save->SetID(0);
	{
		auto gameSave = save->TakeGameSave();
		gameSave->paused = pausedCheckbox->GetChecked();
		save->SetGameSave(std::move(gameSave));
	}
	AddAuthorInfo();
	uploadSaveRequest = std::make_unique<http::UploadSaveRequest>(*save);
	uploadSaveRequest->Start();
}

void ServerSaveActivity::Exit()
{
	WindowActivity::Exit();
}

void ServerSaveActivity::ShowPublishingInfo()
{
	new InformationMessage(Localization::Ref().Tr("serversave.publishing_info"), Localization::Ref().Tr("serversave.publishing_info_body"), true);
}

void ServerSaveActivity::ShowRules()
{
	new InformationMessage(Localization::Ref().Tr("serversave.save_uploading_rules"), Localization::Ref().Tr("serversave.rules_body"), true);
}

void ServerSaveActivity::CheckName(String newname)
{
	auto user = Client::Ref().GetAuthUser();
	if (newname.length() && newname == save->GetName() && user && save->GetUserName() == user->Username)
		titleLabel->SetText(Localization::Ref().Tr("serversave.modify_properties"));
	else
		titleLabel->SetText(Localization::Ref().Tr("serversave.upload_new"));
}

void ServerSaveActivity::OnTick()
{
	if (thumbnailRenderer)
	{
		thumbnailRenderer->Poll();
		if (thumbnailRenderer->GetDone())
		{
			thumbnail = thumbnailRenderer->Finish();
			thumbnailRenderer = nullptr;
		}
	}

	if (uploadSaveRequest && uploadSaveRequest->CheckDone())
	{
		okayButton->Enabled = true;
		try
		{
			save->SetID(uploadSaveRequest->Finish());
			Exit();
			new SaveIDMessage(save->GetID());
			if (onUploaded)
			{
				onUploaded(std::move(save));
			}
		}
		catch (const http::RequestError &ex)
		{
			new ErrorMessage(Localization::Ref().Tr("serversave.error"), Localization::Ref().Tr("serversave.upload_failed_prefix") + ByteString(ex.what()).FromUtf8());
		}
		uploadSaveRequest.reset();
	}

	if(saveUploadTask)
		saveUploadTask->Poll();
}

void ServerSaveActivity::OnDraw()
{
	Graphics * g = GetGraphics();
	g->BlendRGBAImage(saveToServerImage->data(), RectSized(Vec2(-10, 0), saveToServerImage->Size()));
	g->DrawFilledRect(RectSized(Position, Size).Inset(-1), 0x000000_rgb);
	g->DrawRect(RectSized(Position, Size), 0xFFFFFF_rgb);

	if (Size.X > 220)
		g->DrawLine(Position + Vec2(Size.X / 2 - 1, 0), Position + Vec2(Size.X / 2 - 1, Size.Y - 1), 0xFFFFFF_rgb);

	if (thumbnail)
	{
		auto rect = RectSized(Position + Vec2(Size.X / 2 + (Size.X / 2 - thumbnail->Size().X) / 2, 25), thumbnail->Size());
		g->BlendImage(thumbnail->Data(), 0xFF, rect);
		g->DrawRect(rect, 0xB4B4B4_rgb);
	}
}

ServerSaveActivity::~ServerSaveActivity()
{
	if (thumbnailRenderer)
	{
		thumbnailRenderer->Abandon();
	}
	delete saveUploadTask;
}
