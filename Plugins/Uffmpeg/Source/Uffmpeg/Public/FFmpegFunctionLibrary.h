// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "FFmpegFunctionLibrary.generated.h"

/**
 * 
 */

class UFFmpegDirector;

UCLASS()
class UFFMPEG_API UFFmpegFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
	UFUNCTION(BlueprintCallable)
	static int CreateFFmpegDirector(UWorld* World, FString OutFileName, bool UseGPU,int FPS,bool RTMP,int VideoBitRate,float AudioDelay, float SoundVolume);

	UFUNCTION(BlueprintCallable)
	static UWorld* GetWorldContext(UObject* WorldContextObject);


};
