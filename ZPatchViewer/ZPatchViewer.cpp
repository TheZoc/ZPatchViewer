#include "stdafx.h"
#include "ZPatchViewer.h"
#include <assert.h>

static const int ZPATCH_HEADER_SIZE = 9;

// {881648ac-35f3-4941-8daa-387984d874fd}
static const GUID GUIDPlugin_ZPATCH =
{ 0x881648ac, 0x35f3, 0x4941,{ 0x8d, 0xaa, 0x38, 0x79, 0x84, 0xd8, 0x84, 0xfd } };

ZPATCHVIEWER_API BOOL DVP_InitEx(LPDVPINITEXDATA pInitExData)
{
	// Apparently, there is a bug in Directory Opus that it sends the version in a wrong manner to DVP_InitEx().
	// This area handles this issue.
	union DopusVersion
	{
		struct 
		{
			DWORD minor;
			DWORD major;
		} win32ver;
		struct
		{
			WORD build;
			WORD minor;
			WORD major;
			WORD version;
		} splitver;
	};

	// Convert the version to human-readable format
	DopusVersion Version;
	Version.win32ver.major = pInitExData->dwOpusVerMajor;
	Version.win32ver.minor = pInitExData->dwOpusVerMinor;

	if (Version.splitver.version < 12)
		return false;

	// A bug was fixed in 12.25.4 beta, that prevented the plugin from working. Do not allow older versions to initialize the plugin.
	if (Version.splitver.version == 12)
	{
		if (Version.splitver.major < 26)
		{
			if (Version.splitver.major == 25 && Version.splitver.minor >= 4)
			{
				return true;
			}
			return false;
		}
	}

	return true;
}

ZPATCHVIEWER_API BOOL DVP_Identify(LPVIEWERPLUGININFO lpVPInfo)
{
	// Configure plugin support
	lpVPInfo->dwFlags =	// DVPFIF_CanHandleStreams |			// Needs DVP_IdentifyFileStream (Apparently,  DVP_LoadBitmapStream  too!)
						DVPFIF_CanHandleBytes |				// Needs DVP_IdentifyFileBytes
						DVPFIF_ExtensionsOnlyIfSlow |
						DVPFIF_ExtensionsOnlyIfNoRndSeek |
						DVPFIF_ExtensionsOnlyForThumbnails |
						DVPFIF_NeedRandomSeek;

	// Version number (1.0.0.2)
	lpVPInfo->dwVersionHigh = MAKELPARAM(0, 1);
	lpVPInfo->dwVersionLow = MAKELPARAM(2, 0);

	// Handle ".zpatch" files
	StringCchCopy(lpVPInfo->lpszHandleExts,		lpVPInfo->cchHandleExtsMax,			TEXT(".zpatch"));

	// Plugin Information
	StringCchCopy(lpVPInfo->lpszName,			lpVPInfo->cchNameMax,				TEXT("ZPatchViewer"));
	StringCchCopy(lpVPInfo->lpszDescription,	lpVPInfo->cchDescriptionMax,		TEXT("This plugin allows the user to vire ZPatch file data contents in Directory Opus"));
	StringCchCopy(lpVPInfo->lpszCopyright,		lpVPInfo->cchCopyrightMax,			TEXT("(C) Copyright 2016-2021 Felipe Guedes da Silveira"));
	StringCchCopy(lpVPInfo->lpszURL,			lpVPInfo->cchURLMax,				TEXT("https://github.com/TheZoc/ZPatchViewer"));

	// The absolute minimum file size for a ZPatch file is 19 (A single delete file operation in a file with a single character, on ZPATCH version 1.0)
	lpVPInfo->dwlMinFileSize = 19;
	lpVPInfo->dwlMinPreviewFileSize = 19;

	lpVPInfo->uiMajorFileType = DVPMajorType_Image; // Maybe call this as text for thumbnails?

	// Our GUID to uniquely identify us to DOpus
	lpVPInfo->idPlugin = GUIDPlugin_ZPATCH;

	return true;
}

ZPATCHVIEWER_API BOOL DVP_IdentifyFileBytes(HWND hWnd, LPTSTR lpszName, LPBYTE lpData, UINT uiDataSize, LPVIEWERPLUGINFILEINFO lpVPFileInfo, DWORD dwStreamFlags)
{
	if (uiDataSize < ZPATCH_HEADER_SIZE || !FileHasZPatchHeader(lpData))
		return false;

	// Fill in the required information fields
	lpVPFileInfo->dwFlags = DVPFIF_ReturnsText;
	if (lpVPFileInfo->lpszInfo)
		StringCchPrintf(lpVPFileInfo->lpszInfo, lpVPFileInfo->cchInfoMax, TEXT("@Bytes@ ZPatch File v%u"), lpData[7]);

	lpVPFileInfo->dwPrivateData[0] = lpData[7];

	return true;
}

