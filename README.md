# DerGaijin - CmdIO

Simple and easy to use asynchronous console input/output.

## Features

* Asynchronous I/O
* Simple usage
* Output streams redirected
* Thread-safety
* Cursor movement
* Cross-platform

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
