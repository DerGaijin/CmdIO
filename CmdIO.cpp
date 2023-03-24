#include "CmdIO.h"

#include <iostream>
#include <conio.h>

#if _WIN32 || _WIN64
#include <windows.h>
#undef min
#undef max
#elif __linux__
#include <sys/ioctl.h>
#elif __APPLE__
#include <sys/ioctl.h>
#include <unistd.h>
#else
#error Unsupported Platform
#endif


namespace DerGaijin
{
	std::atomic<bool> CmdIO::m_InputEnabled = false;
	std::thread CmdIO::m_InputThread;
	CmdIO::EMode CmdIO::m_Mode = CmdIO::EMode::Line;
	std::mutex CmdIO::m_InputMutex;
	std::wstring CmdIO::m_InputPrefix;
	std::wstring CmdIO::m_Input;
	size_t CmdIO::m_CursorPos = 0;
	bool CmdIO::m_Replace = false;
	std::queue<std::wstring> CmdIO::m_InputQueue;
	std::condition_variable CmdIO::m_InputNotifier;
	size_t CmdIO::m_Column = 0;
	std::wstreambuf* CmdIO::m_WCoutStreamBuf = nullptr;
	std::streambuf* CmdIO::m_CoutStreamBuf = nullptr;

	template<typename Char>
	class ConsoleRedirect : public std::basic_streambuf<Char>
	{
	protected:
		virtual std::streamsize xsputn(typename std::basic_streambuf<Char>::char_type const* s, std::streamsize count) override
		{
			CmdIO::ProcessRedirectedOutput(s, count);
			return count;
		}

		virtual typename std::basic_streambuf<Char>::int_type overflow(typename std::basic_streambuf<Char>::int_type C) override
		{
			CmdIO::ProcessRedirectedOutput((const Char*)&C, 1);
			return 0;
		}
	};
	ConsoleRedirect<wchar_t> WCoutRedirect;
	ConsoleRedirect<char> CoutRedirect;

	void CmdIO::EnableInput(EMode Mode /*= EMode::Line*/)
	{
		// Try to atomically set m_InputEnabled to true if it's currently false
		bool expected = false;
		if (!m_InputEnabled.compare_exchange_weak(expected, true))
		{
			// Input is already enabled
			return;
		}

		// Set Input Mode
		m_Mode = Mode;

		// Start Input Thread
		m_InputThread = std::thread(&CmdIO::InputThread);

		// Backup Output Streams
		m_WCoutStreamBuf = std::wcout.rdbuf();
		m_CoutStreamBuf = std::cout.rdbuf();

		// Redirect Output Streams
		std::wcout.rdbuf(&WCoutRedirect);
		std::cout.rdbuf(&CoutRedirect);

		// Print Preview Prefix
		std::wstring InputEnabled;
		AddInputPreview(InputEnabled);
		WriteOutput(InputEnabled);
	}

	void CmdIO::DisableInput()
	{
		bool Expected = true;
		if (!m_InputEnabled.compare_exchange_weak(Expected, false))
		{
			return;
		}

		// Join Input Thread
		if (m_InputThread.joinable())
		{
			m_InputThread.join();
		}

		// Reset Output Streams
		std::wcout.rdbuf(m_WCoutStreamBuf);
		std::cout.rdbuf(m_CoutStreamBuf);

		// Clear Backup Output Streams
		m_WCoutStreamBuf = nullptr;
		m_CoutStreamBuf = nullptr;

		// Remove Preview Prefix
		std::wstring InputDisabled;
		AddInputRemove(InputDisabled);
		WriteOutput(InputDisabled);
	}

	CmdIO::EMode CmdIO::GetMode()
	{
		return m_Mode;
	}

	void CmdIO::SetPrefix(const std::wstring& Prefix)
	{
		std::lock_guard<std::mutex> Lock(m_InputMutex);
		
		std::wstring PrefixChange;
		AddInputRemove(PrefixChange);
		
		m_InputPrefix = Prefix;
		
		AddInputPreview(PrefixChange);
		WriteOutput(PrefixChange);
	}

	std::wstring CmdIO::GetPrefix()
	{
		std::lock_guard<std::mutex> Lock(m_InputMutex);
		return m_InputPrefix;
	}

	bool CmdIO::HasInput()
	{
		std::lock_guard<std::mutex> Lock(m_InputMutex);
		return !m_InputQueue.empty();
	}

	std::wstring CmdIO::Input()
	{
		std::lock_guard<std::mutex> Lock(m_InputMutex);
		std::wstring Input = m_InputQueue.front();
		m_InputQueue.pop();
		return Input;
	}

	std::wstring CmdIO::CurrentInput()
	{
		std::lock_guard<std::mutex> Lock(m_InputMutex);
		return m_Input;
	}

	bool CmdIO::WaitInput()
	{
		if (m_InputEnabled)
		{
			std::unique_lock<std::mutex> Lock(m_InputMutex);
			m_InputNotifier.wait(Lock, [&]() -> bool { return !m_InputQueue.empty() || !m_InputEnabled.load(); });
			return m_InputEnabled;
		}

		return false;
	}

