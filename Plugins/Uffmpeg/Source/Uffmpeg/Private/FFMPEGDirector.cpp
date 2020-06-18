// Fill out your copyright notice in the Description page of Project Settings.


#include "FFMPEGDirector.h"
#include "Engine/GameViewportClient.h"
#include "SceneViewport.h"
#include "SlateApplication.h"
#include "EncoderThread.h"
#include "EncodeData.h"
#include "LogVerbosity.h"
#include "Ticker.h"
#include "Editor.h"

#include "Engine/World.h"

UFFmpegDirector::UFFmpegDirector() :
	Runnable(nullptr),
	out_format_context(nullptr),
	video_encoder_codec_context(nullptr),
	audio_encoder_codec_context(nullptr),
	sws_context(nullptr),
	out_video_stream(nullptr),
	swr(nullptr),
	gameWindow(nullptr),
	buff_rgb(nullptr),
	AudioDevice(nullptr)
{
	outs[0] = nullptr;
	outs[1] = nullptr;
	FPS = 0;
}

UFFmpegDirector::~UFFmpegDirector()
{
}

void UFFmpegDirector::DestoryDirector()
{
	if (!IsDestory)
	{
		if (AudioDevice)
		{
			AudioDevice->UnregisterSubmixBufferListener(this);
		}
		FSlateApplication::Get().GetRenderer()->OnBackBufferReadyToPresent().RemoveAll(this);
		FEditorDelegates::PrePIEEnded.Remove(EndPIEDelegateHandle);

		FTicker::GetCoreTicker().RemoveTicker(TickDelegateHandle);

		RunnableThread->Kill(true);

		delete Runnable;

		Encode_Finish();
		FMemory::Free(outs[0]);
		FMemory::Free(outs[1]);
		FMemory::Free(buff_rgb);
		IsDestory = true;
	}
}


void UFFmpegDirector::EndPIE(const bool l)
{
	DestoryDirector();
	this->RemoveFromRoot();
	this->ConditionalBeginDestroy();
	this->BeginDestroy();
}

void UFFmpegDirector::Begin_Receive_AudioBufferData(UWorld* world)
{
	AudioDevice = world->GetAudioDevice();
	if (AudioDevice)
	{
		AudioDevice->RegisterSubmixBufferListener(this);
	}
}

void UFFmpegDirector::Initialize_Director(UWorld* World, FString OutFileName, bool UseGPU, int VideoFps, bool RTMP, int VideoBitRate, float AudioDelay, float SoundVolume)
{
	av_register_all();
	avformat_network_init();

	audio_delay = AudioDelay;
	FPS = VideoFps;	
	Video_Tick_Time = float(1) / float(FPS);
	audio_volume = SoundVolume;
	TickDelegate = FTickerDelegate::CreateUObject(this, &UFFmpegDirector::AddTickTime);
	TickDelegateHandle = FTicker::GetCoreTicker().AddTicker(TickDelegate);
	EndPIEDelegateHandle = FEditorDelegates::PrePIEEnded.AddUObject(this, &UFFmpegDirector::EndPIE);

	gameWindow = GEngine->GameViewport->GetWindow().Get();
	buff_rgb = (uint8_t *)FMemory::Realloc(buff_rgb, 3 * sizeof(uint8_t) * gameWindow->GetViewportSize().X * gameWindow->GetViewportSize().Y);

	width = gameWindow->GetViewportSize().X;
	height = gameWindow->GetViewportSize().Y;
	outs[0] = (uint8_t *)FMemory::Realloc(outs[0], 4096);
	outs[1] = (uint8_t *)FMemory::Realloc(outs[1], 4096);

	if (RTMP)
	{
		if (avformat_alloc_output_context2(&out_format_context, NULL, "flv", TCHAR_TO_ANSI(*OutFileName)) < 0)
		{
			check(false);
		}
	}
	else
	{
		if (avformat_alloc_output_context2(&out_format_context, NULL, NULL, TCHAR_TO_ANSI(*OutFileName)) < 0)
		{
			check(false);
		}
	}
	Create_Audio_Swr();
	Create_Audio_Encoder("aac");
	Create_Video_Encoder(UseGPU, FPS, TCHAR_TO_ANSI(*OutFileName), VideoBitRate);
	Runnable = new FEncoderThread();

	Runnable->CreateQueue(4 * width*height, 2048 * sizeof(float), 30, 40);

	Runnable->GetAudioProcessDelegate().BindUObject(this, &UFFmpegDirector::Encode_Audio_Frame);
	Runnable->video_encode_delegate.BindUObject(this, &UFFmpegDirector::Encode_Video_Frame);
	Runnable->GetAudioTimeProcessDelegate().BindUObject(this, &UFFmpegDirector::Encode_SetCurrentAudioTime);
	RunnableThread = FRunnableThread::Create(Runnable, TEXT("EncoderThread"));

	FSlateApplication::Get().GetRenderer()->OnBackBufferReadyToPresent().AddUObject(this, &UFFmpegDirector::OnBackBufferReady_RenderThread);
	Begin_Receive_AudioBufferData(World);
}
void UFFmpegDirector::OnBackBufferReady_RenderThread(SWindow& SlateWindow, const FTexture2DRHIRef& BackBuffer)
{
	if (gameWindow == &SlateWindow)
	{
		if (ticktime >= Video_Tick_Time)
		{
			GameTexture = BackBuffer;
			ticktime -= Video_Tick_Time;
			GetScreenVideoData();
		}
	}
}

