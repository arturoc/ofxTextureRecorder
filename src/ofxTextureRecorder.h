/*
 * ofxTextureRecorder.h
 *
 *  Created on: Oct 14, 2014
 *      Author: arturo
 */
#pragma once
#include "ofMain.h"
#include <future>
class ofxTextureRecorder{
public:
    ofxTextureRecorder();
    ~ofxTextureRecorder();
    void setup(int w, int h, ofPixelFormat pixelFormat, ofImageFormat imageFormat_);
    void save(const ofTexture & tex);

private:
    ofThreadChannel<unsigned char *> channel;
    ofThreadChannel<std::pair<std::string, ofPixels>> pixelsChannel;
    ofThreadChannel<std::pair<std::string, ofBuffer>> encodedChannel;
    ofThreadChannel<bool> channelReady;
    bool firstFrame;
    ofBufferObject pixelBufferBack, pixelBufferFront;
    ofPixelFormat pixelFormat;
    ofImageFormat imageFormat;
    int width;
    int height;
    int frame = 0;
    std::condition_variable done;
    std::vector<std::future<bool>> waiting;
    std::thread saveThread;
    std::vector<std::thread> encodeThreads;
    std::thread downloadThread;
};
