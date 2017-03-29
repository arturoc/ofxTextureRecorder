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
    ~ofxTextureRecorder();
	struct Settings{
		Settings(int w, int h);
		Settings(const ofTexture & tex);
		Settings(const ofTextureData & texData);

		int w;
		int h;
		GLenum textureInternalFormat = GL_RGB;
		ofImageFormat imageFormat = OF_IMAGE_FORMAT_PNG;
		string folderPath;
		// default number encoding threads == number of hw cores - 2
		size_t numThreads = std::max(1u, std::thread::hardware_concurrency() - 2);

	private:
		ofPixelFormat pixelFormat;
		GLenum glType;
		friend class ofxTextureRecorder;
	};
	void setup(int w, int h);
	void setup(const Settings & settings);
	void setup(const ofTexture & tex);
	void setup(const ofTextureData & texData);
    void save(const ofTexture & tex);
    void save(const ofTexture & tex, int frame);

private:
	void stopThreads();
	void createThreads(size_t numThreads);
    ofThreadChannel<std::pair<std::string, unsigned char *>> channel;
    ofThreadChannel<std::pair<std::string, ofPixels>> pixelsChannel;
	ofThreadChannel<std::pair<std::string, ofShortPixels>> shortPixelsChannel;
	ofThreadChannel<std::pair<std::string, ofFloatPixels>> floatPixelsChannel;
    ofThreadChannel<std::pair<std::string, ofBuffer>> encodedChannel;
    ofThreadChannel<bool> channelReady;
	bool firstFrame = true;
    ofBufferObject pixelBufferBack, pixelBufferFront;
    ofPixelFormat pixelFormat;
    ofImageFormat imageFormat;

    std::string folderPath;
	int width = 0;
	int height = 0;
	int frame = 0;
	GLenum glType = GL_UNSIGNED_BYTE;
    std::condition_variable done;
    std::vector<std::future<bool>> waiting;
    std::thread saveThread;
    std::vector<std::thread> encodeThreads;
    std::thread downloadThread;
};
