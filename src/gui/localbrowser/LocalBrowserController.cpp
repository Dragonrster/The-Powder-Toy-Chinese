#include "LocalBrowserController.h"

#include "LocalBrowserModel.h"
#include "LocalBrowserView.h"

#include "common/Localization.h"
#include "client/Client.h"
#include "client/GameSave.h"
#include "client/SaveFile.h"
#include "gui/dialogues/ConfirmPrompt.h"
#include "gui/dialogues/TextPrompt.h"
#include "gui/dialogues/ErrorMessage.h"
#include "tasks/TaskWindow.h"
#include "tasks/Task.h"

#include "Controller.h"

#include <algorithm>

LocalBrowserController::LocalBrowserController(std::function<void ()> onDone_):
	HasDone(false)
{
	browserModel = new LocalBrowserModel();
	browserView = new LocalBrowserView();
	browserView->AttachController(this);
	browserModel->AddObserver(browserView);

	onDone = onDone_;

	browserModel->UpdateSavesList(0);
}

void LocalBrowserController::OpenSave(int index)
{
	browserModel->OpenSave(index);
}

std::unique_ptr<SaveFile> LocalBrowserController::TakeSave()
{
	return browserModel->TakeSave();
}

void LocalBrowserController::RemoveSelected()
{
	StringBuilder desc;
	desc << "Are you sure you want to delete " << browserModel->GetSelected().size() << " stamp";
	if(browserModel->GetSelected().size()>1)
		desc << "s";
	desc << "?";
	new ConfirmPrompt(Localization::Ref().Tr("localbrowser.confirm_delete_stamps"), desc.Build(), { [this] { removeSelectedC(); } });
}

void LocalBrowserController::removeSelectedC()
{
	class RemoveSavesTask : public Task
	{
		std::vector<ByteString> saves;
		LocalBrowserController * c;
	public:
		RemoveSavesTask(LocalBrowserController * c, std::vector<ByteString> saves_) : c(c) { saves = saves_; }
		bool doWork() override
		{
			for (size_t i = 0; i < saves.size(); i++)
			{
				notifyStatus(String::Build(Localization::Ref().Tr("localbrowser.status_deleting_stamp"), saves[i].FromUtf8(), Localization::Ref().Tr("localbrowser.status_deleting_stamp_suffix")));
				Client::Ref().DeleteStamp(saves[i]);
				notifyProgress((i + 1) * 100 / saves.size());
			}
			return true;
		}
		void after() override
		{
			c->RefreshSavesList();
		}
	};

	std::vector<ByteString> selected = browserModel->GetSelected();
	new TaskWindow(Localization::Ref().Tr("localbrowser.task_removing_stamps"), new RemoveSavesTask(this, selected));
}

void LocalBrowserController::RenameSelected()
{
	ByteString save = browserModel->GetSelected()[0];

	new TextPrompt(Localization::Ref().Tr("localbrowser.rename_stamp_title"), Localization::Ref().Tr("localbrowser.rename_stamp_message"), "", Localization::Ref().Tr("localbrowser.rename_stamp_placeholder"), false, { [this, save](const String &newName) {
		if (newName.length() == 0)
		{
			new ErrorMessage(Localization::Ref().Tr("localbrowser.error_renaming_title"), Localization::Ref().Tr("localbrowser.error_renaming_message"));
			return;
		}

		Client::Ref().RenameStamp(save, newName.ToUtf8());

		RefreshSavesList();
	} });
}

void LocalBrowserController::RescanStamps()
{
	browserModel->RescanStamps();
	browserModel->UpdateSavesList(browserModel->GetPageNum());
}

void LocalBrowserController::RefreshSavesList()
{
	ClearSelection();
	browserModel->UpdateSavesList(browserModel->GetPageNum());
}

void LocalBrowserController::ClearSelection()
{
	browserModel->ClearSelected();
}

void LocalBrowserController::SetPage(int page)
{
	if (page != browserModel->GetPageNum() && page >= 0 && page < browserModel->GetPageCount())
		browserModel->UpdateSavesList(page);
}

void LocalBrowserController::SetPageRelative(int offset)
{
	int page = std::max(std::min(browserModel->GetPageNum() + offset, browserModel->GetPageCount() - 1), 0);
	if (page != browserModel->GetPageNum())
		browserModel->UpdateSavesList(page);
}

void LocalBrowserController::Update()
{
	if (browserModel->GetSave())
	{
		Exit();
	}
}

void LocalBrowserController::Selected(ByteString saveName, bool selected)
{
	if(selected)
		browserModel->SelectSave(saveName);
	else
		browserModel->DeselectSave(saveName);
}

bool LocalBrowserController::GetMoveToFront()
{
	return browserModel->GetMoveToFront();
}

void LocalBrowserController::SetMoveToFront(bool move)
{
	browserModel->SetMoveToFront(move);
}

void LocalBrowserController::Exit()
{
	browserView->CloseActiveWindow();
	if (onDone)
		onDone();
	HasDone = true;
}

LocalBrowserController::~LocalBrowserController()
{
	delete browserModel;
	browserView->CloseActiveWindow();
	delete browserView;
}

