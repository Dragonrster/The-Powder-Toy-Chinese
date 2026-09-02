#include "Dynamic.h"
#include "Clipboard.h"
#include "client/GameSave.h"
#include "common/platform/Platform.h"
#include "PowderToySDL.h"
#include "WindowSubsystem.h"
#include <SDL3/SDL.h>
#include <iostream>
#include <windows.h>

namespace Clipboard
{
	class WindowsClipboardImpl : public ClipboardImpl
	{
		UINT saveClipboardFormat = 0;
		HWND ourHwnd = nullptr;
		DWORD seqNumber = 0; // 0 is invalid

		class ClipboardSession
		{
			bool open = false;

		public:
			ClipboardSession(HWND ourHwnd)
			{
				if (ourHwnd)
				{
					open = ::OpenClipboard(ourHwnd);
				}
			}

			~ClipboardSession()
			{
				if (open)
				{
					::CloseClipboard();
				}
			}

			explicit operator bool() const
			{
				return open;
			}
		};

		void Transfer()
		{
			if (!saveClipboardFormat)
			{
				std::cerr << "cannot transfer save data: save clipboard format not registered" << std::endl;
				return;
			}
			if (!clipboardData)
			{
				std::cerr << "cannot transfer save data: no data to transfer" << std::endl;
				return;
			}
			std::vector<char> saveData;
			SerializeClipboard(saveData);
			auto handle = std::unique_ptr<void, decltype(&::GlobalFree)>(::GlobalAlloc(GMEM_MOVEABLE, saveData.size()), GlobalFree);
			if (!handle)
			{
				std::cerr << "cannot transfer save data: GlobalAlloc failed: " << ::GetLastError() << std::endl;
				return;
			}
			{
				auto data = std::unique_ptr<void, decltype(&::GlobalUnlock)>(::GlobalLock(handle.get()), ::GlobalUnlock);
				auto base = reinterpret_cast<char *>(data.get());
				std::copy(saveData.begin(), saveData.end(), base);
			}
			if (!::SetClipboardData(saveClipboardFormat, handle.get()))
			{
				std::cerr << "cannot transfer save data: SetClipboardData failed: " << ::GetLastError() << std::endl;
				return;
			}
			handle.release(); // windows owns it now
			auto newSeqNumber = ::GetClipboardSequenceNumber();
			if (newSeqNumber)
			{
				seqNumber = newSeqNumber;
			}
			std::cerr << "transferred save data" << std::endl;
		}

		static bool TransferWatchWrapper(void *userdata, MSG *msg)
		{
			return reinterpret_cast<WindowsClipboardImpl *>(userdata)->TransferWatch(msg);
		}

		bool TransferWatch(MSG *msg)
		{
			// SDL3 delivers raw Windows messages through SDL_SetWindowsMessageHook
			// instead of the removed SDL_SYSWMEVENT. WM_RENDERFORMAT and
			// WM_RENDERALLFORMATS are only posted to windows that have announced
			// data on the clipboard, and only our main thread ever owns a window.
			switch (msg->message)
			{
			case WM_RENDERFORMAT:
				if (msg->wParam == saveClipboardFormat)
				{
					Transfer();
				}
				break;

			case WM_RENDERALLFORMATS:
				{
					ClipboardSession cs(ourHwnd);
					if (cs)
					{
						if (ourHwnd && ::GetClipboardOwner() == ourHwnd)
						{
							Transfer();
						}
					}
					else
					{
						std::cerr << "cannot place save on clipboard: OpenClipboard failed: " << ::GetLastError() << std::endl;
					}
				}
				break;
			}
			return true;
		}

	public:
		WindowsClipboardImpl()
		{
			ourHwnd = reinterpret_cast<HWND>(SDL_GetPointerProperty(SDL_GetWindowProperties(sdl_window), SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr));
			saveClipboardFormat = ::RegisterClipboardFormatW(Platform::WinWiden(clipboardFormatName).c_str());
			if (!saveClipboardFormat)
			{
				std::cerr << "cannot register save clipboard format: RegisterClipboardFormatW failed: " << ::GetLastError() << std::endl;
				return;
			}
			std::cerr << "save clipboard format registered" << std::endl;
			SDL_SetWindowsMessageHook(&WindowsClipboardImpl::TransferWatchWrapper, this);
		}

		~WindowsClipboardImpl()
		{
			SDL_SetWindowsMessageHook(nullptr, nullptr);
		}

		void SetClipboardData() final override
		{
			if (!saveClipboardFormat)
			{
				std::cerr << "cannot announce save on clipboard: save clipboard format not registered" << std::endl;
				return;
			}
			ClipboardSession cs(ourHwnd);
			if (!cs)
			{
				std::cerr << "cannot announce save on clipboard: OpenClipboard failed: " << ::GetLastError() << std::endl;
				return;
			}
			if (!::EmptyClipboard())
			{
				std::cerr << "cannot announce save on clipboard: EmptyClipboard failed: " << ::GetLastError() << std::endl;
				return;
			}
			::SetClipboardData(saveClipboardFormat, nullptr);
			std::cerr << "announced save on clipboard" << std::endl;
		}

		GetClipboardDataResult GetClipboardData() final override
		{
			// Note that the data from the local clipboard is left alone if any error occurs so
			// the local clipboard keeps working even in the worst case.
			if (!saveClipboardFormat)
			{
				std::cerr << "cannot get save from clipboard: save clipboard format not registered" << std::endl;
				return GetClipboardDataUnknown{};
			}
			ClipboardSession cs(ourHwnd);
			if (!cs)
			{
				std::cerr << "cannot get save from clipboard: OpenClipboard failed: " << ::GetLastError() << std::endl;
				return GetClipboardDataUnknown{};
			}
			auto newSeqNumber = ::GetClipboardSequenceNumber();
			if (seqNumber && newSeqNumber && seqNumber == newSeqNumber)
			{
				std::cerr << "not getting save from clipboard, data unchanged" << std::endl;
				return GetClipboardDataUnchanged{};
			}
			seqNumber = newSeqNumber;
			if (!::IsClipboardFormatAvailable(saveClipboardFormat))
			{
				std::cerr << "not getting save from clipboard: no data" << std::endl;
				return GetClipboardDataFailed{};
			}
			auto handle = ::GetClipboardData(saveClipboardFormat);
			if (!handle)
			{
				std::cerr << "cannot get save from clipboard: GetClipboardData failed: " << ::GetLastError() << std::endl;
				return GetClipboardDataFailed{};
			}
			auto size = ::GlobalSize(handle);
			auto data = std::unique_ptr<void, decltype(&::GlobalUnlock)>(::GlobalLock(handle), ::GlobalUnlock);
			if (!data)
			{
				std::cerr << "cannot get save from clipboard: GlobalLock failed: " << ::GetLastError() << std::endl;
				return GetClipboardDataFailed{};
			}
			auto base = reinterpret_cast<const char *>(data.get());
			return GetClipboardDataChanged{ std::vector<char>(base, base + size) };
		}

		std::optional<String> Explanation() final override
		{
			return std::nullopt;
		}
	};

	std::unique_ptr<ClipboardImpl> WindowsClipboardFactory()
	{
		return std::make_unique<WindowsClipboardImpl>();
	}
}
