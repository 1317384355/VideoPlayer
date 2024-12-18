#pragma once

#include "playerCommand.h"
#include <QAudioOutput>
#include <QIODevice>

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
}

#include <QDebug>
#include <QThread>

// Q_DECLARE_METATYPE(SwrContext)

struct packets
{
    enum PACKET_TYPE
    {
        AUDIO = 0,
        VIDEO = 1,
    };
    PACKET_TYPE type;
    int64_t pts;
    uint8_t *data;
    int size;
};

class Decode : public QObject
{
    Q_OBJECT
signals:
    void startPlay();
    void playOver();
    void initAudioThread(AVCodecContext *audioCodecContext, void *swrContext, double time_base_q2d);
    void initAudioOutput(int sampleRate, int channels);
    void initVideoThread(AVCodecContext *videoCodecContext, int hw_device_type, double time_base_q2d);
    void initVideoOutput(int format);

    void sendAudioData(uint8_t *audioBuffer, int bufferSize, double pts);

    void sendAudioPacket(AVPacket *packet);
    void sendVideoPacket(AVPacket *packet);

    void decodeAudioPacket();
    void decodeVideoPacket();

public slots:
    // 响应拖动进度条, 跳转到帧并返回这一帧画面
    void setCurFrame(int64_t _curFrame);

    void onAudioDataUsed();
    void onVideoDataUsed();

    void onVideoQueueStatus(int status);

    // 开始播放
    void decodePacket();

public:
    enum FFMPEG_INIT_ERROR
    {
        NO_ERROR = 0,
        OPEN_STREAM_ERROR,
        FIND_INFO_ERROR,
        FIND_STREAM_ERROR,
        INIT_AUDIO_CODEC_CONTEXT_ERROR,
        INIT_VIDEO_CODEC_CONTEXT_ERROR,
        INIT_RESAMPLER_CONTEXT_ERROR,
        INIT_SW_RENDERER_CONTEXT,
    };

    enum FFMPEG_MEDIA_TYPE
    {
        UNKNOWN,
        ONLY_AUDIO,
        ONLY_VIDEO,
        MULTI_AUDIO_VIDEO,
    };

private:
    QList<AVHWDeviceType> devices; // 设备支持的硬解码器, 在类初始化时遍历获取

    AVFormatContext *formatContext; // 用于处理媒体文件格式的结构, 包含了许多用于描述文件格式和元数据的信息

    AVFormatContext *audioFormatContext;
    AVFormatContext *videoFormatContext;
    AVCodecContext *videoCodecContext;
    AVCodecContext *audioCodecContext;

    enum AVPixelFormat hw_device_pixel = AV_PIX_FMT_NONE;
    enum AVHWDeviceType hw_device_type = AV_HWDEVICE_TYPE_NONE;
    AVBufferRef *hw_device_ctx = nullptr;

    SwrContext *swrContext{nullptr};

    // AVPacket packet;
    int audioStreamIndex;
    int videoStreamIndex;
    FFMPEG_MEDIA_TYPE mediaType;

    double time_base_q2d;
    int64_t curPts;

    const int *m_type; // 控制播放状态

    bool isAudioPacketEmpty = true;
    bool isVideoPacketEmpty = true;
    int videoQueueStatus = 0;

    bool isIniting = false;
    bool isInitSuccess = false;

    // 音频相关结构体初始化
    int initFFmpeg(const QString &filePath);
    // AVCodecContext *getCudaDecoder
    int initCodec(AVCodecContext **codecContext, int videoStreamIndex, const AVCodec *codec);

    void clean();

    void debugError(FFMPEG_INIT_ERROR error);

    static AVPixelFormat getHwFormat(AVCodecContext *ctx, const enum AVPixelFormat *pix_fmts);

    void decodeMultMedia();
    void decodeAudio();
    void decodeVideo();

public:
    explicit Decode(const int *_type, QObject *parent = nullptr);
    ~Decode();

    void setVideoPath(const QString &filePath);

    bool resume();

    // 得到总音频帧数
    int64_t getAudioFrameCount() const;
    // 得到总视频帧数
    int64_t getVideoFrameCount() const;
    // 得到总时长
    int64_t getDuration() const;

    // 获取支持的硬解码器
    QList<QString> getSupportedHwDecoderNames();
};
