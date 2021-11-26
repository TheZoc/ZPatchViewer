// The following ifdef block is the standard way of creating macros which make exporting 
// from a DLL simpler. All files within this DLL are compiled with the ZPATCHVIEWER_EXPORTS
// symbol defined on the command line. This symbol should not be defined on any project
// that uses this DLL. This way any other project whose source files include this file see 
// ZPATCHVIEWER_API functions as being imported from a DLL, whereas this DLL sees symbols
// defined with this macro as being exported.
#ifdef ZPATCHVIEWER_EXPORTS
#define ZPATCHVIEWER_API __declspec(dllexport)
#else
#define ZPATCHVIEWER_API __declspec(dllimport)
#endif


extern "C"
{
	ZPATCHVIEWER_API BOOL DVP_Init() { return false; };		// Don't support old Directory Opus versions
	ZPATCHVIEWER_API BOOL DVP_InitEx(LPDVPINITEXDATA pInitExData);
	ZPATCHVIEWER_API void DVP_Uninit(void) {};
	ZPATCHVIEWER_API BOOL DVP_USBSafe(LPOPUSUSBSAFEDATA pUSBSafeData) { return true; };

	ZPATCHVIEWER_API BOOL DVP_Identify(LPVIEWERPLUGININFO lpVPInfo);
//	ZPATCHVIEWER_API BOOL DVP_IdentifyFile(HWND hWnd, LPTSTR lpszName, LPVIEWERPLUGINFILEINFO lpVPFileInfo, HANDLE hAbortEvent);
	ZPATCHVIEWER_API BOOL DVP_IdentifyFileBytes(HWND hWnd, LPTSTR lpszName, LPBYTE lpData, UINT uiDataSize, LPVIEWERPLUGINFILEINFO lpVPFileInfo, DWORD dwStreamFlags);
//	ZPATCHVIEWER_API BOOL DVP_IdentifyFileStream(HWND hWnd, LPSTREAM lpStream, LPTSTR lpszName, LPVIEWERPLUGINFILEINFO lpVPFileInfo, DWORD dwStreamFlags);

	ZPATCHVIEWER_API BOOL DVP_LoadText(LPDVPLOADTEXTDATA lpLoadTextData);

}

inline  bool FileHasZPatchHeader(const BYTE* Header);

// Extracted from ZPatcherCurrentVersion.h
enum PatchOperation
{
	Patch_Unknown			= 0,	// Invalid entry - non existent on actual patch files
	Patch_File_Delete		= 1,	// Delete an existing file (Removed on the new version)
	Patch_File_Add			= 2,	// Add a file contained in the patch
	Patch_File_Replace		= 3,	// Replace a file with the one contained in the patch
	Patch_Dir_Add			= 4,	// Add a Directory that is new in the new version
	Patch_MAX				= Patch_Dir_Add,
};

struct FileOperation 
{
	PatchOperation	Operation = Patch_Unknown;
	std::string		FileName;
};