#include "Console.h"

#include <iostream>
#include <conio.h>
#include <windows.h>
#undef min
#undef max


namespace DerGaijin
{
	std::atomic<bool> Console::m_InputEnabled = false;
	std::thread Console::m_InputThread;
	std::mutex Console::m_InputMutex;
	std::wstring Console::m_InputPrefix;
	std::wstring Console::m_Input;
	size_t Console::m_CursorPos = 0;
	bool Console::m_Replace = false;
	std::queue<std::wstring> Console::m_InputQueue;
	std::condition_variable Console::m_InputNotifier;


	void Console::Output(const std::wstring& Str)
	{
		std::lock_guard<std::mutex> Lock(m_InputMutex);

		std::wstring OutputStr;
		OutputStr.reserve(Str.size() + m_InputPrefix.size() + m_Input.size());
		GetClearInputPreview(OutputStr);
		OutputStr += Str;
		OutputStr += L"\n";
		if (m_InputEnabled)
		{
			GetInputPreview(OutputStr);
		}
		std::wcout << OutputStr;
	}

	void Console::Output(const std::string& Str)
	{
		Output(std::wstring(Str.begin(), Str.end()));
	}

	void Console::EnableInput()
	{
		bool Expected = false;
		if (m_InputEnabled.compare_exchange_weak(Expected, true))
		{
			m_InputThread = std::thread(&Console::InputThreadFunc);
		}
	}

	void Console::SetInputPrefix(const std::wstring& Prefix)
	{
		std::lock_guard<std::mutex> Lock(m_InputMutex);
		std::wstring InputPreviewUpdate;
		GetClearInputPreview(InputPreviewUpdate);
		m_InputPrefix = Prefix;
		GetInputPreview(InputPreviewUpdate);
		std::wcout << InputPreviewUpdate;
	}

	void Console::SetInputPrefix(const std::string& Prefix)
	{
		SetInputPrefix(std::wstring(Prefix.begin(), Prefix.end()));
	}

	std::wstring Console::GetInputPrefix()
	{
		std::lock_guard<std::mutex> Lock(m_InputMutex);
		return m_InputPrefix;
	}

	bool Console::HasInput()
	{
		std::lock_guard<std::mutex> Lock(m_InputMutex);
		return !m_InputQueue.empty();
	}

	std::wstring Console::Input()
	{
		std::lock_guard<std::mutex> Lock(m_InputMutex);
		std::wstring Input = m_InputQueue.front();
		m_InputQueue.pop();
		return Input;
	}

	bool Console::WaitInput()
	{
		if (m_InputEnabled)
		{
			std::unique_lock<std::mutex> Lock(m_InputMutex);
			m_InputNotifier.wait(Lock, [&]() -> bool { return !m_InputQueue.empty() || !m_InputEnabled.load(); });
			return m_InputEnabled;
		}

		return false;
	}

	void Console::DisableInput()
	{
		bool Expected = true;
		if (m_InputEnabled.compare_exchange_weak(Expected, false))
		{
			if (m_InputThread.joinable())
			{
				m_InputThread.join();
			}
			{
				std::lock_guard<std::mutex> Lock(m_InputMutex);
				std::wstring ClearPreviewStr;
				GetClearInputPreview(ClearPreviewStr);
				std::wcout << ClearPreviewStr;
				m_Input.clear();
				m_InputNotifier.notify_all();
			}
		}
	}

