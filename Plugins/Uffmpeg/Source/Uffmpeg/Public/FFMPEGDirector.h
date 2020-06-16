// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AudioMixerDevice.h"
#include "RHIResources.h"
#include "RHICommandList.h"
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
#include "libswresample/swresample.h"
}
#include "FFmpegDirector.generated.h"

/**
 * 
 */
class FEncoderThread;

UCLASS(BlueprintType)
class UFFMPEG_API UFFmpegDirector:public UObject,public ISubmixBufferListener
{
	GENERATED_BODY()
public:

	UFFmpegDirector();
	virtual ~UFFmpegDirector();
	UFUNCTION(BlueprintCallable)
	void Initialize_Director(UWorld* World, FString OutFileName, bool UseGPU, int VideoFps, bool RTMP, int VideoBitRate, float AudioDelay,float SoundVolume);
	void Begin_Receive_AudioBufferData(UWorld* world);

	void Encode_Video_Frame(uint8_t *rgb);
	void Encode_SetCurrentAudioTime(uint8_t* rgb);
	void Encode_Audio_Frame(uint8_t *rgb);
	void Encode_Finish();
	
	virtual void OnNewSubmixBuffer(const USoundSubmix* OwningSubmix, float* AudioData, int32 NumSamples, int32 NumChannels, const int32 SampleRate, double AudioClock) override;
	void OnBackBufferReady_RenderThread(SWindow& SlateWindow, const FTexture2DRHIRef& BackBuffer);
	bool AddTickTime(float time);

	UFUNCTION(BlueprintCallable)
	void EndPIE(const bool l);

	void WorldChange(UWorld* change_world);
	void WorldDestory(UWorld* change_world);

	void DestoryDirector();
	
public:
	AVFrame* video_frame;
	AVFrame* audio_frame;	
private:
	void Create_Video_Encoder(bool is_use_NGPU, int fps, const char* out_file_name,int bit_rate);
	void Create_Audio_Encoder(const char* audioencoder_name);
	void Video_Frame_YUV_From_BGR(uint8_t *rgb);
	void Create_Audio_Swr();
	void Set_Audio_Volume(AVFrame *frame);
	void GetScreenVideoData();
private:

	double CurrentAuidoTime = 0.0;
	float audio_delay;
	float audio_volume;
	FEncoderThread * Runnable;
	FRunnableThread* RunnableThread;

	AVFormatContext* out_format_context;

	AVCodecContext* video_encoder_codec_context;
	AVCodecContext* audio_encoder_codec_context;

	AVPacket video_pkt;
	AVPacket audio_pkt;
	SwsContext* sws_context;

	AVStream* out_video_stream;
	AVStream* out_audio_stream;

	int width;
	int height;

	SwrContext	*swr;
	uint8_t *outs[2];

	FTexture2DRHIRef GameTexture;
	SWindow* gameWindow;
	TArray<FColor> TexturePixel;
	float ticktime = 0.0f;

	int64_t pts = 0;
	uint8* buff_rgb;

	int32 video_index;
	int32 audio_index;

	bool IsDestory = false;

	FTickerDelegate TickDelegate;
	FDelegateHandle TickDelegateHandle;
	FDelegateHandle EndPIEDelegateHandle;

	FAudioDevice* AudioDevice;
	int FPS;

	uint32 Video_Frame_Duration;
	float Video_Tick_Time;

};
