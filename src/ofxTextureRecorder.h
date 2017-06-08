/*
 * ofxTextureRecorder.h
 *
 *  Created on: Oct 14, 2014
 *      Author: arturo
 */
#pragma once
#include "ofMain.h"
#include "half.hpp"
#include <future>

#if OFX_VIDEO_RECORDER
#include "ofxVideoRecorder.h"
#endif
#if OFX_HPVLIB
#include "HPVCreator.hpp"
#endif

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
		/// number encoding threads, default == number of hw cores - 2
		size_t numThreads = std::max(1u, std::thread::hardware_concurrency() - 2);
		/// maximum RAM to use in bytes
		size_t maxMemoryUsage = 2000000000;

	private:
		ofPixelFormat pixelFormat;
		GLenum glType;
		friend class ofxTextureRecorder;
	};

#if OFX_VIDEO_RECORDER || OFX_HPVLIB
	struct VideoSettings{
		VideoSettings(int w, int h, float fps);
		VideoSettings(const ofTexture & tex, float fps);
		VideoSettings(const ofTextureData & texData, float fps);

		int w;
		int h;
		float fps;
		string bitrate = "200k";
		string videoCodec = "mp4";
		string extrasettings = "";

		GLenum textureInternalFormat = GL_RGB;
		string videoPath;
		/// maximum RAM to use in bytes
		size_t maxMemoryUsage = 2000000000;
		/// number encoding threads, default == number of hw cores - 2
		size_t numThreads = std::max(1u, std::thread::hardware_concurrency() - 2);

	private:
		ofPixelFormat pixelFormat;
		GLenum glType;
		friend class ofxTextureRecorder;
	};
	void setup(const VideoSettings & settings);
#endif

	void setup(int w, int h);
	void setup(const Settings & settings);
	void setup(const ofTexture & tex);
	void setup(const ofTextureData & texData);
    void save(const ofTexture & tex);
    void save(const ofTexture & tex, int frame);
	void stop(){
		if(!encodeThreads.empty()){
			stopThreads();
		}
	}

	uint64_t getAvgTimeGpuDownload() const;
	uint64_t getAvgTimeEncode() const;
	uint64_t getAvgTimeSave() const;
	uint64_t getAvgTimeTextureCopy() const;
private:
	void stopThreads();
	void createThreads(size_t numThreads);
	ofPixels getBuffer();
	ofShortPixels getShortBuffer();
	ofFloatPixels getFloatBuffer();
	std::vector<half_float::half> getHalfFloatBuffer();
	struct Buffer{
		size_t id;
		std::string path;
		void * data;
	};
	template<typename T>
	struct Frame{
		size_t id;
		std::string path;
		T pixels;
	};

	ofThreadChannel<Buffer> channel;
	ofThreadChannel<Frame<ofPixels>> pixelsChannel;
	size_t poolSize = 0;
	ofThreadChannel<Frame<ofShortPixels>> shortPixelsChannel;
	size_t shortPoolSize = 0;
	ofThreadChannel<Frame<ofFloatPixels>> floatPixelsChannel;
	size_t floatPoolSize = 0;
	ofThreadChannel<Frame<std::vector<half_float::half>>> halffloatPixelsChannel;
	size_t halfFloatPoolSize = 0;
	ofThreadChannel<ofPixels> returnPixelsChannel;
	ofThreadChannel<ofShortPixels> returnShortPixelsChannel;
	ofThreadChannel<ofFloatPixels> returnFloatPixelsChannel;
	ofThreadChannel<std::vector<half_float::half>> returnHalfFloatPixelsChannel;
    ofThreadChannel<std::pair<std::string, ofBuffer>> encodedChannel;
	ofThreadChannel<size_t> channelReady;
	bool firstFrame = true;
	bool isVideo = false;
	bool isHPV = false;
	std::vector<ofBufferObject> pixelBuffers;
    ofPixelFormat pixelFormat;
    ofImageFormat imageFormat;

    std::string folderPath;
	int width = 0;
	int height = 0;
	int frame = 0;
	GLenum glType = GL_UNSIGNED_BYTE;
	size_t size = 0;
	size_t maxMemoryUsage = 2000000000;
    std::condition_variable done;
    std::vector<std::future<bool>> waiting;
    std::thread saveThread;
    std::vector<std::thread> encodeThreads;
	std::vector<std::thread> halfDecodingThreads;
	std::thread downloadThread;
	std::queue<size_t> buffersReady;
	std::queue<size_t> buffersCopying;
	std::queue<GLuint> copyQueryReady;
	std::queue<GLuint> copyQueryCopying;
	uint64_t timeDownload = 0, halfDecodingTime = 0, encodingTime = 0, saveTime = 0, copyTextureTime = 0;


#if OFX_VIDEO_RECORDER
	ofxVideoRecorder videoRecorder;
#endif
#if OFX_HPVLIB
	HPV::HPVCreator hpvCreator;
	ThreadSafe_Queue<HPV::HPVCompressionProgress> hpvProgress;
	std::thread hpvProgressThread;
#endif
};
