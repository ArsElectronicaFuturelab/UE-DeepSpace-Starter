// Copyright Epic Games, Inc. All Rights Reserved.

#include "ARSFileManagerBPLibrary.h"
#include "ARSFileManager.h"

#include <iostream>
#include <string>
#include <regex>

#include "Engine.h"
#include "HAL/FileManager.h"

#include "GenericPlatform/GenericPlatformFile.h"
#include "Developer/DesktopPlatform/Public/DesktopPlatformModule.h"

#include "Misc/Paths.h"
#include "Misc/FileHelper.h"


UARSFileManagerBPLibrary::UARSFileManagerBPLibrary(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{

}


bool UARSFileManagerBPLibrary::CopyDirectory(FString DestinationDirectory, FString Source, bool IsOverwritingAllowed)
{
	if (!IFileManager::Get().DirectoryExists(*Source))
	{
		// UE_LOG(AEFileManager, Error, TEXT("Directory does not exist (copy directory) %s"), *Source);
		return false;
	}
	return IPlatformFile::GetPlatformPhysical().CopyDirectoryTree(*DestinationDirectory, *Source, IsOverwritingAllowed);
}



bool UARSFileManagerBPLibrary::CopyFile(FString DestinationFilePath, FString SourceFilePath)
{
	if (!IFileManager::Get().FileExists(*SourceFilePath)) {
		return false;
	}
	return IPlatformFile::GetPlatformPhysical().CopyFile(*DestinationFilePath, *SourceFilePath);
}

bool UARSFileManagerBPLibrary::DeleteDirectoryRecursively(FString Directory)
{
	if (!IFileManager::Get().DirectoryExists(*Directory))
	{
		// UE_LOG(AEFileManager, Error, TEXT("Directory does not exist (delete directory recursively) %s"), *Directory);
		return false;
	}
	return IPlatformFile::GetPlatformPhysical().DeleteDirectoryRecursively(*Directory);
}

TArray<FString> UARSFileManagerBPLibrary::GetFileNamesFromDirectory(FString DirectoryName)
{
	TArray<FString> FoundFiles;

	if (!IFileManager::Get().DirectoryExists(*DirectoryName))
	{
		// UE_LOG(AEFileManager, Error, TEXT("Directory does not exist (get filename from directory)"));
		return FoundFiles;
	}

	IFileManager::Get().FindFiles(FoundFiles, *DirectoryName);
	if (FoundFiles.Num() <= 0)
	{
		// UE_LOG(AEFileManager, Error, TEXT("No Files Found"));
	}
	return FoundFiles;
}

TArray<FString> UARSFileManagerBPLibrary::GetFileNamesFromDirectoryWithFilterExtension(FString DirectoryName, FString FileExtension)
{
	TArray<FString> FoundFiles;

	if (!IFileManager::Get().DirectoryExists(*DirectoryName))
	{
		// UE_LOG(AEFileManager, Error, TEXT("Directory does not exist (Get filename from directory with extension). %s"), *DirectoryName);
		return FoundFiles;
	}

	IFileManager::Get().FindFiles(FoundFiles, *DirectoryName, *FileExtension);
	if (FoundFiles.Num() <= 0)
	{
		// UE_LOG(AEFileManager, Error, TEXT("No Files Found. (Get files from directory with filter extension)"));
	}
	return FoundFiles;
}

TArray<FString> UARSFileManagerBPLibrary::GetDirectoryNamesFormDirectory(FString DirectoryName)
{
	TArray<FString> FoundDirectories;
	FPaths::NormalizeDirectoryName(DirectoryName);
	if (!IFileManager::Get().DirectoryExists(*DirectoryName))
	{
		// UE_LOG(AEFileManager, Error, TEXT("Directory does not exist (Get directory names from directory). %s"), *DirectoryName);
		return FoundDirectories;
	}

	DirectoryName = DirectoryName / "*";
	IFileManager::Get().FindFiles(FoundDirectories, *DirectoryName, false, true);
	if (FoundDirectories.Num() <= 0)
	{
		// UE_LOG(AEFileManager, Error, TEXT("No Files Found. (Get directory names from directory)"));
	}
	return FoundDirectories;
}

float UARSFileManagerBPLibrary::GetFileSize(FString DirectoryName, FString FileName)
{
	const FString FilePath = DirectoryName + "\\" + FileName;
	if (!IFileManager::Get().FileExists(*FilePath))
	{
		// UE_LOG(AEFileManager, Error, TEXT("File does not exists (file Size)."));
		return -1;
	}

	return IFileManager::Get().FileSize(*FilePath);
}

FString UARSFileManagerBPLibrary::GetCreationTime(FString DirectoryName, FString FileName)
{
	const FString FilePath = DirectoryName + "\\" + FileName;
	if (!IFileManager::Get().FileExists(*FilePath))
	{
		// UE_LOG(AEFileManager, Error, TEXT("File does not exists (creation time)."));
		return "";
	}

	FFileStatData fileStatData = IFileManager::Get().GetStatData(*FilePath);
	return fileStatData.CreationTime.ToString();
}

FString UARSFileManagerBPLibrary::ReadFile(FString DirectoryName, FString FileName)
{
	std::string data = UARSFileManagerBPLibrary::ReadFileFromDirectoryAndFile(DirectoryName, FileName);
	return data.c_str();
}

bool UARSFileManagerBPLibrary::SaveFile(FString DirectoryName, FString FileName, FString Content)
{
	const FString FilePath = DirectoryName + "\\" + FileName;
	if (IFileManager::Get().FileExists(*FilePath))
	{
		IFileManager::Get().Delete(*FilePath);
	}
	return FFileHelper::SaveStringToFile(Content, *FilePath, FFileHelper::EEncodingOptions::AutoDetect, &IFileManager::Get(), EFileWrite::FILEWRITE_Append);
}

std::string UARSFileManagerBPLibrary::ReadFileFromDirectoryAndFile(FString DirectoryName, FString FileName)
{
	using namespace std;
	const FString FilePath = DirectoryName + "\\" + FileName;
	string FileLocation(TCHAR_TO_UTF8(*FilePath));
	ifstream t(FileLocation);
	string data((std::istreambuf_iterator<char>(t)),
		istreambuf_iterator<char>()
	);
	return data;
}

bool UARSFileManagerBPLibrary::IsFileExisting(FString FileLocation)
{
	return IFileManager::Get().FileExists(*FileLocation);
}

bool UARSFileManagerBPLibrary::DeleteFile(FString FileLocation)
{
	if (!IsFileExisting(FileLocation))
	{
		return false;
	}

	return IFileManager::Get().Delete(*FileLocation);
}

bool UARSFileManagerBPLibrary::RenameFile(FString OldFileLocation, FString NewFileLocation)
{
	if (!IsFileExisting(OldFileLocation))
	{
		return false;
	}

	return IFileManager::Get().Move(*NewFileLocation, *OldFileLocation);
}

FString UARSFileManagerBPLibrary::GetValueFromIniFile(FString Value, FString FilePath)
{
	using namespace std;
	FString Result = "Not Found";

	ifstream InFile;
	string ValueToCompare = (TCHAR_TO_UTF8(*Value));

	// open the file stream
	InFile.open(TCHAR_TO_UTF8(*FilePath));

	// check if opening a file failed
	if (InFile.fail()) {
		cerr << "Error opening a file" << endl;
		InFile.close();
		return Result;
	}

	string Line;
	while (getline(InFile, Line))
	{
		if (Line.find(ValueToCompare))
		{
			Result = FString(Line.c_str());
		}
	}
	// close the file stream
	InFile.close();

	return Result;
}