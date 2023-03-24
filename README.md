# DerGaijin - CmdIO

This library provides an interface for reading input asynchronous from the console.

The library is implemented as a static class that provides static methods for enabling/disabling input, checking for input availability, and reading input.
It uses a thread for capturing console input, and it supports two modes of input: line and character. In line mode, input is captured one line at a time, whereas in character mode, each keystroke is captured.

## Features

* Asynchronous I/O
* Simple usage
* Output streams redirected
* Thread-safety
* Cursor movement
* Supported Platforms: Windows and Linux

## Example

```cpp
#include "CmdIO.h"
using namespace DerGaijin;
#include <iostream>

void OutputThreadFunc()
{
	while (true)
	{
		std::cout << "Hello World" << std::endl;
		std::this_thread::sleep_for(std::chrono::milliseconds(500));
	}
}

int main()
{
	std::thread OutputThread = std::thread(&OutputThreadFunc);
	CmdIO::EnableInput();
	CmdIO::SetPrefix(L"> ");
	while (true)
	{
		if (CmdIO::WaitInput())
		{
			std::wcout << CmdIO::GetPrefix() << CmdIO::Input() << std::endl;
		}
	}
	return 0;
}

```
