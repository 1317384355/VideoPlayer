# VideoPlayer

#### 介绍
学习项目, 按自己对Qt低耦合的理解, FFmpeg的硬解, QOpenglWidget显示画面(yuv420, nv12) 的视频播放器, 测试了部分格式(AVC, HEVC-10bit, MPEG-4v, AAC, MP3)的硬解(部分视频不支持DXVA2, 仅支持cuda的视频不支持4k硬解), 线程为4个线程: 显示主线程, 解码线程, 音频播放线程, 视频同步线程

#### 使用说明

内置了ffmpeg的环境, 仅需搭建Qt环境即可测试运行, 本人环境为vsc, cmake, Qt5.15.2(mingw), 初期下载ffmpeg版本为ffmpeg-6.1.1-full_build-shared, 后续改为7.1
若qtc编译错误请使用cmake编译

#### 待办
目前交互功能较少, 仅能更换视频, 暂停, 点击快进(拖动快进不支持), 重播, 全屏功能尚待完善, 快进功能暂无思路
对于不同个格式目前应对手法较少, 尤其是未作出视频与音频的区分(暂时都是一套东西), 页面等都仍需完善
部分画面格式暂时都是简单的转为YUV420或NV12, 后续显示页面仍需更多支持
部分硬解(cuda)有适用范围(我的电脑不支持4k)但可以初始化的部分情况暂时无法在早期更好的区分
解码顺序需要根据DTS调整, 整体思路还需梳理, 目前回跳进度仍有bug(回跳后read_packet会有部分包为之前的包, 而我是根据pts之差睡眠, 会导致视频线程睡死)

#### 部分参考(我还记得的)
https://www.cnblogs.com/wangguchangqing/p/5900426.html
https://blog.csdn.net/qq_23282479/article/details/118993650