ZPATCHVIEWER_API BOOL DVP_LoadText(LPDVPLOADTEXTDATA lpLoadTextData)
{
	if (lpLoadTextData->dwStreamFlags & DVPSF_NoRandomSeek)
		return false;

	lpLoadTextData->iOutTextType = DVPText_Plain;		// TODO: Format text

	//////////////////////////////////////////////////////////////////////////
	LPSTREAM FileStream = nullptr;

	if (!(lpLoadTextData->dwFlags & DVPCVF_FromStream))
	{
		if (SHCreateStreamOnFileEx(lpLoadTextData->lpszFile, STGM_READ | STGM_SHARE_DENY_WRITE | STGM_FAILIFTHERE, 0, false, nullptr, &FileStream) != S_OK)
			return false;
	}
	else
	{
		FileStream = lpLoadTextData->lpInStream;
	}

	std::queue<FileOperation> FileOperationList;
	LARGE_INTEGER	SeekForward;
	FileOperation	fo;
	DWORD			bytesRead;
	HRESULT			ReadResult = S_OK;
	uint64_t		TargetFileLength;
	int64_t			CompressedDataLength;

	// Read header data
	BYTE header[ZPATCH_HEADER_SIZE];
	if (FileStream->Read(header, sizeof(header), &bytesRead) != S_OK || bytesRead != sizeof(header))
	{
		// Close our stream, if we opened it.
		if (!(lpLoadTextData->dwFlags & DVPCVF_FromStream) && FileStream)
			FileStream->Release();

		return false;
	}

	//////////////////////////////////////////////////////////////////////////

	if (header[7] > 1) // Version 1 is unsupported.
	{
		while (ReadResult == S_OK)
		{
			SeekForward.QuadPart = 0;
			TargetFileLength = 0;
			fo.FileName.clear();
			fo.Operation = PatchOperation::Patch_Unknown;

			// Read the operation
			ReadResult = FileStream->Read(&fo.Operation, sizeof(BYTE), &bytesRead);
			//		assert(bytesRead == sizeof(fo.Operation));
			if (ReadResult == S_FALSE) break;

			// Read the length of the filename, in bytes
			FileStream->Read(&TargetFileLength, sizeof(TargetFileLength), &bytesRead);
			assert(bytesRead == sizeof(TargetFileLength));

			// Prepare the string and read the filename
			fo.FileName.resize(TargetFileLength, '\0');
			FileStream->Read(&(fo.FileName[0]), (ULONG)(sizeof(BYTE) * TargetFileLength), &bytesRead);
			assert(bytesRead == TargetFileLength * sizeof(BYTE));

			// The data below only exists when not deleting a file.
			if (fo.Operation != PatchOperation::Patch_File_Delete && fo.Operation != PatchOperation::Patch_Dir_Add)
			{
				// Read the size of the compressed data
				FileStream->Read(&CompressedDataLength, sizeof(CompressedDataLength), &bytesRead);
				assert(bytesRead == sizeof(CompressedDataLength));

				// Skip the compressed data
				SeekForward.QuadPart = CompressedDataLength;
				FileStream->Seek(SeekForward, STREAM_SEEK_CUR, nullptr);
			}

			FileOperationList.push(fo);
		}
	}

	// Close our stream, if we opened it.
	if (!(lpLoadTextData->dwFlags & DVPCVF_FromStream) && FileStream)
		FileStream->Release();

	//////////////////////////////////////////////////////////////////////////

	ULONG bytesWritten;

	// Create and verify our IStream object
	if (CreateStreamOnHGlobal(nullptr, true, &lpLoadTextData->lpOutStream) != S_OK)
	{
		lpLoadTextData->lpOutStream = nullptr;
		return false;
	}

	// BOM and End-Line characters
//	const wchar_t wcUTF16BOM = TEXT('\xFEFF');
//	const wchar_t wcUTF16LineBreak = TEXT('\x000A');
	char mbLineBreak = '\n';

	try
	{
		HRESULT res;
		std::string WriteString;

		// Add the header
		WriteString = "ZPatch file - Version " + std::to_string((UINT)(header[7])) + mbLineBreak + mbLineBreak;
		res = lpLoadTextData->lpOutStream->Write(WriteString.c_str(), (ULONG)(sizeof(WriteString[0]) * WriteString.size()), &bytesWritten);

		// Check if write was successful
		if (res != S_OK || (ULONG)(sizeof(WriteString[0]) * WriteString.size()) != WriteString.size()) throw;

		while (!FileOperationList.empty())
		{
			FileOperation FOItem = FileOperationList.front();
			FileOperationList.pop();

			// Write the operation
			switch ((PatchOperation)(FOItem.Operation))
			{
			case Patch_File_Delete:
				WriteString = "[Delete ]\t";
				break;
			case Patch_File_Add:
				WriteString = "[Add    ]\t";
				break;
			case Patch_File_Replace:
				WriteString = "[Replace]\t";
				break;
			case Patch_Dir_Add:
				WriteString = "[Add Dir]\t";
				break;
			default:
				WriteString = "[Unknown]\t";
				break;
			}

			// Add the file name with full path and Line Ending
			WriteString += FOItem.FileName + mbLineBreak;

			// Write to the stream
			res = lpLoadTextData->lpOutStream->Write(WriteString.c_str(), (ULONG)(WriteString.size() * sizeof(WriteString[0])), &bytesWritten);

			// Check if write was successful
			if (res != S_OK || (ULONG)(sizeof(WriteString[0]) * WriteString.size()) != WriteString.size()) throw;

		}
	}
	catch (...)
	{
		// If anything go wrong in the write process, release the stream and set it to nullptr
		lpLoadTextData->lpOutStream->Release();
		lpLoadTextData->lpOutStream = nullptr;
	}

	return true;
}

bool FileHasZPatchHeader(const BYTE* Header)
{
	// Adapted from ZPatcher::ReadPatchFileHeader();
	if (Header[0] != 'Z' ||
		Header[1] != 'P' ||
		Header[2] != 'A' ||
		Header[3] != 'T' ||
		Header[4] != 'C' ||
		Header[5] != 'H' ||
		Header[6] != '\u001A')
	{
		return false;
	}

	return true;
}
