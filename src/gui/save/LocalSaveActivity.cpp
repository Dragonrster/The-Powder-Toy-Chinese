#include "LocalSaveActivity.h"

#include "client/Client.h"
#include "client/GameSave.h"
#include "client/ThumbnailRendererTask.h"
#include "common/platform/Platform.h"
#include "graphics/Graphics.h"
#include "graphics/VideoBuffer.h"
#include "gui/Style.h"

#include "gui/dialogues/ConfirmPrompt.h"
#include "gui/dialogues/ErrorMessage.h"
#include "gui/interface/Button.h"
#include "gui/interface/Label.h"
#include "gui/interface/Textbox.h"

#include "Config.h"

LocalSaveActivity::LocalSaveActivity(std::unique_ptr<SaveFile> newSave, OnSaved onSaved_) :
	WindowActivity(ui::Point(-1, -1), ui::Point(220, 200)),
	save(std::move(newSave)),
	thumbnailRenderer(nullptr),
	onSaved(onSaved_)
{
	ui::Label * titleLabel = new ui::Label(ui::Point(4, 5), ui::Point(Size.X-8, 16), ByteString("保存到本地:").FromUtf8());
	titleLabel->SetTextColour(style::Colour::InformationTitle);
	titleLabel->Appearance.HorizontalAlign = ui::Appearance::AlignLeft;
	titleLabel->Appearance.VerticalAlign = ui::Appearance::AlignMiddle;
	AddComponent(titleLabel);

	filenameField = new ui::Textbox(ui::Point(8, 25), ui::Point(Size.X-16, 16), save->GetDisplayName(), ByteString("[文件名]").FromUtf8());
	filenameField->Appearance.VerticalAlign = ui::Appearance::AlignMiddle;
	filenameField->Appearance.HorizontalAlign = ui::Appearance::AlignLeft;
	AddComponent(filenameField);
	FocusComponent(filenameField);

	ui::Button * cancelButton = new ui::Button(ui::Point(0, Size.Y-16), ui::Point(Size.X-75, 16), ByteString("取消").FromUtf8());
	cancelButton->Appearance.HorizontalAlign = ui::Appearance::AlignLeft;
	cancelButton->Appearance.VerticalAlign = ui::Appearance::AlignMiddle;
	cancelButton->Appearance.BorderInactive = ui::Colour(200, 200, 200);
	cancelButton->SetActionCallback({ [this] {
		Exit();
	} });
	AddComponent(cancelButton);
	SetCancelButton(cancelButton);

	ui::Button * okayButton = new ui::Button(ui::Point(Size.X-76, Size.Y-16), ui::Point(76, 16), ByteString("保存").FromUtf8());
	okayButton->Appearance.HorizontalAlign = ui::Appearance::AlignLeft;
	okayButton->Appearance.VerticalAlign = ui::Appearance::AlignMiddle;
	okayButton->Appearance.TextInactive = style::Colour::InformationTitle;
	okayButton->SetActionCallback({ [this] {
		Save();
	} });
	AddComponent(okayButton);
	SetOkayButton(okayButton);

	if(save->GetGameSave())
	{
		thumbnailRenderer = new ThumbnailRendererTask(*save->GetGameSave(), Size - Vec2(16, 16), RendererSettings::decorationEnabled, false);
		thumbnailRenderer->Start();
	}
}

void LocalSaveActivity::OnTick()
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
}

void LocalSaveActivity::Save()
{
	if (filenameField->GetText().Contains('/') || filenameField->GetText().BeginsWith("."))
	{
		new ErrorMessage(ByteString("错误").FromUtf8(), ByteString("无效的文件名").FromUtf8());
	}
	else if (filenameField->GetText().length())
	{
		ByteString finalFilename = ByteString::Build(LOCAL_SAVE_DIR, PATH_SEP_CHAR, filenameField->GetText().ToUtf8(), ".cps");
		save->SetDisplayName(filenameField->GetText());
		save->SetFileName(finalFilename);
		if (Platform::FileExists(finalFilename))
		{
			new ConfirmPrompt(ByteString("覆盖文件").FromUtf8(), ByteString("确定要覆盖吗?\n").FromUtf8()+finalFilename.FromUtf8(), { [this, finalFilename] {
				saveWrite(finalFilename);
			} });
		}
		else
		{
			saveWrite(finalFilename);
		}
	}
	else
	{
		new ErrorMessage(ByteString("错误").FromUtf8(), ByteString("文件名不能为空").FromUtf8());
	}
}

void LocalSaveActivity::saveWrite(ByteString finalFilename)
{
	Platform::MakeDirectory(LOCAL_SAVE_DIR);
	Json::Value localSaveInfo;
	localSaveInfo["type"] = "localsave";
	localSaveInfo["username"] = Client::Ref().GetAuthUser().Username;
	localSaveInfo["title"] = finalFilename;
	localSaveInfo["date"] = (Json::Value::UInt64)time(nullptr);
	Client::Ref().SaveAuthorInfo(&localSaveInfo);
	{
		auto gameSave = save->TakeGameSave();
		gameSave->authors = localSaveInfo;
		save->SetGameSave(std::move(gameSave));
	}
	std::vector<char> saveData;
	std::tie(std::ignore, saveData) = save->GetGameSave()->Serialise();
	if (saveData.size() == 0)
		new ErrorMessage(ByteString("错误").FromUtf8(), ByteString("无法序列化游戏数据").FromUtf8());
	else if (!Platform::WriteFile(saveData, finalFilename))
		new ErrorMessage(ByteString("错误").FromUtf8(), ByteString("无法读取沙盘数据").FromUtf8());
	else
	{
		if (onSaved)
		{
			onSaved(std::move(save));
		}
		Exit();
	}
}

void LocalSaveActivity::OnDraw()
{
	Graphics * g = GetGraphics();
	g->BlendRGBAImage(saveToDiskImage->data(), RectSized(Vec2(0, 0), saveToDiskImage->Size()));
	g->DrawFilledRect(RectSized(Position, Size).Inset(-1), 0x000000_rgb);
	g->DrawRect(RectSized(Position, Size), 0xFFFFFF_rgb);

	if (thumbnail)
	{
		auto rect = RectSized(Position + Vec2((Size.X - thumbnail->Size().X) / 2, 45), thumbnail->Size());
		g->BlendImage(thumbnail->Data(), 0xFF, rect);
		g->DrawRect(rect, 0xB4B4B4_rgb);
	}
}

LocalSaveActivity::~LocalSaveActivity()
{
	if (thumbnailRenderer)
	{
		thumbnailRenderer->Abandon();
	}
}
