// Fill out your copyright notice in the Description page of Project Settings.


#include "FFmpegFunctionLibrary.h"
#include "FFMPEGDirector.h"

int UFFmpegFunctionLibrary::CreateFFmpegDirector(UWorld* World, FString OutFileName, bool UseGPU, int FPS, bool RTMP, int VideoBitRate, float AudioDelay,float SoundVolume)
{
	UFFmpegDirector* d = NewObject<UFFmpegDirector>();
	d->AddToRoot();
	d->Initialize_Director(World, OutFileName, UseGPU, FPS, RTMP, VideoBitRate, AudioDelay, SoundVolume);
	return 1;
}


UWorld* UFFmpegFunctionLibrary::GetWorldContext(UObject* WorldContextObject)
{
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject);
	return World;
}
