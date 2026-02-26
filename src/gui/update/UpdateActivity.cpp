#include "UpdateActivity.h"
#include "client/http/Request.h"
#include "prefs/GlobalPrefs.h"
#include "common/platform/Platform.h"
#include "tasks/Task.h"
#include "tasks/TaskWindow.h"
#include "gui/dialogues/ConfirmPrompt.h"
#include "gui/interface/Engine.h"
#include "Config.h"
#include <bzlib.h>
#include <memory>

class UpdateDownloadTask : public Task
{
public:
	UpdateDownloadTask(ByteString updateName, UpdateActivity * a) : a(a), updateName(updateName) {}
private:
	UpdateActivity * a;
	ByteString updateName;
	void notifyDoneMain() override {
		a->NotifyDone(this);
	}
	void notifyErrorMain() override
	{
		a->NotifyError(this);
	}
	bool doWork() override
	{
		auto &prefs = GlobalPrefs::Ref();

		auto niceNotifyError = [this](String error) {
			notifyError("下载的更新文件已损坏\n" + error);
			return false;
		};

		auto request = std::make_unique<http::Request>(updateName);
		request->Start();
		notifyStatus("正在下载更新文件");
		notifyProgress(-1);
		while(!request->CheckDone())
		{
			int64_t total, done;
			std::tie(total, done) = request->CheckProgress();
			if (total == -1)
			{
				notifyProgress(-1);
			}
			else
			{
				notifyProgress(total ? done * 100 / total : 0);
			}
			Platform::Millisleep(1);
		}

		int status;
		ByteString data;
		try
		{
			std::tie(status, data) = request->Finish();
		}
		catch (const http::RequestError &ex)
		{
			return niceNotifyError("无法下载更新文件： " + String::Build("服务器响应状态 ", ByteString(ex.what()).FromAscii()));
		}
		if (status!=200)
		{
			return niceNotifyError("无法下载更新文件： " + String::Build("服务器响应状态 ", status));
		}
		if (!data.size())
		{
			return niceNotifyError("服务器没有返回任何数据");
		}

		notifyStatus("正在拆包更新文件");
		notifyProgress(-1);

		unsigned int uncompressedLength;

		if(data.size()<16)
		{
			return niceNotifyError(String::Build("数据不足,已获取 ", data.size(), " bytes"));
		}
		if (data[0]!=0x42 || data[1]!=0x75 || data[2]!=0x54 || data[3]!=0x54)
		{
			return niceNotifyError("更新文件格式无效");
		}

		uncompressedLength  = (unsigned char)data[4];
		uncompressedLength |= ((unsigned char)data[5])<<8;
		uncompressedLength |= ((unsigned char)data[6])<<16;
		uncompressedLength |= ((unsigned char)data[7])<<24;

		std::vector<char> res(uncompressedLength);

		int dstate;
		dstate = BZ2_bzBuffToBuffDecompress(res.data(), (unsigned *)&uncompressedLength, &data[8], data.size()-8, 0, 0);
		if (dstate)
		{
			return niceNotifyError(String::Build("无法解压更新文件：", dstate));
		}

		notifyStatus("正在应用更新文件");
		notifyProgress(-1);

		prefs.Set("version.update", true);
		if (!Platform::UpdateStart(res))
		{
			prefs.Set("version.update", false);
			Platform::UpdateCleanup();
			notifyError("更新失败 - 尝试下载新版本");
			return false;
		}

		return true;
	}
};

UpdateActivity::UpdateActivity(UpdateInfo info)
{
	updateDownloadTask = new UpdateDownloadTask(info.file, this);
	updateWindow = new TaskWindow("正在下载更新文件...", updateDownloadTask, true);
}

void UpdateActivity::NotifyDone(Task * sender)
{
	if(sender->GetSuccess())
	{
		Exit();
	}
}

void UpdateActivity::Exit()
{
	updateWindow->Exit();
	ui::Engine::Ref().Exit();
	delete this;
}

void UpdateActivity::NotifyError(Task * sender)
{
	StringBuilder sb;
	if constexpr (USE_UPDATESERVER)
	{
		sb << "请上网手动下载新版本\n";
	}
	else
	{
		sb << "请访问该网站下载更新版本\n";
	}
	sb << "Error: " << sender->GetError();
	new ConfirmPrompt("Autoupdate failed", sb.Build(), { [this] {
		if constexpr (!USE_UPDATESERVER)
		{
			Platform::OpenURI(ByteString::Build(SERVER, "/Download.html"));
		}
		Exit();
	}, [this] { Exit(); } });
}


UpdateActivity::~UpdateActivity() {
}
