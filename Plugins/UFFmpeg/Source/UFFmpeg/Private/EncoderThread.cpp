// Fill out your copyright notice in the Description page of Project Settings.


#include "EncoderThread.h"


FEncoderThread::FEncoderThread()
{
}

FEncoderThread::~FEncoderThread()
{
	if (audio_queue)
	{
		audio_queue->RemoveFromRoot();
		audio_queue->ConditionalBeginDestroy();
		audio_queue->BeginDestroy();
	}
	if (videobuffer_queue)
	{
		videobuffer_queue->RemoveFromRoot();
		videobuffer_queue->ConditionalBeginDestroy();
		videobuffer_queue->BeginDestroy();
	}
	if (audio_time_queue)
	{
		audio_time_queue->RemoveFromRoot();
		audio_time_queue->ConditionalBeginDestroy();
		audio_time_queue->BeginDestroy();
	}
}

bool FEncoderThread::Init()
{
	return true;
}

uint32 FEncoderThread::Run()
{
	while (!IsDone)
	{
		RunEncode();
	}
	return 1;
}

void FEncoderThread::Stop()
{
	IsDone = true;
}

void FEncoderThread::Exit()
{
	
}

void FEncoderThread::CreateQueue(int video_data_size, int auido_data_size, int video_data_num, int auido_data_num)
{
	videobuffer_queue = NewObject<UCircleQueue>();
	videobuffer_queue->AddToRoot();
	videobuffer_queue->Init(video_data_num, video_data_size);
	videobuffer_queue->encode_delegate.BindRaw(this, &FEncoderThread::GetBufferData);

	audio_queue = NewObject<UCircleQueue>();
	audio_queue->AddToRoot();
	audio_queue->Init(auido_data_num, auido_data_size);


	audio_time_queue = NewObject<UCircleQueue>();
	audio_time_queue->AddToRoot();
	audio_time_queue->Init(auido_data_num, auido_data_num*sizeof(double));
}

EncodeDelegate& FEncoderThread::GetAudioProcessDelegate()
{
	return audio_queue->encode_delegate; 
}

EncodeDelegate& FEncoderThread::GetAudioTimeProcessDelegate()
{
	return audio_time_queue->encode_delegate;
}

bool FEncoderThread::InsertVideo(uint8* Src)
{
	if (!videobuffer_queue)
		return false;
	{
		FScopeLock ScopeLock(&VideoBufferMutex);		
		while (!videobuffer_queue->InsertEncodeData(Src))
		{
			//GEngine->AddOnScreenDebugMessage(-1, 0.02f, FColor::Red, FString("video  now  encode"));
			videobuffer_queue->PrcessEncodeData();
			EncodeVideo();
		}
	}		
	return true;
}

bool FEncoderThread::InsertAudio(uint8* Src, uint8* time)
{
	if (!audio_queue)
		return false;
	{
		FScopeLock ScopeLock(&AudioMutex);

		while (!audio_queue->InsertEncodeData(Src) || !audio_time_queue->InsertEncodeData(time))
		{
			//GEngine->AddOnScreenDebugMessage(-1, 1.0f, FColor::Red, FString("audio  now  encode"));
			EncodeAudio();
		}
	}
	return true;
}

void FEncoderThread::GetBufferData(uint8* data)
{
	video_data = data;	
}


bool FEncoderThread::IsQueneEmpty()
{
	return videobuffer_queue->IsEmpty() && audio_queue->IsEmpty() && audio_time_queue->IsEmpty();
}

void FEncoderThread::RunEncode()
{
	bool IsNeedEncode = false;

	{
		FScopeLock ScopeLock(&AudioMutex);
		EncodeAudio();
	}

	{
		FScopeLock ScopeLock1(&VideoBufferMutex);
		IsNeedEncode = videobuffer_queue->PrcessEncodeData();

		if (IsNeedEncode)
			EncodeVideo();
	}
}

void FEncoderThread::EncodeVideo()
{
	if (video_data)
	{
		video_encode_delegate.ExecuteIfBound(video_data);
		video_data = nullptr;
	}
}

void FEncoderThread::EncodeAudio()
{
	if (audio_queue)
	{
		audio_time_queue->PrcessEncodeData();
		audio_queue->PrcessEncodeData();
	}
}
