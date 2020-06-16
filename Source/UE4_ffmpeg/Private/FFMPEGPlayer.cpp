// Fill out your copyright notice in the Description page of Project Settings.
#include "FFMPEGPlayer.h"
#include "FFMPEGDirector.h"

#include "RHI.h"
#include "RenderingThread.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "RunnableThread.h"
#include "AudioThread.h"
#include "CommandLine.h"


extern "C"
{
#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
#include "libavutil/mem.h"
#include "libswscale/swscale.h"
#include "libavutil/file.h"
#include "libavutil/imgutils.h"
#include "libavutil/opt.h"
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavfilter/buffersink.h"
#include "libavfilter/buffersrc.h"
#include "libavutil/opt.h"
#include "libavutil/avutil.h"
#include "libavutil/time.h"
#include "libavutil/error.h"

}

// Sets default values
AFFMPEGPlayer::AFFMPEGPlayer()
{
	
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	f_director = nullptr;
}

// Called when the game starts or when spawned
void AFFMPEGPlayer::BeginPlay()
{
	

}

void AFFMPEGPlayer::BeginDestroy()
{
	Super::BeginDestroy();
	
}



//void AFFMPEGPlayer::GetTexturePixels(FTexture2DRHIRef Texture, TArray<FColor>& OutPixels)
//{
//	struct FReadSurfaceContext
//	{
//		FTexture2DRHIRef Texture;
//		TArray<FColor>* OutData;
//		FIntRect Rect;
//		FReadSurfaceDataFlags Flags;
//	};
//
//	OutPixels.Reset();
//	FReadSurfaceContext ReadSurfaceContext =
//	{
//		Texture,
//		&OutPixels,
//		FIntRect(0, 0, Texture->GetSizeXY().X, Texture->GetSizeXY().Y),
//		FReadSurfaceDataFlags(RCM_UNorm, CubeFace_MAX)
//	};
//
//	//#define ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(TypeName,ParamType1,ParamName1,ParamValue1,Code) 
//	ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
//		ReadSurfaceCommand,
//		FReadSurfaceContext, Context, ReadSurfaceContext,
//		{
//			RHICmdList.ReadSurfaceData(
//				Context.Texture,
//				Context.Rect,
//				*Context.OutData,
//				Context.Flags
//			);
//		});
//
//	FlushRenderingCommands();
//}



// Called every frame
void AFFMPEGPlayer::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	
}