	void Console::InputThreadFunc()
	{
		// Skip Input while it was disabled
		while (_kbhit())
		{
			char C = _getch();
		}

		// Main Input Loop
		while (m_InputEnabled)
		{
			if (_kbhit())
			{
				// Get Char without pressing Enter
				int C = _getch();
				{
					std::lock_guard<std::mutex> Lock(m_InputMutex);
					std::wstring InputPreviewResult;
					GetClearInputPreview(InputPreviewResult);
					if (C == '\n' || C == '\r')	// If enter is pressed submit Input
					{
						m_InputQueue.push(m_Input);
						m_Input.clear();
						m_CursorPos = 0;
						m_InputNotifier.notify_one();
					}
					else if (C == '\b' || C == 127)	// If Backspace is pressed erase from Input
					{
						if (m_Input.size() > 0)
						{
							if (m_CursorPos > 0)
							{
								m_Input.erase(m_Input.begin() + (m_CursorPos - 1));
							}
							m_CursorPos = m_CursorPos == 0 ? 0 : m_CursorPos - 1;
						}
					}
					else if (C == -32)
					{
						int P = _getch();
						switch (P)
						{
						case 71:	// Home
							m_CursorPos = 0;
							break;
						case 72:	// Up Arrow
							break;
						case 75:	// Left Arrow
							m_CursorPos = m_CursorPos == 0 ? 0 : m_CursorPos - 1;
							break;
						case 77:	// Right Arrow
							m_CursorPos = std::min(m_Input.size(), m_CursorPos + 1);
							break;
						case 79:	// End
							m_CursorPos = m_Input.size();
							break;
						case 80:	// Down Arrow
							break;
						case 82:	// Insert
							m_Replace = !m_Replace;
							break;
						case 83:	// Del
							if (m_CursorPos < m_Input.size())
							{
								m_Input.erase(m_Input.begin() + m_CursorPos);
							}
							break;
						}
					}
					else
					{
						if (m_CursorPos == m_Input.size())
						{
							m_Input += C;
						}
						else
						{
							if (m_Replace)
							{
								m_Input[m_CursorPos] = C;
							}
							else
							{
								m_Input.insert(m_Input.begin() + m_CursorPos, C);
							}
						}
						m_CursorPos++;
					}

					GetInputPreview(InputPreviewResult);
					std::wcout << InputPreviewResult;
				}
			}
			else
			{
				// Wait 10 Milliseconds for input
				std::this_thread::sleep_for(std::chrono::milliseconds(10));
			}
		}
	}

	size_t Console::ConsoleLineWidth()
	{
#if _WIN32 || _WIN64
		HANDLE ConsoleHandle = GetStdHandle(STD_OUTPUT_HANDLE);

		CONSOLE_SCREEN_BUFFER_INFO cbsi;
		GetConsoleScreenBufferInfo(ConsoleHandle, &cbsi);

		size_t LineWidth = cbsi.dwSize.X;
#else
#error Unsupported Platform
#endif
		return LineWidth;
	}

	size_t Console::LineCount(size_t LineWidth, size_t Width, bool Equal /*= false*/)
	{
		size_t Lines = (Width / LineWidth);

		// if the Width is the same as the last lines width, we dont want a new Line
		if (!Equal && Lines > 0 && LineWidth + (LineWidth * (Lines - 1)) == Width)
		{
			Lines -= 1;
		}

		return Lines;
	}

	void Console::GetClearInputPreview(std::wstring& Result)
	{
		size_t ConsoleWidth = ConsoleLineWidth();
		size_t Lines = LineCount(ConsoleWidth, m_InputPrefix.size() + m_Input.size());
		size_t CurrLine = LineCount(ConsoleWidth, m_InputPrefix.size() + (m_CursorPos + 1));

		// Move Cursor to last line
		if (CurrLine < Lines)
		{
			for (size_t i = 0; i < Lines - CurrLine; i++)
			{
				Result += L"\033[B";
			}
		}

		// Create the Ansi erase String
		Result += L"\x1b[0G\x1b[2K";
		for (size_t i = 0; i < Lines; i++)
		{
			Result += L"\033[A\x1b[0G\x1b[2K";
		}
	}

	void Console::GetInputPreview(std::wstring& Result)
	{
		size_t ConsoleWidth = ConsoleLineWidth();
		size_t Lines = LineCount(ConsoleWidth, m_InputPrefix.size() + m_Input.size());
		size_t CurrLine = LineCount(ConsoleWidth, m_InputPrefix.size() + (m_CursorPos + 1));

		size_t Column = m_InputPrefix.size() + m_CursorPos + 1;
		Column -= (CurrLine * ConsoleWidth);

		Result += m_InputPrefix;
		Result += m_Input;
		if (Lines > CurrLine)
		{
			Result += L"\x1b[";
			Result += std::to_wstring(Lines - CurrLine);
			Result += L"F";
		}
		Result += L"\x1b[";
		Result += std::to_wstring(Column);
		Result += L"G";
	}
}

/*

- Handle Tabs
- Input History
- Auto Completion
- make it faster
- Multi platform Support

*/
