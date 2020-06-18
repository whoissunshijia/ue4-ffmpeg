// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FUFFmpegModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;


private:
	void* LoadLibrary(const  FString& name, const FString& version);
	void* AVUtilLibrary;
	void* SWResampleLibrary;
	void* AVCodecLibrary;
	void* SWScaleLibrary;
	void* AVFormatLibrary;
	void* PostProcLibrary;
	void* AVFilterLibrary;
	void* AVDeviceLibrary;

	bool Initialized;
};
