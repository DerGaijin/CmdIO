#pragma once
#include <string>
#include <thread>
#include <mutex>
#include <queue>

namespace DerGaijin
{
	class CmdIO
	{
		template<typename Char>
		friend class ConsoleRedirect;

	public:
		enum class EMode
		{
			Line,
			Char,
		};

	public:
		CmdIO() = delete;

		// Enables the Async Input and redirects the output streams
		static void EnableInput(EMode Mode = EMode::Line);

		// Disables the Async Input and reset the output streams to default
		static void DisableInput();

		// Returns the Input Mode
		static EMode GetMode();


		// Sets the Input Prefix
		static void SetPrefix(const std::wstring& Prefix);

		// Returns the Input Prefix
		static std::wstring GetPrefix();


		// Returns true if a Input is ready
		static bool HasInput();

		// Returns the submitted Input or empty string if none was ready
		static std::wstring Input();

		// Returns the current Input
		static std::wstring CurrentInput();


		// Waits for a Input Submit
		static bool WaitInput();

		// Waits for a Input Submit for Time
		template<typename Rep, typename Period>
		static bool WaitInputFor(const std::chrono::duration<Rep, Period>& Time)
		{
			if (m_InputEnabled)
			{
				std::unique_lock<std::mutex> Lock(m_InputMutex);
				if (m_InputNotifier.wait_for(Lock, Time, [&]() -> bool { return !m_InputQueue.empty() || !m_InputEnabled.load(); }))
				{
					return m_InputEnabled;
				}
			}

			return false;
		}

		// Waits for a Input Submit until Timepoint
		template<typename Clock, typename Dur>
		static void WaitInputUntil(const std::chrono::time_point<Clock, Dur>& TimePoint)
		{
			if (m_InputEnabled)
			{
				std::unique_lock<std::mutex> Lock(m_InputMutex);
				if (m_InputNotifier.wait_until(Lock, TimePoint, [&]() -> bool { return !m_InputQueue.empty() || !m_InputEnabled.load(); }))
				{
					return m_InputEnabled;
				}
			}

			return false;
		}

	private:
		static void InputThread();

		static void SubmitInput();

		static size_t MaxLineWidth();

		static size_t LineCount(size_t LineWidth, size_t Width, bool Equal = false);

		static void AddInputRemove(std::wstring& Result);

		static void AddInputPreview(std::wstring& Result);

		static void WriteOutput(const std::wstring& Output);

		static void ProcessRedirectedOutput(const wchar_t* Str, size_t Count);

		static void ProcessRedirectedOutput(const char* Str, size_t Count);

	private:
		// Input Thread
		static std::atomic<bool> m_InputEnabled;
		static std::thread m_InputThread;
		static EMode m_Mode;

		// Input
		static std::mutex m_InputMutex;
		static std::wstring m_InputPrefix;
		static std::wstring m_Input;

		// Input Editing
		static size_t m_CursorPos;
		static bool m_Replace;
		
		// Input Queue
		static std::queue<std::wstring> m_InputQueue;
		static std::condition_variable m_InputNotifier;

		// Output
		static size_t m_Column;
		static std::wstreambuf* m_WCoutStreamBuf;
		static std::streambuf* m_CoutStreamBuf;

	};
}
