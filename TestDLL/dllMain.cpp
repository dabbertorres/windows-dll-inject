#include <iostream>
#include <sstream>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch(ul_reason_for_call)
	{
		case DLL_PROCESS_ATTACH:
			std::cout << "process attach\n";
			break;

		case DLL_THREAD_ATTACH:
			std::cout << "thread attach\n";
			break;

		case DLL_THREAD_DETACH:
			std::cout << "thread detach\n";
			break;

		case DLL_PROCESS_DETACH:
			std::cout << "process detach\n";
			break;
	}

	return TRUE;
}