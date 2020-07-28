// Fill out your copyright notice in the Description page of Project Settings.


#include "FFmpegFunctionLibrary.h"
#include "FFmpegDirector.h"

int UFFmpegFunctionLibrary::CreateFFmpegDirector(UWorld* World, FString OutFileName, FString VideoFilter, bool UseGPU, int FPS, int VideoBitRate, float AudioDelay, float SoundVolume)
{
	UFFmpegDirector* d = NewObject<UFFmpegDirector>();
	d->AddToRoot();
	d->Initialize_Director(World, OutFileName, UseGPU, VideoFilter, FPS, VideoBitRate, AudioDelay, SoundVolume);
	return 1;
}


UWorld* UFFmpegFunctionLibrary::GetWorldContext(UObject* WorldContextObject)
{
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject);
	return World;
}