bool UFFmpegDirector::AddTickTime(float time)
{
	ticktime += time;
	return true;
}

void UFFmpegDirector::GetScreenVideoData()
{
	uint32 LolStride;
	FRHICommandListImmediate& ll = GRHICommandList.GetImmediateCommandList();

	uint8* TextureData = (uint8*)ll.LockTexture2D(GameTexture->GetTexture2D(), 0, EResourceLockMode::RLM_ReadOnly, LolStride, false);
	Runnable->InsertVideo(TextureData);
	ll.UnlockTexture2D(GameTexture, 0, false);
	
}

void UFFmpegDirector::Create_Audio_Encoder(const char* audioencoder_name)
{
	AVCodec* audioencoder_codec;
	audioencoder_codec = avcodec_find_encoder_by_name(audioencoder_name);
	out_audio_stream = avformat_new_stream(out_format_context, audioencoder_codec);

	audio_encoder_codec_context = avcodec_alloc_context3(audioencoder_codec);

	if (!out_audio_stream)
	{
		check(false);
	}
	audio_encoder_codec_context->codec_id = AV_CODEC_ID_AAC;
	audio_encoder_codec_context->bit_rate = 120000;
	audio_encoder_codec_context->codec_type = AVMEDIA_TYPE_AUDIO;
	audio_encoder_codec_context->sample_rate = 48000;
	audio_encoder_codec_context->sample_fmt = AV_SAMPLE_FMT_FLTP;
	audio_encoder_codec_context->channels = 2;
	audio_encoder_codec_context->channel_layout = AV_CH_LAYOUT_STEREO;

	if (out_format_context->oformat->flags & AVFMT_GLOBALHEADER)
		audio_encoder_codec_context->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

	audio_encoder_codec_context->codec_tag = 0;
	out_audio_stream->codecpar->codec_tag = 0;

	if (avcodec_open2(audio_encoder_codec_context, audioencoder_codec, NULL) < 0)
	{
		check(false);
	}
	avcodec_parameters_from_context(out_audio_stream->codecpar, audio_encoder_codec_context);

	audio_frame = av_frame_alloc();
	audio_frame->nb_samples = audio_encoder_codec_context->frame_size;
	audio_frame->format = audio_encoder_codec_context->sample_fmt;

}

