#include <iostream>
#include <string>
#include <memory>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <Psapi.h>

constexpr std::size_t PROCESS_IDS_INITIAL_ARRAY_SIZE = 1024;

int main(int argc, char** argv)
{
	std::string targetProcess;
	std::string dllToInject;

	// parse command line args
	try
	{
		for(int i = 1; i < argc; ++i)
		{
			std::string arg(argv[i]);

			if(arg == "-t")
			{
				if(i + 1 < argc)
					targetProcess = argv[++i];
				else
					throw std::runtime_error("value for argument '-t' missing.");
			}
			else if(arg == "-i")
			{
				if(i + 1 < argc)
					dllToInject = argv[++i];
				else
					throw std::runtime_error("value for argument '-i' missing.");
			}
			else
			{
				std::string msg = "Unknown argument: '";
				msg += arg + "'";
				throw std::runtime_error(msg);
			}
		}

		if(targetProcess.empty() || dllToInject.empty())
			throw std::runtime_error("Missing 1 or more arguments.");
	}
	catch(const std::runtime_error& re)
	{
		std::cerr << re.what() << '\n';
		return 1;
	}

	// enumerate processes, using larger arrays while there are (possibly) more processes than the current array can hold
	std::unique_ptr<DWORD[]> processIds;
	DWORD bytesRet = 0;
	DWORD currentSize = PROCESS_IDS_INITIAL_ARRAY_SIZE;

	do
	{
		processIds = std::make_unique<DWORD[]>(currentSize);

		if(EnumProcesses(processIds.get(), currentSize * sizeof(DWORD), &bytesRet) == 0)
		{
			std::cerr << "Failed to enumerate processes...\n";
			return 2;
		}

		currentSize *= 2;
	}
	while(currentSize == bytesRet);

	// search for target process
	std::size_t numProcesses = bytesRet / sizeof(DWORD);
	DWORD targetID = 0;

	for(auto i = 0u; i < numProcesses; ++i)
	{
		HANDLE processHandle = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processIds[i]);

		if(processHandle != nullptr)
		{
			HMODULE module;
			DWORD bytesNeeded;

			std::string processName(MAX_PATH, 0);

			if(EnumProcessModules(processHandle, &module, sizeof(module), &bytesNeeded))
			{
				GetModuleBaseName(processHandle, module, &processName[0], MAX_PATH);
			}

			// change size down to get rid of null characters (for comparison)
			processName.resize(processName.find('\0', 0));

			if(processName == targetProcess)
				targetID = processIds[i];
		}

		CloseHandle(processHandle);
	}

	if(targetID == 0)
	{
		std::cerr << "Could not find targeted process.\n";
		return 3;
	}

	// get handle to target process
	HANDLE targetHandle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, targetID);

	// allocate memory in target process
	auto* ptr = VirtualAllocEx(targetHandle, nullptr, dllToInject.size() + 1, MEM_COMMIT, PAGE_EXECUTE_READWRITE);
	
	if(ptr == nullptr)
	{
		auto err = GetLastError();

		char* buf = nullptr;
		
		FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, err, MAKELANGID(LANG_ENGLISH, SUBLANG_DEFAULT), reinterpret_cast<LPTSTR>(&buf), 0, nullptr);

		std::cerr << "Error allocating memory in target process:\n";
		std::cerr << buf << '\n';
		return 4;
	}

	// put injection dll name in allocated memory
	char* charPtr = static_cast<char*>(ptr);
	WriteProcessMemory(targetHandle, charPtr, dllToInject.data(), dllToInject.size(), nullptr);
	WriteProcessMemory(targetHandle, &charPtr[dllToInject.size()], "\0", 1, nullptr);
	
	// launch a thread in target process that calls LoadLibrary, using the memory previously allocated as the library file to load
	// calls the library's DllMain
	if(CreateRemoteThread(targetHandle, nullptr, 0, reinterpret_cast<LPTHREAD_START_ROUTINE>(LoadLibrary), ptr, 0, nullptr) != nullptr)
	{
		std::cout << "Success!\n";
	}
	else
	{
		std::cout << "Failure.\n";
	}

	// we done
	CloseHandle(targetHandle);

	return 0;
}
