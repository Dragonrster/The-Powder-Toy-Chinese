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
		statusText =  ByteString("請使用您的TPT帳戶登入，而不是郵箱。\n如果您沒有TPT帳戶，可以在此註冊powdertoy.co.uk/register.html").FromUtf8();
		loginStatus = loginIdle;
		notifyStatusChanged();
		return;
	}
	statusText =  ByteString("登陸中...").FromUtf8();
	loginStatus = loginWorking;
	notifyStatusChanged();
	loginRequest = std::make_unique<http::LoginRequest>(username, password);
	loginRequest->Start();
}

void LoginModel::Logout()
{
	statusText = ByteString("登出中...").FromUtf8();
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
			statusText = ByteString("已登陸").FromUtf8();
			loginStatus = loginSucceeded;
		}
		catch (const http::RequestError &ex)
		{
			statusText = ByteString(ex.what()).FromUtf8();
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
			client.SetAuthUser(User(0, ""));
			statusText = ByteString("已登出").FromUtf8();
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