void UFFmpegDirector::Create_Video_Encoder(bool is_use_NGPU, int fps, const char* out_file_name, int bit_rate)
{
	AVCodec *encoder_codec;
	int ret;

	if (is_use_NGPU)
	{
		encoder_codec = avcodec_find_encoder_by_name("nvenc_h264");
	}
	else
	{
		encoder_codec = avcodec_find_encoder(AV_CODEC_ID_H264);
	}
	if (!encoder_codec)
	{
		check(false);
	}
	out_video_stream = avformat_new_stream(out_format_context, encoder_codec);
	video_encoder_codec_context = avcodec_alloc_context3(encoder_codec);
	if (!video_encoder_codec_context)
	{
		check(false);
	}
	video_encoder_codec_context->bit_rate = bit_rate;
	video_encoder_codec_context->rc_min_rate = bit_rate;
	video_encoder_codec_context->rc_max_rate = bit_rate;
	video_encoder_codec_context->bit_rate_tolerance = bit_rate;
	video_encoder_codec_context->rc_buffer_size = bit_rate;
	video_encoder_codec_context->rc_initial_buffer_occupancy = bit_rate * 3 / 4;
	video_encoder_codec_context->width = width;
	video_encoder_codec_context->height = height;
	video_encoder_codec_context->max_b_frames = 2;
	video_encoder_codec_context->time_base.num = 1;
	video_encoder_codec_context->time_base.den = fps;
	video_encoder_codec_context->pix_fmt = AV_PIX_FMT_YUV420P;
	video_encoder_codec_context->me_range = 16;
	video_encoder_codec_context->codec_type = AVMEDIA_TYPE_VIDEO;
	video_encoder_codec_context->profile = FF_PROFILE_H264_BASELINE;
	video_encoder_codec_context->frame_number = 1;
	video_encoder_codec_context->qcompress = 0.6;
	video_encoder_codec_context->max_qdiff = 4;
	video_encoder_codec_context->level = 30;
	video_encoder_codec_context->gop_size = 25;
	video_encoder_codec_context->qmin = 31;
	video_encoder_codec_context->qmax = 50;
	video_encoder_codec_context->me_range = 16;
	video_encoder_codec_context->framerate = { fps,1 };

	//ultrafast,superfast, veryfast, faster, fast, medium, slow, slower, veryslow,placebo.
	if (encoder_codec)
		av_opt_set(video_encoder_codec_context->priv_data, "preset", "faster",
			0);



	if (out_format_context->oformat->flags & AVFMT_GLOBALHEADER)
		video_encoder_codec_context->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

	int i = avcodec_open2(video_encoder_codec_context, encoder_codec, NULL);
	if (i < 0)
	{
		check(false);
	}

	ret = avcodec_parameters_from_context(out_video_stream->codecpar, video_encoder_codec_context);
	if (ret < 0)
	{
		check(false);
	}

	video_frame = av_frame_alloc();
	if (!video_frame)
	{
		check(false);
	}

	avio_open(&out_format_context->pb, out_file_name, AVIO_FLAG_WRITE);
	ret = av_image_alloc(
		video_frame->data,
		video_frame->linesize,
		video_encoder_codec_context->width,
		video_encoder_codec_context->height,
		video_encoder_codec_context->pix_fmt,
		32);
	if (ret < 0)
	{
		check(false);
	}

	sws_context = sws_getCachedContext(sws_context,
		video_encoder_codec_context->width, video_encoder_codec_context->height, AV_PIX_FMT_BGR24,
		video_encoder_codec_context->width, video_encoder_codec_context->height, AV_PIX_FMT_YUV420P,
		0, 0, 0, 0);

	if (avformat_write_header(out_format_context, NULL) < 0)
	{
		check(false);
	}

	Video_Frame_Duration = out_video_stream->time_base.den / FPS;

}

void UFFmpegDirector::Video_Frame_YUV_From_BGR(uint8_t *rgb)
{
	static const int in_linesize[1] = { 3 * width };

	sws_scale(sws_context, (const uint8_t * const *)&rgb, in_linesize, 0,
		height, video_frame->data, video_frame->linesize);
	video_frame->width = width;
	video_frame->height = height;
	video_frame->format = AV_PIX_FMT_YUV420P;
}

void UFFmpegDirector::Create_Audio_Swr()
{
	swr = swr_alloc();
	av_opt_set_int(swr, "in_channel_layout", AV_CH_LAYOUT_STEREO, 0);
	av_opt_set_int(swr, "out_channel_layout", AV_CH_LAYOUT_STEREO, 0);
	av_opt_set_int(swr, "in_sample_rate", 48000, 0);
	av_opt_set_int(swr, "out_sample_rate", 48000, 0);
	av_opt_set_sample_fmt(swr, "in_sample_fmt", AV_SAMPLE_FMT_FLT, 0);
	av_opt_set_sample_fmt(swr, "out_sample_fmt", AV_SAMPLE_FMT_FLTP, 0);
	swr_init(swr);
}

void UFFmpegDirector::Set_Audio_Volume(AVFrame *frame)
{
	int i = 0;
	float *ch_left = (float *)frame->data[0];
	float *ch_right = (float *)frame->data[1];
	for (i = 0; i < frame->nb_samples; i++)
	{
		ch_left[i] *= audio_volume;
		ch_right[i] *= audio_volume;
	}
}

void UFFmpegDirector::OnNewSubmixBuffer(const USoundSubmix* OwningSubmix, float* AudioData, int32 NumSamples, int32 NumChannels, const int32 SampleRate, double AudioClock)
{
	Runnable->InsertAudio((uint8_t*)AudioData, (uint8_t*)&AudioClock);
}

