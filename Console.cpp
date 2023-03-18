#include "Console.h"

#include <iostream>
#include <conio.h>
#include <windows.h>
#undef min
#undef max


namespace DerGaijin
{
	std::mutex Console::m_IOMutex;
	size_t Console::m_Column = 0;
	std::wstreambuf* Console::m_WCoutStreamBuf = nullptr;
	std::streambuf* Console::m_CoutStreamBuf = nullptr;
	std::atomic<bool> Console::m_InputEnabled;
	std::thread Console::m_InputThread;
	std::wstring Console::m_InputPrefix;
	std::wstring Console::m_Input;
	size_t Console::m_CursorPos = 0;
	bool Console::m_Replace = false;
	std::queue<std::wstring> Console::m_InputQueue;
	std::condition_variable Console::m_InputNotifier;

	template<typename Char>
	class ConsoleRedirect : public std::basic_streambuf<Char>
	{
	protected:
		virtual std::streamsize xsputn(typename std::basic_streambuf<Char>::char_type const* s, std::streamsize count) override
		{
			Console::Output(std::basic_string<Char>(s, count));
			return count;
		}

		virtual typename std::basic_streambuf<Char>::int_type overflow(typename std::basic_streambuf<Char>::int_type C) override
		{
			Console::Output(std::basic_string<Char>((Char*)&C, 1));
			return 0;
		}
	};
	ConsoleRedirect<wchar_t> WCoutRedirect;
	ConsoleRedirect<char> CoutRedirect;

	void Console::Output(const std::wstring& Str)
	{
		if (Str.size() == 0)
		{
			return;
		}

		std::lock_guard<std::mutex> Lock(m_IOMutex);

		std::wstring Result;

		if (m_InputEnabled)
		{
			AddInputRemove(Result);
		}

		Result.reserve(Result.size() + Str.size());
		for (auto& C : Str)
		{
			Result += C;
			m_Column++;
			if (C == '\n' || C == '\r')
			{
				m_Column = 0;
			}
		}

		if (m_InputEnabled)
		{
			AddInputPreview(Result);
		}

		Write(Result);
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
			m_InputThread = std::thread(&Console::InputThread);

			m_WCoutStreamBuf = std::wcout.rdbuf();
			m_CoutStreamBuf = std::cout.rdbuf();

			std::wcout.rdbuf(&WCoutRedirect);
			std::cout.rdbuf(&CoutRedirect);

			std::lock_guard<std::mutex> Lock(m_IOMutex);
			std::wstring InputPreview;
			AddInputPreview(InputPreview);
			Write(InputPreview);
		}
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

			std::wcout.rdbuf(m_WCoutStreamBuf);
			std::cout.rdbuf(m_CoutStreamBuf);

			m_WCoutStreamBuf = nullptr;
			m_CoutStreamBuf = nullptr;

			std::lock_guard<std::mutex> Lock(m_IOMutex);
			std::wstring InputRemove;
			AddInputRemove(InputRemove);
			Write(InputRemove);
		}
	}

	void Console::SetInputPrefix(const std::wstring& Prefix)
	{
		std::lock_guard<std::mutex> Lock(m_IOMutex);
		std::wstring InputPreviewUpdate;
		AddInputRemove(InputPreviewUpdate);
		m_InputPrefix = Prefix;
		AddInputPreview(InputPreviewUpdate);
		Write(InputPreviewUpdate);
	}

	void Console::SetInputPrefix(const std::string& Prefix)
	{
		SetInputPrefix(std::wstring(Prefix.begin(), Prefix.end()));
	}

	std::wstring Console::GetInputPrefix()
	{
		std::lock_guard<std::mutex> Lock(m_IOMutex);
		return m_InputPrefix;
	}

	bool Console::HasInput()
	{
		std::lock_guard<std::mutex> Lock(m_IOMutex);
		return !m_InputQueue.empty();
	}

	std::wstring Console::Input()
	{
		std::lock_guard<std::mutex> Lock(m_IOMutex);
		std::wstring Input = m_InputQueue.front();
		m_InputQueue.pop();
		return Input;
	}

	bool Console::WaitInput()
	{
		if (m_InputEnabled)
		{
			std::unique_lock<std::mutex> Lock(m_IOMutex);
			m_InputNotifier.wait(Lock, [&]() -> bool { return !m_InputQueue.empty() || !m_InputEnabled.load(); });
			return m_InputEnabled;
		}

		return false;
	}

	void Console::Write(const std::wstring& Str)
	{
		if (m_WCoutStreamBuf != nullptr)
		{
			std::wcout.rdbuf(m_WCoutStreamBuf);
		}
		std::wcout << Str;
		if (m_WCoutStreamBuf != nullptr)
		{
			std::wcout.rdbuf(&WCoutRedirect);
		}
	}

	void Console::InputThread()
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
					std::lock_guard<std::mutex> Lock(m_IOMutex);
					std::wstring InputPreviewResult;
					AddInputRemove(InputPreviewResult);
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
					else if (C == -32 || C == 224)
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

					AddInputPreview(InputPreviewResult);
					Write(InputPreviewResult);
				}
			}
			else
			{
				// Wait 10 Milliseconds for input
				std::this_thread::sleep_for(std::chrono::milliseconds(10));
			}
		}
	}

	size_t Console::MaxLineWidth()
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

	void Console::AddInputRemove(std::wstring& Result)
	{
		size_t ConsoleWidth = MaxLineWidth();
		size_t ColumnLines = LineCount(ConsoleWidth, m_Column);
		size_t AbsColumn = m_Column - (ColumnLines * ConsoleWidth);
		AbsColumn++;	// Fixes the first Column to skip one char?
		size_t InputLines = LineCount(ConsoleWidth, m_InputPrefix.size() + m_Input.size());
		size_t CursorLine = LineCount(ConsoleWidth, m_InputPrefix.size() + m_CursorPos, true);

		// Move Cursor to last line
		if (CursorLine < InputLines)
		{
			InputLines -= (InputLines - CursorLine);
		}

		if (m_Column > 0)
		{
			InputLines++;
		}
		if (InputLines > 0)
		{
			Result += L"\033[" + std::to_wstring(InputLines) + L"F";
		}
		Result += L"\033[" + std::to_wstring(AbsColumn) + L"G";
		Result += L"\x1b[0J";
	}

	void Console::AddInputPreview(std::wstring& Result)
	{
		// Add New Line for Input if needed
		if (m_Column > 0)
		{
			Result += L"\n";
		}

		// Add Input
		Result += m_InputPrefix;
		Result += m_Input;

		size_t ConsoleWidth = MaxLineWidth();
		size_t Lines = LineCount(ConsoleWidth, m_InputPrefix.size() + m_Input.size());
		size_t CurrLine = LineCount(ConsoleWidth, m_InputPrefix.size() + (m_CursorPos + 1));

		size_t Column = m_InputPrefix.size() + m_CursorPos + 1;
		Column -= (CurrLine * ConsoleWidth);

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
