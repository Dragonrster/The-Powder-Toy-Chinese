#include "DeleteSaveRequest.h"
#include "Config.h"

namespace http
{
	DeleteSaveRequest::DeleteSaveRequest(int saveID) :
		APIRequest({ ByteString::Build(SERVER, "/Browse/Delete.json"), {
			{ "ID", ByteString::Build(saveID) },
			{ "Mode", "删除" },
		} }, authRequireAppendSession, true)
	{
	}

	void DeleteSaveRequest::Finish()
	{
		APIRequest::Finish();
	}
}
