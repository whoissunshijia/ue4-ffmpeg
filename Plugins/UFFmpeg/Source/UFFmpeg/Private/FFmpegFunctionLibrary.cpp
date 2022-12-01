// Fill out your copyright notice in the Description page of Project Settings.


#include "FFmpegFunctionLibrary.h"
#include "FFmpegDirector.h"
#include <Kismet/GameplayStatics.h>

DEFINE_LOG_CATEGORY(LogFFmpeg);

UFFmpegDirector* UFFmpegFunctionLibrary::CreateFFmpegDirector(UWorld* World, int32 VideoLength, FString OutFileName, FString VideoFilter, bool UseGPU, int FPS, int VideoBitRate, float AudioDelay, float SoundVolume)
{
	UFFmpegDirector* d = NewObject<UFFmpegDirector>();
	d->AddToRoot();
	d->Initialize_Director(World, VideoLength, OutFileName, UseGPU, VideoFilter, FPS, VideoBitRate, AudioDelay, SoundVolume);
	return d;
}


UWorld* UFFmpegFunctionLibrary::GetWorldContext(UObject* WorldContextObject)
{
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject,EGetWorldErrorMode::LogAndReturnNull);
	return World;
}
