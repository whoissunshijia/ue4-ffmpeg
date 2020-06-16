// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "Runnable.h"

#include "RHICommandList.h"
#include "RHIResources.h"

#include "FFMPEGPlayer.generated.h"



class UFFmpegDirector;
class SWindow;

bool bIsProcess=false;




UCLASS()
class UE4_FFMPEG_API AFFMPEGPlayer : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AFFMPEGPlayer();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	virtual void BeginDestroy() override;
	
	void GetTexturePixels(FTexture2DRHIRef Texture, TArray<FColor>& OutPixels);

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;
	
private:

	UFFmpegDirector* f_director;

	AFFMPEGPlayer* ins;


};



