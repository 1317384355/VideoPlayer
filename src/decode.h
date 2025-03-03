#pragma once
#include <QAudioOutput>
#include <QDebug>
#include <QIODevice>
#include <QMetaType>
#include <QQueue>
#include <QScopedPointer>
#include <QSharedPointer>
#include <QThread>

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
}

class AudioDecoder;
class VideoDecoder;
class AVPacketUniquePtr;

#ifndef QMETATYPEID_DECODER
#define QMETATYPEID_DECODER
// Q_DECLARE_METATYPE(AudioDecoder *);
// Q_DECLARE_METATYPE(VideoDecoder *);
#endif

class Decoder : public QObject
{
    Q_OBJECT
signals:
    void getCurPts(double &pts);

    void startPlay();
    void playOver();

    void initAudioOutput(int sampleRate, int channels);
    void initVideoOutput(int format);

    void sendAudioPacket(AVPacket *packet);
    void sendVideoPacket(AVPacket *packet);

public slots:
    // 响应拖动进度条, 跳转到帧并返回这一帧画面
    void setCurFrame(int64_t _curFrame);

    // 开始播放
    void decodePacket();

public:
    enum FFMPEG_INIT_ERROR
    {
        NO_ERROR = 0,
        OPEN_STREAM_ERROR,
        FIND_INFO_ERROR,
        FIND_STREAM_ERROR,
        FIND_AUDIO_DECODER_ERROR,
        FIND_VIDEO_DECODER_ERROR,
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

    AudioDecoder *audioDecoder{nullptr};
    VideoDecoder *videoDecoder{nullptr};

    QQueue<AVPacket *> audioPacketQueue;
    QQueue<AVPacket *> videoPacketQueue;

    // AVPacket packet;
    FFMPEG_MEDIA_TYPE mediaType;

    int audioStreamIndex;
    int videoStreamIndex;  // 视频流索引
    int defaltStreamIndex; // 默认流索引
    double defalt_time_base_q2d_ms;

    double curPts = 0.0;

    const int *m_type; // 控制播放状态

    // 初始化
    int initFFmpeg(const QString &filePath);

    // 音频相关结构体初始化, 成功返回audioStreamIndex, 无音频流返回-1, 失败抛出FFMPEG_INIT_ERROR
    int initAudioDecoder();
    // 视频相关结构体初始化, 成功返回videoStreamIndex, 无视频流返回-1, 失败抛出FFMPEG_INIT_ERROR
    int initVideoDecoder(QList<AVHWDeviceType> &devices);
    // 硬解所需相关
    void initHwdeviceCtx(const AVCodec *videoCodec, int videoWidth, QList<AVHWDeviceType> &devices, AVPixelFormat &hw_device_pix_fmt, AVHWDeviceType &hw_device_type, AVBufferRef **hw_device_ctx);

    // AVCodecContext *getCudaDecoder
    int initCodec(AVCodecContext **codecContext, AVCodecParameters *codecParameters, const AVCodec *codec);

    void clean();
    void clearPacketQueue();

    void debugError(FFMPEG_INIT_ERROR error);

    void decodeAudio();
    void decodeVideo();
    void decodeMultMedia();

public:
    explicit Decoder(const int *_type, QObject *parent = nullptr);
    ~Decoder();

    void setVideoPath(const QString &filePath);

    bool resume();

    AudioDecoder *getAudioDecoder() const { return audioDecoder; }
    VideoDecoder *getVideoDecoder() const { return videoDecoder; }

    // 得到总音频帧数
    int64_t getAudioFrameCount() const;
    // 得到总视频帧数
    int64_t getVideoFrameCount() const;
    // 得到总时长
    int64_t getDuration() const;

    // 获取支持的硬解码器
    QList<QString> getSupportedHwDecoderNames();
};

class AudioDecoder : public QObject
{
    friend class Decoder;
    Q_OBJECT
signals:
    void sendAudioBuffer(uint8_t *audioBuffer, int bufferSize, double pts);

private:
    AVCodecContext *codecContext{nullptr};
    SwrContext *swrContext{nullptr};

    int audioStreamIndex;

    double time_base_q2d_ms;

    double lastPts = -1.0;

    void clean();

public:
    AudioDecoder(QObject *parent = nullptr) : QObject(parent) {}
    ~AudioDecoder() = default;

    // 将音频帧转换为 PCM 格式
    // 返回值: 转换后音频数据的大小
    int transferFrameToPCM(AVFrame *frame, uint8_t *dstBuffer);

    void decodeAudioPacket(AVPacketUniquePtr packet);
};

class VideoDecoder : public QObject
{
    friend class Decoder;
    Q_OBJECT
signals:
    void sendVideoFrame(uint8_t *pixelData, int pixelWidth, int pixelHeight, double pts);

private:
    AVCodecContext *codecContext{nullptr};

    AVBufferRef *hw_device_ctx = nullptr;
    enum AVPixelFormat hw_device_pix_fmt = AV_PIX_FMT_NONE;
    enum AVHWDeviceType hw_device_type = AV_HWDEVICE_TYPE_NONE;

    int videoStreamIndex; // 视频流索引

    double time_base_q2d_ms;

    double lastPts = -1.0;

    void clean();

    // 将硬件解码后的数据拷贝到内存中(但部分数据会消失, 例如pts)
    void transferDataFromHW(AVFrame **frame);

    uint8_t *copyNv12Data(uint8_t **pixelData, int *linesize, int pixelWidth, int pixelHeight);
    uint8_t *copyYuv420pData(uint8_t **pixelData, int *linesize, int pixelWidth, int pixelHeight);
    uint8_t *copyDefaultData(AVFrame *rawFrame);

public:
    VideoDecoder(QObject *parent = nullptr) : QObject(parent) {}
    ~VideoDecoder() = default;

    void decodeVideoPacket(AVPacketUniquePtr packet);

    AVFrame *transFrameToRGB24(AVFrame *frame, int pixelWidth, int pixelHeight);
    AVFrame *transFrameToDstFmt(AVFrame *srcFrame, int pixelWidth, int pixelHeight, AVPixelFormat dstFormat);

    int writeOneFrame(AVFrame *frame, int pixelWidth, int pixelHeight, QString fileName);
};
