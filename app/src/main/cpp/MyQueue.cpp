//
// Created by tck on 2020/11/27.
//

#include "MyQueue.h"

MyQueue::MyQueue(MyPlayerStatus *_playerStatus) : playerStatus(_playerStatus) {

    pthread_mutex_init(&mutexPacket, nullptr);
    pthread_cond_init(&condPacket, nullptr);

}


int MyQueue::putAvPacket(AVPacket *packet) {

    pthread_mutex_lock(&mutexPacket);

    queuePacket.push(packet);

//    if (LOG_DEBUG) {
//        LOGD("放入一个AVpacket到队里面， 个数为：%d", queuePacket.size());
//    }
    pthread_cond_signal(&condPacket);
    pthread_mutex_unlock(&mutexPacket);
    return 0;
}

int MyQueue::getAvPacket(AVPacket *packet) {

    pthread_mutex_lock(&mutexPacket);

    while (playerStatus != NULL && !playerStatus->exit) {
        if (queuePacket.size() > 0) {
            AVPacket *avPacket = queuePacket.front();
            if (av_packet_ref(packet, avPacket) == 0) {
                queuePacket.pop();
            }
            av_packet_free(&avPacket);
            av_free(avPacket);
            avPacket = NULL;
//            if (LOG_DEBUG) {
//                LOGD("从队列里面取出一个AVpacket，还剩下 %d 个", queuePacket.size());
//            }
            break;
        } else {
            pthread_cond_wait(&condPacket, &mutexPacket);
        }
    }
    pthread_mutex_unlock(&mutexPacket);
    return 0;
}

int MyQueue::getQueueSize() {
    int size = 0;
    pthread_mutex_lock(&mutexPacket);
    size = queuePacket.size();
    pthread_mutex_unlock(&mutexPacket);
    return size;
}


MyQueue::~MyQueue() {
    clearAvPacket();
}

void MyQueue::clearAvPacket() {
    if (LOG_DEBUG) {
        LOGD("释放队列--->开始");
    }
    pthread_cond_signal(&condPacket);
    pthread_mutex_unlock(&mutexPacket);
    while (!queuePacket.empty()) {
        AVPacket *packet = queuePacket.front();
        queuePacket.pop();
        av_packet_free(&packet);
        av_free(packet);
        packet = NULL;
    }
    pthread_mutex_unlock(&mutexPacket);

    if (LOG_DEBUG) {
        LOGD("释放队列--->完成");
    }
}
