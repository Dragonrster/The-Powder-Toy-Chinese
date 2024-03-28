#include "RemoveTagRequest.h"
#include "client/Client.h"
#include "Config.h"

namespace http
{
	RemoveTagRequest::RemoveTagRequest(int saveID, ByteString tag) :
		APIRequest(ByteString::Build(SCHEME, SERVER, "/Browse/EditTag.json?Op=delete&ID=", saveID, "&Tag=", tag, "&Key=", Client::Ref().GetAuthUser().SessionKey), authRequire, true)
	{
	}

	std::list<ByteString> RemoveTagRequest::Finish()
	{
		auto result = APIRequest::Finish();
		std::list<ByteString> tags;
		try
		{
			for (auto &tag : result["Tags"])
			{
				tags.push_back(tag.asString());
			}
		}
		catch (const std::exception &ex)
		{
			throw RequestError("\u65e0\u6cd5\u8bfb\u53d6\u54cd\u5e94\u003a\u0020" + ByteString(ex.what()));
		}
		return tags;
	}
}
