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
	AudioDevice(nullptr),
	gameWindow(nullptr),
	buff_bgr(nullptr),
	Runnable(nullptr),
	RunnableThread(nullptr),
	out_format_context(nullptr),
	video_encoder_codec_context(nullptr),
	audio_encoder_codec_context(nullptr),
	sws_context(nullptr),
	out_video_stream(nullptr),
	out_audio_stream(nullptr),
	swr(nullptr),
	audio_buffersrc(nullptr),
	audio_buffersink(nullptr),
	audio_outputs(nullptr),
	audio_inputs(nullptr),
	audio_filter_graph(nullptr),
	audio_buffersink_ctx(nullptr),
	audio_buffersrc_ctx(nullptr)
{
	outs[0] = nullptr;
	outs[1] = nullptr;
	video_fps = 0;
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
		FMemory::Free(buff_bgr);
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

void UFFmpegDirector::Begin_Receive_AudioData(UWorld* world)
{
	AudioDevice = world->GetAudioDevice();
	if (AudioDevice)
	{
		AudioDevice->RegisterSubmixBufferListener(this);
	}
}

void UFFmpegDirector::Initialize_Director(UWorld* World, FString OutFileName, bool UseGPU, int VideoFps, bool RTMP, int VideoBitRate, float AudioDelay, float SoundVolume)
{
	avfilter_register_all();
	av_register_all();
	avformat_network_init();

	audio_delay = AudioDelay;
	video_fps = VideoFps;
	Video_Tick_Time = float(1) / float(video_fps);
	audio_volume = SoundVolume;
	gameWindow = GEngine->GameViewport->GetWindow().Get();
	width = gameWindow->GetViewportSize().X;
	height = gameWindow->GetViewportSize().Y;
	buff_bgr = (uint8_t *)FMemory::Realloc(buff_bgr, 3 * width *height);
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
	//create audio encoder
	Create_Audio_Swr();
	Create_Audio_Encoder("aac");

	//create video encoder
	Create_Video_Encoder(UseGPU, TCHAR_TO_ANSI(*OutFileName), VideoBitRate);

	//initialize and set vlalue of audio filter
	AudioFilterAlloc();
	SetAudioFilterValue();

	//End PIE deleate and tick delegate
	AddPIEEndFunction();
	AddTickFunction();

	//create encode thread
	CreateEncodeThread();

	//bind delegate for get video data and audio data 
	Begin_Receive_VideoData();
	Begin_Receive_AudioData(World);
}

void UFFmpegDirector::Begin_Receive_VideoData()
{
	FSlateApplication::Get().GetRenderer()->OnBackBufferReadyToPresent().AddUObject(this, &UFFmpegDirector::OnBackBufferReady_RenderThread);
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

void UFFmpegDirector::AddPIEEndFunction()
{
	EndPIEDelegateHandle = FEditorDelegates::PrePIEEnded.AddUObject(this, &UFFmpegDirector::EndPIE);
}

void UFFmpegDirector::AddTickFunction()
{
	TickDelegateHandle = FTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UFFmpegDirector::AddTickTime));
}

void UFFmpegDirector::GetScreenVideoData()
{
	uint32 LolStride;
	FRHICommandListImmediate& list = GRHICommandList.GetImmediateCommandList();

	uint8* TextureData = (uint8*)list.LockTexture2D(GameTexture->GetTexture2D(), 0, EResourceLockMode::RLM_ReadOnly, LolStride, false);
	Runnable->InsertVideo(TextureData);
	list.UnlockTexture2D(GameTexture, 0, false);
}

void UFFmpegDirector::CreateEncodeThread()
{
	Runnable = new FEncoderThread();
	Runnable->CreateQueue(4 * width*height, 2048 * sizeof(float), 30, 40);
	Runnable->GetAudioProcessDelegate().BindUObject(this, &UFFmpegDirector::Encode_Audio_Frame);
	Runnable->video_encode_delegate.BindUObject(this, &UFFmpegDirector::Encode_Video_Frame);
	Runnable->GetAudioTimeProcessDelegate().BindUObject(this, &UFFmpegDirector::Encode_SetCurrentAudioTime);
	RunnableThread = FRunnableThread::Create(Runnable, TEXT("EncoderThread"));
}