	void CmdIO::InputThread()
	{
		// Skip Input while it was disabled
		while (_kbhit())
		{
			char C = _getch();
		}

		bool NextIsCtrlChar = false;

		// Main Input Loop
		while (m_InputEnabled)
		{
			if (_kbhit())
			{
				int C = _getch();

				std::lock_guard<std::mutex> Lock(m_InputMutex);
				std::wstring InputPreviewResult;
				if (m_Mode == EMode::Line)
				{
					AddInputRemove(InputPreviewResult);
				}

				bool AddToInput = true;
				switch (C)
				{
				case '\n':
				case '\r':
				{
					AddToInput = false;
					if (m_Mode == EMode::Line)
					{
						SubmitInput();
					}
					break;
				}
				case '\b':	// Backspace
				case 127:	// Other Delete
				{
					if (m_Mode == EMode::Line)
					{
						AddToInput = false;
						if (m_Input.size() > 0)
						{
							if (m_CursorPos > 0)
							{
								m_Input.erase(m_Input.begin() + (m_CursorPos - 1));
							}
							m_CursorPos = m_CursorPos == 0 ? 0 : m_CursorPos - 1;
						}
					}
					break;
				}
				case -32:	// Next Is Ctrl Char
				case 224:
				{
					NextIsCtrlChar = true;
					AddToInput = false;
					break;
				}
				case 71:	// Ctrl Char Home
				{
					if (NextIsCtrlChar)
					{
						NextIsCtrlChar = false;
						m_CursorPos = 0;
					}
					break;
				}
				case 72:	// Ctrl Char Up Arrow
				{
					if (NextIsCtrlChar)
					{
						NextIsCtrlChar = false;
						AddToInput = false;
					}
					break;
				}
				case 75:	// Ctrl Char Left Arrow
				{
					if (NextIsCtrlChar)
					{
						NextIsCtrlChar = false;
						AddToInput = false;
						m_CursorPos = m_CursorPos == 0 ? 0 : m_CursorPos - 1;
					}
					break;
				}
				case 77:	// Ctrl Char Right Arrow
				{
					if (NextIsCtrlChar)
					{
						NextIsCtrlChar = false;
						AddToInput = false;
						m_CursorPos = std::min(m_Input.size(), m_CursorPos + 1);
					}
					break;
				}
				case 79:	// Ctrl Char End
				{
					if (NextIsCtrlChar)
					{
						NextIsCtrlChar = false;
						AddToInput = false;
						m_CursorPos = m_Input.size();
					}
					break;
				}
				case 80:	// Ctrl Char Down Arrow
				{
					if (NextIsCtrlChar)
					{
						NextIsCtrlChar = false;
						AddToInput = false;
					}
					break;
				}
				case 82:	// Ctrl Char Insert
				{
					if (NextIsCtrlChar)
					{
						NextIsCtrlChar = false;
						AddToInput = false;
						m_Replace = !m_Replace;
					}
					break;
				}
				case 83:	// Ctrl Char Del
				{
					if (NextIsCtrlChar)
					{
						NextIsCtrlChar = false;
						AddToInput = false;
						if (m_CursorPos < m_Input.size())
						{
							m_Input.erase(m_Input.begin() + m_CursorPos);
						}
					}
					break;
				}
				}

				if (AddToInput)
				{
					NextIsCtrlChar = false;

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

					if (m_Mode == EMode::Char)
					{
						SubmitInput();
					}
				}

				if (m_Mode == EMode::Line)
				{
					AddInputPreview(InputPreviewResult);
					WriteOutput(InputPreviewResult);
				}
			}
			else
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(10));
			}
		}
	}

	void CmdIO::SubmitInput()
	{
		m_InputQueue.push(m_Input);
		m_Input.clear();
		m_CursorPos = 0;
		m_InputNotifier.notify_one();
	}

	size_t CmdIO::MaxLineWidth()
	{
#if _WIN32 || _WIN64
		HANDLE ConsoleHandle = GetStdHandle(STD_OUTPUT_HANDLE);

		CONSOLE_SCREEN_BUFFER_INFO cbsi;
		GetConsoleScreenBufferInfo(ConsoleHandle, &cbsi);

		size_t LineWidth = cbsi.dwSize.X;
#elif __linux__ || __APPLE__
		struct winsize w;
		ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);

		size_t LineWidth = w.ws_col;
#endif
		return LineWidth;
	}

	size_t CmdIO::LineCount(size_t LineWidth, size_t Width, bool Equal /*= false*/)
	{
		size_t Lines = (Width / LineWidth);

		// if the Width is the same as the last lines width, we dont want a new Line
		if (!Equal && Lines > 0 && LineWidth + (LineWidth * (Lines - 1)) == Width)
		{
			Lines -= 1;
		}

		return Lines;
	}

	void CmdIO::AddInputRemove(std::wstring& Result)
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

	void CmdIO::AddInputPreview(std::wstring& Result)
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

	void CmdIO::WriteOutput(const std::wstring& Output)
	{
		if (m_WCoutStreamBuf != nullptr)
		{
			std::wcout.rdbuf(m_WCoutStreamBuf);
		}
		std::wcout << Output;
		if (m_WCoutStreamBuf != nullptr)
		{
			std::wcout.rdbuf(&WCoutRedirect);
		}
	}

	void CmdIO::ProcessRedirectedOutput(const wchar_t* Str, size_t Count)
	{
		if (Count == 0)
		{
			return;
		}

		std::lock_guard<std::mutex> Lock(m_InputMutex);

		std::wstring Result;

		if (m_InputEnabled)
		{
			AddInputRemove(Result);
		}

		Result.reserve(Count);
		for (size_t i = 0; i < Count; i++)
		{
			const wchar_t& C = Str[i];

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

		WriteOutput(Result);
	}

	void CmdIO::ProcessRedirectedOutput(const char* Str, size_t Count)
	{
		if (Count == 0)
		{
			return;
		}

		std::lock_guard<std::mutex> Lock(m_InputMutex);

		std::wstring Result;

		if (m_InputEnabled)
		{
			AddInputRemove(Result);
		}

		Result.reserve(Count);
		for (size_t i = 0; i < Count; i++)
		{
			wchar_t C = Str[i];

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

		WriteOutput(Result);
	}
}
