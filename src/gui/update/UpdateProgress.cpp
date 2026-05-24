#include "UpdateProgress.h"
#include "bzlib.h"
#include "common/Format.h"
#include "common/Translations.h"
#include "game/Request.h"
#include "gui/dialogs/ErrorPrompt.h"
#include "interface/Engine.h"
#include "interface/Label.h"
#include "interface/ProgressBar.h"

UpdateProgress::UpdateProgress(std::string uri, std::string username, std::function<void(char*, int)> callback):
	ui::Window(Point(CENTERED, CENTERED), Point(242, 62)),
	callback(callback)
{
	Label *titleLabel = new Label(Point(5, 3), Point(Label::AUTOSIZE, Label::AUTOSIZE), TrUI("Please wait"));
	titleLabel->SetColor(COLRGB(100, 100, 255));
	this->AddComponent(titleLabel);

	Label *messageLabel = new Label(titleLabel->Below(Point(0, 0)), Point(240, Label::AUTOSIZE), TrUI("Downloading update..."));
	this->AddComponent(messageLabel);

	progress = new ProgressBar(Point(0, size.Y - 16), Point(242, 16));
	progress->SetColor(COLRGB(140, 140, 255));
	AddComponent(progress);

	download = new Request(uri);
	const char *usernameC = username.c_str();
	download->AuthHeaders(usernameC, "");
	download->Start();
}

void UpdateProgress::ShowError(std::string message)
{
	ErrorPrompt *error = new ErrorPrompt(message);
	Engine::Ref().ShowWindow(error);
	Close(ui::NoDeleteReason);
}

void UpdateProgress::OnTick(uint32_t ticks)
{
	int total, done;
	download->CheckProgress(&total, &done);
	int prog = total ? (done*100)/total : 0;
	progress->SetProgress(prog);

	if (completed)
	{
		int status;
		std::string data = download->Finish(&status);
		if (status != 200)
		{
			ShowError(Request::GetStatusCodeDesc(status));
			return;
		}
		if (data.length() < 16)
		{
			ShowError(TrUI("Server did not return data"));
			return;
		}

		// BuTT format, blame Skylark
		if (data[0] != 0x42 || data[1] != 0x75 || data[2] != 0x54 || data[3] != 0x54)
		{
			ShowError(TrUI("Invalid update format"));
			return;
		}

		unsigned int ulen  = (unsigned char)data[4];
		ulen |= ((unsigned char)data[5])<<8;
		ulen |= ((unsigned char)data[6])<<16;
		ulen |= ((unsigned char)data[7])<<24;

		char* updateBuf = new char[ulen];
		int bzStatus = BZ2_bzBuffToBuffDecompress(updateBuf, (unsigned *)&ulen, (char*)data.substr(8).c_str(), data.length()-8, 0, 0);
		if (bzStatus)
		{
			ShowError(TrUI("Decompression failure: ") + Format::NumberToString<int>(bzStatus));
			delete[] updateBuf;
			return;
		}

		callback(updateBuf, ulen);
		Close(ui::NoDeleteReason);
		return;
	}
	// delay completion a frame so that progress bar shows 100%
	if (download->CheckDone())
	{
		completed = true;
		progress->SetProgress(100);
	}
}