void UFFmpegDirector::AudioFilterAlloc()
{
	audio_buffersrc = avfilter_get_by_name("abuffer");
	audio_buffersink = avfilter_get_by_name("abuffersink");

	audio_outputs = avfilter_inout_alloc();
	audio_inputs = avfilter_inout_alloc();
}

void UFFmpegDirector::SetAudioFilterValue()
{
	if (audio_index == -1)
		return;
	int ret = 0;
	audio_filter_graph = avfilter_graph_alloc();

	if (!audio_inputs || !audio_outputs || !audio_filter_graph)
	{
		ret = AVERROR(ENOMEM);
		return;
	}
	const enum AVSampleFormat out_sample_fmts[] = { audio_encoder_codec_context->sample_fmt,
			AV_SAMPLE_FMT_NONE };
	const int64_t out_channel_layouts[] = { audio_encoder_codec_context->channel_layout, -1 };
	const int out_sample_rates[] = { audio_encoder_codec_context->sample_rate, -1 };

	if (!audio_encoder_codec_context->channel_layout)
		audio_encoder_codec_context->channel_layout = av_get_default_channel_layout(
			audio_encoder_codec_context->channels);

	char args[100];
	snprintf(args, sizeof(args),
		"time_base=%d/%d:sample_rate=%d:sample_fmt=%s:channel_layout=%lu",
		audio_encoder_codec_context->time_base.num, audio_encoder_codec_context->time_base.den,
		audio_encoder_codec_context->sample_rate, av_get_sample_fmt_name(audio_encoder_codec_context->sample_fmt),
		(unsigned long)audio_encoder_codec_context->channel_layout);

	ret = avfilter_graph_create_filter(&audio_buffersrc_ctx, audio_buffersrc, "in",
		args, NULL, audio_filter_graph);
	if (ret < 0)
	{
		check(false);
	}

	ret = avfilter_graph_create_filter(&audio_buffersink_ctx, audio_buffersink, "out",
		NULL, NULL, audio_filter_graph);
	if (ret < 0) 
	{
		check(false);
	}

	ret = av_opt_set_int_list(audio_buffersink_ctx, "sample_fmts", out_sample_fmts,
		-1, AV_OPT_SEARCH_CHILDREN);
	if (ret < 0)
	{
		check(false);
	}

	ret = av_opt_set_int_list(audio_buffersink_ctx, "channel_layouts",
		out_channel_layouts, -1, AV_OPT_SEARCH_CHILDREN);
	if (ret < 0)
	{
		check(false);
	}

	ret = av_opt_set_int_list(audio_buffersink_ctx, "sample_rates", out_sample_rates,
		-1, AV_OPT_SEARCH_CHILDREN);
	if (ret < 0) 
	{
		check(false);
	}

	audio_outputs->name = av_strdup("in");
	audio_outputs->filter_ctx = audio_buffersrc_ctx;
	audio_outputs->pad_idx = 0;
	audio_outputs->next = NULL;

	audio_inputs->name = av_strdup("out");
	audio_inputs->filter_ctx = audio_buffersink_ctx;
	audio_inputs->pad_idx = 0;
	audio_inputs->next = NULL;

	audio_filter_descr = FString::FString("volume=");
	audio_filter_descr.Append(FString::SanitizeFloat(audio_volume));
	if ((ret = avfilter_graph_parse_ptr(audio_filter_graph,TCHAR_TO_ANSI(*audio_filter_descr),
		&audio_inputs, &audio_outputs, NULL)) < 0)
	{
		check(false);
	}
	if ((ret = avfilter_graph_config(audio_filter_graph, NULL)) < 0)
	{
		check(false);
	}
	av_buffersink_set_frame_size(audio_buffersink_ctx, 1024);
}

void UFFmpegDirector::Create_Audio_Encoder(const char* audioencoder_name)
{
	AVCodec* audioencoder_codec;
	audioencoder_codec = avcodec_find_encoder_by_name(audioencoder_name);
	out_audio_stream = avformat_new_stream(out_format_context, audioencoder_codec);
	audio_index = out_audio_stream->index;
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
}

