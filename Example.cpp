#include "Console.h"
using namespace DerGaijin;

void OutputThreadFunc()
{
	while (true)
	{
		Console::Output("Hello World");
		std::this_thread::sleep_for(std::chrono::milliseconds(500));
	}
}

int main()
{
	std::thread OutputThread = std::thread(&OutputThreadFunc);
	
	Console::EnableInput();
	Console::SetInputPrefix("> ");
	
	while (true)
	{
		if (Console::WaitInput())
		{
			std::wstring Input = Console::GetInputPrefix() + Console::Input();
			Console::Output(Input);
		}
	}
	
	return 0;
}
