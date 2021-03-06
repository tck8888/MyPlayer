//
// Created by tck on 2020/11/26.
//

#include "MusicPlayer.h"
#include <cstring>
#include <malloc.h>


MusicPlayer::MusicPlayer(JavaVM *_javaVM,
                         JNIEnv *_env, jobject &_jobj) {
    this->helper = new JavaCallHelper(_javaVM, _env, _jobj);
    this->playerStatus = new MyPlayerStatus();
    this->exit = false;
    pthread_mutex_init(&init_mutex, NULL);
    pthread_mutex_init(&seek_mutex, NULL);
}


void MusicPlayer::setDataSource(const char *path_) {
    path = new char[strlen(path_) + 1];
    strcpy(path, path_);
}

void *decodeFFmpeg(void *data) {
    auto *player = (MusicPlayer *) data;
    player->decodeFFmpegThread();
    pthread_exit(&player->decodeThread);
    return NULL;
}

int avformat_callback(void *ctx) {
    auto *player = (MusicPlayer *) ctx;
    if (player->playerStatus->exit) {
        return AVERROR_EOF;
    }
    return 0;
}

void MusicPlayer::decodeFFmpegThread() {

    pthread_mutex_lock(&init_mutex);

    avformat_network_init();

    avFormatContext = avformat_alloc_context();
    avFormatContext->interrupt_callback.callback = avformat_callback;
    avFormatContext->interrupt_callback.opaque = this;

    if (avformat_open_input(&avFormatContext, path, NULL, NULL) != 0) {
        if (LOG_DEBUG) {
            LOGE("can not open url :%s", path);
            helper->onCallOnError(1001, "can not open url");
            exit = true;
            pthread_mutex_unlock(&init_mutex);
            return;
        }
    }

    if (avformat_find_stream_info(avFormatContext, NULL) < 0) {
        if (LOG_DEBUG) {
            LOGE("can not find streams from %s", path);
            helper->onCallOnError(1002, "can not find streams from url");
            exit = true;
            pthread_mutex_unlock(&init_mutex);
            return;
        }
    }

    for (int i = 0; i < avFormatContext->nb_streams; ++i) {
        if (avFormatContext->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            if (audio == NULL) {
                audio = new AudioChannel(this->playerStatus,
                                         avFormatContext->streams[i]->codecpar->sample_rate,
                                         helper);
                audio->streamIndex = i;
                audio->codecpar = avFormatContext->streams[i]->codecpar;
                //avFormatContext->streams[i]->duration单位us  转换为秒需要除以1000000 1000000
                audio->duration = (avFormatContext->streams[i]->duration / AV_TIME_BASE);
                audio->time_base = avFormatContext->streams[i]->time_base;
                duration = audio->duration;
                break;
            }
        }
    }
    if (audio == NULL) {
        if (LOG_DEBUG) {
            LOGE("audio = NULL");
        }
        exit = true;
        pthread_mutex_unlock(&init_mutex);
        return;
    }

    AVCodec *avCodec = avcodec_find_decoder(audio->codecpar->codec_id);
    if (!avCodec) {
        if (LOG_DEBUG) {
            LOGE("can not find decoder");
        }
        helper->onCallOnError(1003, "can not find decoder");
        exit = true;
        pthread_mutex_unlock(&init_mutex);
        return;
    }
    audio->avCodecContext = avcodec_alloc_context3(avCodec);
    if (!audio->avCodecContext) {
        if (LOG_DEBUG) {
            LOGE("can not alloc new decodecctx");
        }
        helper->onCallOnError(1004, "can not alloc new decodecctx");
        exit = true;
        pthread_mutex_unlock(&init_mutex);
        return;
    }

    if (avcodec_parameters_to_context(audio->avCodecContext, audio->codecpar) < 0) {
        if (LOG_DEBUG) {
            LOGE("can not fill decodecctx");
        }
        helper->onCallOnError(1005, "ccan not fill decodecctx");
        exit = true;
        pthread_mutex_unlock(&init_mutex);
        return;
    }

    if (avcodec_open2(audio->avCodecContext, avCodec, NULL) != 0) {
        if (LOG_DEBUG) {
            LOGE("cant not open audio strames");
        }
        helper->onCallOnError(1006, "cant not open audio strames");
        exit = true;
        pthread_mutex_unlock(&init_mutex);
        return;
    }

    if (helper != NULL) {
        if (playerStatus != NULL && !playerStatus->exit) {
            helper->onCallJavaPrepared(THREAD_CHILD);
        } else {
            exit = true;
        }
    }

    pthread_mutex_unlock(&init_mutex);
}