void UFFmpegDirector::Create_Video_Encoder(bool is_use_NGPU, const char* out_file_name, int bit_rate)
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

	video_index = out_video_stream->index;

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
	video_encoder_codec_context->time_base.den = video_fps;
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
	video_encoder_codec_context->framerate = { video_fps,1 };

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

	Video_Frame_Duration = out_video_stream->time_base.den / video_fps;

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

	uint8_t* Firset_ptr = buff_bgr;
	for (Row = 0; Row < GameTexture->GetSizeY(); ++Row)
	{
		uint32* PixelPtr = (uint32*)TextureDataPtr;
		for (Col = 0; Col < GameTexture->GetSizeX(); ++Col)
		{
			uint32 EncodedPixel = *PixelPtr;
			//	AV_PIX_FMT_BGR24	这里暂时转换为BGR
			//	AV_PIX_FMT_RGB24	掉帧严重 暂时不知道为什么
			*(buff_bgr + 2) = (EncodedPixel >> 2) & 0xFF;
			*(buff_bgr + 1) = (EncodedPixel >> 12) & 0xFF;
			*(buff_bgr) = (EncodedPixel >> 22) & 0xFF;
			buff_bgr += 3;
			++PixelPtr;
		}
		TextureDataPtr += Add_Size;
	}
	buff_bgr = Firset_ptr;

	int got_output = 0;
	Video_Frame_YUV_From_BGR(buff_bgr);
	av_init_packet(&video_pkt);
	video_frame->pts = video_pts;
	
	video_pkt.data = NULL;
	video_pkt.size = 0;
	if (avcodec_encode_video2(video_encoder_codec_context, &video_pkt, video_frame, &got_output) < 0)
	{
		check(false);
	}
	if (got_output)
	{
		++video_pts;
		video_pkt.pts = video_pkt.dts = video_frame->pts*Video_Frame_Duration;
		video_pkt.stream_index = video_index;
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
	AVFrame* filt_frame = av_frame_alloc();
	AVFrame* audio_frame= av_frame_alloc();
	AVPacket* audio_pkt = av_packet_alloc();
	int count = swr_convert(swr, outs, 4096, &data, 1024);
	int got_output = 0;
	
	audio_frame->data[0] = (uint8_t*)outs[0];
	audio_frame->data[1] = (uint8_t*)outs[1];
	audio_frame->nb_samples = audio_encoder_codec_context->frame_size;
	audio_frame->format = audio_encoder_codec_context->sample_fmt;
	audio_frame->sample_rate = out_audio_stream->time_base.den;
	audio_frame->pkt_dts = audio_frame->pkt_pts = audio_frame->pts = 1;
	audio_frame->channel_layout = audio_encoder_codec_context->channel_layout;
	audio_frame->channels = audio_encoder_codec_context->channels;
	audio_frame->pkt_size = 1024;
	if (av_buffersrc_add_frame_flags(audio_buffersrc_ctx, audio_frame, AV_BUFFERSRC_FLAG_KEEP_REF) < 0)
	{
		check(false);
	}
	while (1)
	{
		int ret = av_buffersink_get_frame(audio_buffersink_ctx, filt_frame);
		if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
			break;
		if (ret < 0)
			break;
		if (ret >= 0)
		{
			ret = avcodec_send_frame(audio_encoder_codec_context, filt_frame);
			if (ret < 0)
			{
			}
			while (ret >= 0)
			{
				ret = avcodec_receive_packet(audio_encoder_codec_context, audio_pkt);
				if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
				{
					av_packet_free(&audio_pkt);
					break;
				}
				else if (ret < 0)
				{
					av_packet_free(&audio_pkt);
					break;
				}
				audio_pkt->stream_index = audio_index;
				audio_pkt->pts = audio_pkt->dts = av_rescale_q(
					(CurrentAuidoTime + audio_delay) / av_q2d({ 1,48000 }),
					{ 1,48000 },
					out_audio_stream->time_base);

				audio_pkt->duration = av_rescale_q(
					audio_pkt->duration,
					{ 1,48000 },
					out_audio_stream->time_base);
				ret = av_interleaved_write_frame(out_format_context, audio_pkt);
			}
			av_packet_free(&audio_pkt);
		}
	}

	av_frame_unref(filt_frame);
	av_frame_unref(audio_frame);


	/*if (avcodec_encode_audio2(audio_encoder_codec_context, &audio_pkt, audio_frame, &got_output) < 0)
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
	
		audio_pkt.stream_index = audio_index;
		av_write_frame(out_format_context, &audio_pkt);
		av_packet_unref(&audio_pkt);
	}*/
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

	avfilter_inout_free(&audio_outputs);
	avfilter_inout_free(&audio_inputs);

	av_packet_unref(&video_pkt);
	av_frame_free(&video_frame);
}