#pragma once
#include <string>
#include <thread>
#include <mutex>
#include <queue>

namespace DerGaijin
{
	class Console
	{
	public:
		Console() = delete;

		static void Output(const std::wstring& Str);

		static void Output(const std::string& Str);

		static void EnableInput();
		
		static void SetInputPrefix(const std::string& Prefix);
		
		static void SetInputPrefix(const std::wstring& Prefix);

		static std::wstring GetInputPrefix();

		static bool HasInput();

		static std::wstring Input();

		static bool WaitInput();
		
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

		static void DisableInput();

	private:
		static void InputThreadFunc();

		static size_t ConsoleLineWidth();

		static size_t LineCount(size_t LineWidth, size_t Width, bool Equal = false);

		static void GetClearInputPreview(std::wstring& Result);

		static void GetInputPreview(std::wstring& Result);

	private:
		static std::atomic<bool> m_InputEnabled;
		static std::thread m_InputThread;

		static std::mutex m_InputMutex;
		static std::wstring m_InputPrefix;
		static std::wstring m_Input;
		static size_t m_CursorPos;
		static bool m_Replace;
		static std::queue<std::wstring> m_InputQueue;
		static std::condition_variable m_InputNotifier;
	};
}