void MusicPlayer::prepare() {
    pthread_create(&decodeThread, NULL, decodeFFmpeg, this);
}

void MusicPlayer::start() {

    if (audio == NULL) {
        if (LOG_DEBUG) {
            LOGE("audio = NULL");
        }
        return;
    }
    audio->play();

    while (playerStatus != NULL && !playerStatus->exit) {
        if (playerStatus->seek) {
            continue;
        }

        if (audio->queue->getQueueSize() > 40) {
            continue;
        }

        AVPacket *avPacket = av_packet_alloc();
        if (av_read_frame(avFormatContext, avPacket) == 0) {
            if (avPacket->stream_index == audio->streamIndex) {
                audio->queue->putAvPacket(avPacket);
            } else {
                av_packet_free(&avPacket);
                av_free(avPacket);
            }
        } else {
            av_packet_free(&avPacket);
            av_free(avPacket);
            while (playerStatus != NULL && !playerStatus->exit) {
                if (audio->queue->getQueueSize() > 0) {
                    continue;
                } else {
                    playerStatus->exit = true;
                    break;
                }
            }
        }
    }

    exit = true;

    if (LOG_DEBUG) {
        LOGD("解码完成");
    }
}

void MusicPlayer::onResume() {
    if (audio != NULL) {
        audio->onResume();
    }
}

void MusicPlayer::onPause() {
    if (audio != NULL) {
        audio->onPause();
    }
}

void MusicPlayer::release() {
    if (LOG_DEBUG) {
        LOGE("开始释放ffmpeg--->1");
    }
    if (playerStatus->exit) {
        return;
    }
    if (LOG_DEBUG) {
        LOGE("开始释放ffmpeg--->2");
    }
    playerStatus->exit = true;
    pthread_mutex_lock(&init_mutex);

    int sleepCount = 0;

    while (!exit) {
        if (sleepCount > 1000) {
            exit = true;
        }
        if (LOG_DEBUG) {
            LOGE("wait ffmpeg  exit %d", sleepCount);
        }
        sleepCount++;
        av_usleep(1000 * 10);//暂停10毫秒
    }


    if (LOG_DEBUG) {
        LOGE("释放 Audio");
    }

    if (audio != NULL) {
        audio->release();
        delete (audio);
        audio = NULL;
    }

    if (LOG_DEBUG) {
        LOGE("释放 封装格式上下文");
    }

    if (avFormatContext != NULL) {
        avformat_close_input(&avFormatContext);
        avformat_free_context(avFormatContext);
        avFormatContext = NULL;
    }
    if (LOG_DEBUG) {
        LOGE("释放 helper");
    }
    if (helper != NULL) {
        delete helper;
        helper = NULL;
    }
    if (LOG_DEBUG) {
        LOGE("释放 playstatus");
    }
    if (playerStatus != NULL) {
        delete playerStatus;
        playerStatus = NULL;
    }
    if (LOG_DEBUG) {
        LOGE("释放 path");
    }
    delete path;
    path = NULL;

    pthread_mutex_unlock(&init_mutex);

    if (LOG_DEBUG) {
        LOGE("释放 MusicPlayer---->完成");
    }
}

MusicPlayer::~MusicPlayer() {
    pthread_mutex_destroy(&init_mutex);
    pthread_mutex_destroy(&seek_mutex);
}

void MusicPlayer::seek(int64_t seconds) {
    if (duration <= 0) {
        return;
    }

    if (seconds >= 0 && seconds <= duration) {
        if (audio != NULL) {
            playerStatus->seek = true;
            audio->queue->clearAvPacket();
            audio->clock = 0;
            audio->last_time = 0;
            pthread_mutex_lock(&seek_mutex);
            int64_t rel = seconds * AV_TIME_BASE;
            avformat_seek_file(avFormatContext, -1, INT64_MIN, rel, INT64_MAX, 0);
            pthread_mutex_unlock(&seek_mutex);
            playerStatus->seek = false;
        }
    }
}







