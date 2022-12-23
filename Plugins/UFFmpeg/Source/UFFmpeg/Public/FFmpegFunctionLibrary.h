// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "FFmpegFunctionLibrary.generated.h"

/**
 * 
 */

DECLARE_LOG_CATEGORY_EXTERN(LogFFmpeg, Log, All);

class UFFmpegDirector;

UCLASS()
class UFFMPEG_API UFFmpegFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
	UFUNCTION(BlueprintCallable, Category = "FFMpegEncoder")
	static UFFmpegDirector* CreateFFmpegDirector(UWorld* World,int32 VideoLength, FString OutFileName, FString VideoFilter,bool UseGPU,int FPS,int VideoBitRate,float AudioDelay, float SoundVolume);

	UFUNCTION(BlueprintCallable, Category = "FFMpegEncoder")
	static UWorld* GetWorldContext(UObject* WorldContextObject);


};
