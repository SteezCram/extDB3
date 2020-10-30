/*
 * extDB3
 * © 2016 Declan Ireland <https://bitbucket.org/torndeco/extdb3>
 */

#include "ext.h"

#include <boost/filesystem.hpp>

#pragma warning(disable : 4996)


Ext *extension;


#ifdef __GNUC__
	#include <dlfcn.h>
	// Code for GNU C compiler
	static void __attribute__((constructor))
	extension_init(void)
	{

		Dl_info dl_info;
		dladdr((void*)extension_init, &dl_info);
		extension = new Ext(boost::filesystem::path (dl_info.dli_fname).string());
	}

	static void __attribute__((destructor))
	extension_destroy(void)
	{
		extension->stop();
	}

	extern "C"
	{
		void RVExtension(char *output, int outputSize, const char *function);
		void RVExtensionVersion(char* output, int outputSize);
		int RVExtensionArgs(char* output, int outputSize, const char* function, const char** argv, int argc);
	};

	void RVExtension(char *output, int outputSize, const char *function)
	{
		outputSize -= 1;
		extension->callExtension(output, outputSize, function);
	};

	int RVExtensionArgs(char* output, int outputSize, const char* function, const char** argv, int argc) {
		std::strncpy(output, "not supported", outputSize - 1);
		return 0;
	}

	void RVExtensionVersion(char* output, int outputSize) {
		std::strncpy(output, "extDB3 v1033 Linux", outputSize - 1);
	}


#elif _MSC_VER
	// Code for MSVC compiler
	//#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers   // Now Defined VIA CMake Build System

	#include <windows.h>
	#include <shellapi.h>

	EXTERN_C IMAGE_DOS_HEADER __ImageBase;

	BOOL APIENTRY DllMain(HMODULE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved)
	{
		switch (ul_reason_for_call)
		{
			case DLL_PROCESS_ATTACH:
				{
					int nArgs;
					LPWSTR *pszArgsW = CommandLineToArgvW(GetCommandLineW(), &nArgs);
					std::unordered_map<std::string, std::string> options;

					WCHAR path[MAX_PATH + 1];
					GetModuleFileNameW((HINSTANCE)&__ImageBase, path, (MAX_PATH + 1));
					extension = new Ext(boost::filesystem::path(path).string());
				}
				break;
			case DLL_PROCESS_DETACH:
				extension->stop();
				break;
			case DLL_THREAD_ATTACH:
			case DLL_THREAD_DETACH:
				break;
		}
		return TRUE;
	}

	extern "C"
	{
		__declspec(dllexport) void __stdcall RVExtension(char *output, int outputSize, const char *function);
		__declspec(dllexport) int __stdcall RVExtensionArgs(char* output, int outputSize, const char* function, const char** argv, int argc);
		__declspec(dllexport) void __stdcall RVExtensionVersion(char* output, int outputSize);
	};

	void __stdcall RVExtension(char *output, int outputSize, const char *function) {
		outputSize -= 1;
		extension->callExtension(output, outputSize, function);
	};

	int RVExtensionArgs(char* output, int outputSize, const char* function, const char** argv, int argc) {
		std::strncpy(output, "not supported", outputSize - 1);
		return 0;
	}

	void RVExtensionVersion(char* output, int outputSize) {
		std::strncpy(output, "extDB3 v1033 Windows", outputSize - 1);
	}
#endif
