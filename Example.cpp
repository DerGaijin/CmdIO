#include "Console.h"
using namespace DerGaijin;

#include <iostream>

void OutputThreadFunc()
{
	size_t Counter = 0;
	while (true)
	{
		std::cout << "Redirected " << "to " << "the " << "Console" << std::endl;
		
		std::wcout << L"Also" << L" Redirected " << L"to " << L"the " << L"Console" << std::endl;
		
		Console::Output("[" + std::to_string(Counter) + "] ");
		Console::Output("Hello World!\n");

		std::this_thread::sleep_for(std::chrono::milliseconds(400));
		Counter++;
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
			std::wstring Input = Console::GetInputPrefix() + Console::Input() + L"\n";
			Console::Output(Input);
		}
	}

	return 0;
}
