#include "LoginModel.h"
#include "LoginView.h"
#include "Config.h"
#include "client/Client.h"
#include "client/http/LoginRequest.h"
#include "client/http/LogoutRequest.h"

void LoginModel::Login(ByteString username, ByteString password)
{
	if (username.Contains("@"))
	{
		statusText = String::Build("请使用你的 Powder Toy 账号登录，而不是邮箱。如果还没有账号，可以在 ", SERVER, "/Register.html 注册");
		loginStatus = loginIdle;
		notifyStatusChanged();
		return;
	}
	statusText = "登陆中...";
	loginStatus = loginWorking;
	notifyStatusChanged();
	loginRequest = std::make_unique<http::LoginRequest>(username, password);
	loginRequest->Start();
}

void LoginModel::Logout()
{
	statusText = "注销中...";
	loginStatus = loginWorking;
	notifyStatusChanged();
	logoutRequest = std::make_unique<http::LogoutRequest>();
	logoutRequest->Start();
}

void LoginModel::AddObserver(LoginView * observer)
{
	observers.push_back(observer);
	notifyStatusChanged();
}

String LoginModel::GetStatusText()
{
	return statusText;
}

void LoginModel::Tick()
{
	if (loginRequest && loginRequest->CheckDone())
	{
		try
		{
			auto info = loginRequest->Finish();
			auto &client = Client::Ref();
			client.SetAuthUser(info.user);
			for (auto &item : info.notifications)
			{
				client.AddServerNotification(item);
			}
			statusText = "已登陆";
			loginStatus = loginSucceeded;
		}
		catch (const http::RequestError &ex)
		{
			ByteString english = ByteString(ex.what());
			// Translate common server error messages to Chinese
			if (english == "Wrong username or password") english = "用户名或密码错误";
			else if (english == "Unspecified Error") english = "未知错误";
			else if (english.BeginsWith("Could not read response")) english = "无法读取服务器响应";
			else if (english == "Error") english = "服务器错误";
			statusText = english.FromUtf8();
			loginStatus = loginIdle;
		}
		notifyStatusChanged();
		loginRequest.reset();
	}
	if (logoutRequest && logoutRequest->CheckDone())
	{
		try
		{
			logoutRequest->Finish();
			auto &client = Client::Ref();
			client.SetAuthUser(std::nullopt);
			statusText = "已注销";
		}
		catch (const http::RequestError &ex)
		{
			statusText = ByteString(ex.what()).FromUtf8();
		}
		loginStatus = loginIdle;
		notifyStatusChanged();
		logoutRequest.reset();
	}
}

void LoginModel::notifyStatusChanged()
{
	for (size_t i = 0; i < observers.size(); i++)
	{
		observers[i]->NotifyStatusChanged(this);
	}
}

LoginModel::~LoginModel()
{
	// Satisfy std::unique_ptr
}
