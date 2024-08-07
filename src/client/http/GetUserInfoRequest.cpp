#include "GetUserInfoRequest.h"
#include "client/UserInfo.h"
#include "Config.h"

namespace http
{
	GetUserInfoRequest::GetUserInfoRequest(ByteString username) :
		APIRequest(ByteString::Build(SERVER, "/User.json?Name=", username), authOmit, false)
	{
	}

	UserInfo GetUserInfoRequest::Finish()
	{
		auto result = APIRequest::Finish();
		UserInfo userInfo;
		try
		{
			auto &user = result["User"];
			userInfo = UserInfo(
				user["ID"].asInt(),
				user["Age"].asInt(),
				user["Username"].asString(),
				ByteString(user["Biography"].asString()).FromUtf8(),
				ByteString(user["Location"].asString()).FromUtf8(),
				user["Website"].asString(),
				user["Saves"]["Count"].asInt(),
				user["Saves"]["AverageScore"].asFloat(),
				user["Saves"]["HighestScore"].asInt(),
				user["Forum"]["Topics"].asInt(),
				user["Forum"]["Replies"].asInt(),
				user["Forum"]["Reputation"].asInt()
			);
		}
		catch (const std::exception &ex)
		{
			throw RequestError("\u65e0\u6cd5\u8bfb\u53d6\u54cd\u5e94\u003a\u0020" + ByteString(ex.what()));
		}
		return userInfo;
	}
}

