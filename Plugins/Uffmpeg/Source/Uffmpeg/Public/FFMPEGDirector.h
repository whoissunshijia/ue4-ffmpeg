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
	void Initialize_Director(UWorld* World, FString OutFileName, bool UseGPU,FString VideoFilter,int VideoFps, int VideoBitRate, float AudioDelay,float SoundVolume);
	void Begin_Receive_AudioData(UWorld* world);
	void Begin_Receive_VideoData();

	void Encode_Video_Frame(uint8_t *rgb);
	void Encode_SetCurrentAudioTime(uint8_t* rgb);
	void Encode_Audio_Frame(uint8_t *rgb);
	void Encode_Finish();
	
	virtual void OnNewSubmixBuffer(const USoundSubmix* OwningSubmix, float* AudioData, int32 NumSamples, int32 NumChannels, const int32 SampleRate, double AudioClock) override;
	void OnBackBufferReady_RenderThread(SWindow& SlateWindow, const FTexture2DRHIRef& BackBuffer);
	bool AddTickTime(float time);

	void EndWindowReader(const bool i);
	void EndWindowReader_StandardGame(void* i);
	
	void DestoryDirector();
	
private:
	void Create_Video_Encoder(bool is_use_NGPU, const char* out_file_name,int bit_rate);
	void Create_Audio_Encoder(const char* audioencoder_name);
	void Video_Frame_YUV_From_BGR(uint8_t *rgb);
	void Create_Audio_Swr();
	void GetScreenVideoData();

	void AddTickFunction();
	void AddEndFunction();
	void CreateEncodeThread();
	void Set_Audio_Volume(AVFrame *frame);

	void Alloc_Video_Filter();
	uint32 FormatSize_X(uint32 x);

private:
	bool IsDestory = false;
	FString filter_descr;

	int video_fps;
	uint32 Video_Frame_Duration;
	float Video_Tick_Time;
	double CurrentAuidoTime = 0.0;
	float audio_delay;
	float audio_volume;
	uint32 width;
	uint32 height;
	uint32 out_width;
	uint32 out_height;

	FTexture2DRHIRef GameTexture;


	AVFilterInOut *outputs;
	AVFilterInOut *inputs;
	AVFilterGraph *filter_graph;
	AVFilterContext *buffersink_ctx;
	AVFilterContext *buffersrc_ctx;

	FAudioDevice* AudioDevice;
	SWindow* gameWindow;
	TArray<FColor> TexturePixel;
	float ticktime = 0.0f;
	int64_t video_pts = 0;
	uint8_t* buff_bgr;
	int32_t video_index;
	int32_t audio_index;

	FEncoderThread * Runnable;
	FRunnableThread* RunnableThread;

	AVFormatContext* out_format_context;
	AVCodecContext* video_encoder_codec_context;
	AVCodecContext* audio_encoder_codec_context;

	SwsContext* sws_context;
	AVStream* out_video_stream;
	AVStream* out_audio_stream;
	SwrContext* swr;
	uint8_t* outs[2];

	FDelegateHandle TickDelegateHandle;
	FDelegateHandle EndPIEDelegateHandle;

	AVFrame* audio_frame;
	AVFrame* video_frame;

	uint32 LolStride;
	TEnumAsByte<EWorldType::Type> GameMode;
};
