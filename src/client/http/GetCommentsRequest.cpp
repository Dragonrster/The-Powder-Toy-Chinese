#include "GetCommentsRequest.h"
#include "client/Client.h"
#include "Config.h"

namespace http
{
	GetCommentsRequest::GetCommentsRequest(int saveID, int start, int count) :
		APIRequest(ByteString::Build(SCHEME, SERVER, "/Browse/Comments.json?ID=", saveID, "&Start=", start, "&Count=", count), authOmit, false)
	{
	}

	std::vector<Comment> GetCommentsRequest::Finish()
	{
		auto result = APIRequest::Finish();
		std::vector<Comment> comments;
		auto user = Client::Ref().GetAuthUser();
		try
		{
			for (auto &comment : result)
			{
				comments.push_back({
					comment["Username"].asString(),
					User::ElevationFromString(comment["Elevation"].asString()),
					comment["Username"].asString() == user.Username,
					comment["IsBanned"].asBool(),
					ByteString(comment["Text"].asString()).FromUtf8(),
				});
			}
		}
		catch (const std::exception &ex)
		{
			throw RequestError("\u65e0\u6cd5\u8bfb\u53d6\u54cd\u5e94\u003a\u0020" + ByteString(ex.what()));
		}
		return comments;
	}
}
