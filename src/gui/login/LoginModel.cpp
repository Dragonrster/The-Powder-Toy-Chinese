#include "LoginModel.h"
#include "LoginView.h"
#include "Config.h"
#include "client/Client.h"
#include "client/http/LoginRequest.h"
#include "client/http/LogoutRequest.h"
#include "common/Localization.h"

void LoginModel::Login(ByteString username, ByteString password)
{
	if (username.Contains("@"))
	{
		statusText = String::Build(Localization::Ref().Tr("login.use_account_not_email"), SERVER, Localization::Ref().Tr("login.use_account_not_email_suffix"));
		loginStatus = loginIdle;
		notifyStatusChanged();
		return;
	}
	statusText = Localization::Ref().Tr("login.logging_in");
	loginStatus = loginWorking;
	notifyStatusChanged();
	loginRequest = std::make_unique<http::LoginRequest>(username, password);
	loginRequest->Start();
}

void LoginModel::Logout()
{
	statusText = Localization::Ref().Tr("login.logging_out");
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
			statusText = Localization::Ref().Tr("login.logged_in");
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
			client.SetAuthUser(std::nullopt);
			statusText = Localization::Ref().Tr("login.logged_out");
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
