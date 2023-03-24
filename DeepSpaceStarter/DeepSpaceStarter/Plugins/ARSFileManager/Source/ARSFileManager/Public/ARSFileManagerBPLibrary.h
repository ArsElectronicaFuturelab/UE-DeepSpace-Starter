// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <fstream>

#include "Kismet/BlueprintFunctionLibrary.h"
#include "ARSFileManagerBPLibrary.generated.h"

UCLASS()
class ARSFILEMANAGER_API UARSFileManagerBPLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_UCLASS_BODY()
	
	UFUNCTION(BlueprintCallable, Category = "ARS|File Manager")
	static bool CopyDirectory(FString DestinationDirectory, FString Source, bool IsOverwritingAllowed);
	
	UFUNCTION(BlueprintCallable, Category = "ARS|File Manager")
	static bool CopyFile(FString DestinationFilePath, FString SourceFilePath);
	
	UFUNCTION(BlueprintCallable, Category = "ARS|File Manager")
	static bool DeleteDirectoryRecursively(FString Directory);
	
	UFUNCTION(BlueprintCallable, Category = "ARS|File Manager")
	static TArray<FString> GetFileNamesFromDirectory(FString DirectoryName);
	
	UFUNCTION(BlueprintCallable, Category = "ARS|File Manager")
	static TArray<FString> GetFileNamesFromDirectoryWithFilterExtension(FString DirectoryName, FString FileExtension);
	
	UFUNCTION(BlueprintCallable, Category = "ARS|File Manager")
	static TArray<FString> GetDirectoryNamesFormDirectory(FString DirectoryName);
	
	UFUNCTION(BlueprintCallable, Category = "ARS|File Manager")
	static float GetFileSize(FString DirectoryName, FString FileName);
	
	UFUNCTION(BlueprintCallable, Category = "ARS|File Manager")
	static FString GetCreationTime(FString DirectoryName, FString FileName);
	
	UFUNCTION(BlueprintCallable, Category = "ARS|File Manager")
	static FString ReadFile(FString DirectoryName, FString FileName);
	
	UFUNCTION(BlueprintCallable, Category = "ARS|File Manager")
	static bool SaveFile(FString DirectoryName, FString FileName, FString Content);
	
	UFUNCTION(BlueprintPure, Category = "ARS|File Manager")
	static bool IsFileExisting(FString FileLocation);

	UFUNCTION(BlueprintCallable, Category = "ARS|File Manager")
	static bool DeleteFile(FString FileLocation);
	
	UFUNCTION(BlueprintCallable, Category = "ARS|File Manager")
	static bool RenameFile(FString OldFileLocation, FString NewFileLocation);
	static std::string ReadFileFromDirectoryAndFile(FString DirectoryName, FString FileName);

	UFUNCTION(BlueprintPure, Category = "ARS|File Manager")
	static FString GetValueFromIniFile(FString Value, FString FilePath);
};
