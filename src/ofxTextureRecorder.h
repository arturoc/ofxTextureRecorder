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
    void setup(int w, int h, ofPixelFormat pixelFormat, ofImageFormat imageFormat, const string& folderPath = "");
	void save(const ofTexture & tex);
	void save(const ofTexture & tex, int frame);

private:
    ofThreadChannel<std::pair<std::string, unsigned char *>> channel;
    ofThreadChannel<std::pair<std::string, ofPixels>> pixelsChannel;
    ofThreadChannel<std::pair<std::string, ofBuffer>> encodedChannel;
    ofThreadChannel<bool> channelReady;
    bool firstFrame;
    ofBufferObject pixelBufferBack, pixelBufferFront;
    ofPixelFormat pixelFormat;
    ofImageFormat imageFormat;
	std::string folderPath;
    int width;
    int height;
    int frame = 0;
    std::condition_variable done;
    std::vector<std::future<bool>> waiting;
    std::thread saveThread;
    std::vector<std::thread> encodeThreads;
    std::thread downloadThread;
};
