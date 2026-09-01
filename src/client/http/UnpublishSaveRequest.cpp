#include "UnpublishSaveRequest.h"
#include "Config.h"

namespace http
{
	UnpublishSaveRequest::UnpublishSaveRequest(int saveID) :
		APIRequest({ ByteString::Build(SERVER, "/Browse/Delete.json"), {
			{ "ID", ByteString::Build(saveID) },
			{ "Mode", "取消发布" },
		} }, authRequireAppendSession, true)
	{
	}

	void UnpublishSaveRequest::Finish()
	{
		APIRequest::Finish();
	}
}