void UFFmpegDirector::Encode_Video_Frame(uint8_t *rgb)
{
	uint32 Add_Size= width * 4;
	uint32 Row = 0;
	uint32 Col = 0;
	uint8* TextureDataPtr=rgb;

	uint8_t* Firset_ptr = buff_rgb;
	for (Row = 0; Row < GameTexture->GetSizeY(); ++Row)
	{
		uint32* PixelPtr = (uint32*)TextureDataPtr;
		for (Col = 0; Col < GameTexture->GetSizeX(); ++Col)
		{
			uint32 EncodedPixel = *PixelPtr;
			//	AV_PIX_FMT_BGR24	这里暂时转换为BGR
			//	AV_PIX_FMT_RGB24	掉帧严重 暂时不知道为什么
			*(buff_rgb + 2) = (EncodedPixel >> 2) & 0xFF;
			*(buff_rgb + 1) = (EncodedPixel >> 12) & 0xFF;
			*(buff_rgb) = (EncodedPixel >> 22) & 0xFF;
			buff_rgb += 3;
			++PixelPtr;
		}
		TextureDataPtr += Add_Size;
	}
	buff_rgb = Firset_ptr;

	int got_output = 0;
	Video_Frame_YUV_From_BGR(buff_rgb);
	av_init_packet(&video_pkt);
	video_frame->pts = pts;
	
	video_pkt.data = NULL;
	video_pkt.size = 0;
	if (avcodec_encode_video2(video_encoder_codec_context, &video_pkt, video_frame, &got_output) < 0)
	{
		check(false);
	}
	if (got_output)
	{
		++pts;
		video_pkt.pts = video_pkt.dts = video_frame->pts*Video_Frame_Duration;
		video_pkt.stream_index = out_video_stream->index;
		video_pkt.duration = Video_Frame_Duration;
		av_write_frame(out_format_context, &video_pkt);
		av_packet_unref(&video_pkt);
	}
}

void UFFmpegDirector::Encode_SetCurrentAudioTime(uint8_t* rgb)
{
	CurrentAuidoTime = *(double*)rgb;
}

void UFFmpegDirector::Encode_Audio_Frame(uint8_t *rgb)
{
	const uint8_t* data = rgb;
	int count = swr_convert(swr, outs, 4096, &data, 1024);

	int got_output = 0;
	av_init_packet(&audio_pkt);
	audio_pkt.data = NULL;
	audio_pkt.size = 0;
	
	audio_frame->data[0] = (uint8_t*)outs[0];
	audio_frame->data[1] = (uint8_t*)outs[1];

	Set_Audio_Volume(audio_frame);

	if (avcodec_encode_audio2(audio_encoder_codec_context, &audio_pkt, audio_frame, &got_output) < 0)
	{
		check(false);
	}
	if (got_output)
	{
		audio_pkt.pts = audio_pkt.dts = av_rescale_q(
			(CurrentAuidoTime + audio_delay) / av_q2d({ 1,48000 }),
			{ 1,48000 },
			out_audio_stream->time_base);

		audio_pkt.duration = av_rescale_q(
			audio_pkt.duration,
			{ 1,48000 },
			out_audio_stream->time_base);
	
		audio_pkt.stream_index = out_audio_stream->index;
		av_write_frame(out_format_context, &audio_pkt);
		av_packet_unref(&audio_pkt);
	}
}

void UFFmpegDirector::Encode_Finish()
{
	if (out_format_context)
	{
		av_write_trailer(out_format_context);
		avio_close(out_format_context->pb);
		avformat_free_context(out_format_context);
	}

	if (video_encoder_codec_context)
	{
		avcodec_free_context(&video_encoder_codec_context);
		avcodec_close(video_encoder_codec_context);
		av_free(video_encoder_codec_context);
	}

	if (audio_encoder_codec_context)
	{
		avcodec_free_context(&audio_encoder_codec_context);
		avcodec_close(audio_encoder_codec_context);
		av_free(audio_encoder_codec_context);
	}
	if (swr)
	{
		swr_close(swr);
		swr_free(&swr);
		sws_freeContext(sws_context);
	}


	av_packet_unref(&video_pkt);
	av_packet_unref(&audio_pkt);
	av_frame_free(&video_frame);
	av_frame_free(&audio_frame);
}