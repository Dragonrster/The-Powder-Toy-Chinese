#include "SearchTagsRequest.h"
#include "Config.h"
#include "Format.h"

namespace http
{
	static ByteString Url(int start, int count, ByteString query)
	{
		ByteStringBuilder builder;
		builder << SERVER << "/Browse/Tags.json?Start=" << start << "&Count=" << count;
		if (query.size())
		{
			builder << "&Search_Query=" << format::URLEncode(query);
		}
		return builder.Build();
	}

	SearchTagsRequest::SearchTagsRequest(int start, int count, ByteString query) : APIRequest(Url(start, count, query), authOmit, false)
	{
	}

	std::vector<std::pair<ByteString, int>> SearchTagsRequest::Finish()
	{
		std::vector<std::pair<ByteString, int>> tags;
		auto result = APIRequest::Finish();
		try
		{
			for (auto &tag : result["Tags"])
			{
				tags.push_back({
					tag["Tag"].asString(),
					tag["Count"].asInt(),
				});
			}
		}
		catch (const std::exception &ex)
		{
			throw RequestError("\u65e0\u6cd5\u8bfb\u53d6\u54cd\u5e94\u003a\u0020" + ByteString(ex.what()));
		}
		return tags;
	}
}
