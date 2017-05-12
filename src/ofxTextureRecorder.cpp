/*
 * ofxTextureRecorder.cpp
 *
 *  Created on: Oct 14, 2014
 *      Author: arturo
 */

#include "ofxTextureRecorder.h"

ofxTextureRecorder::~ofxTextureRecorder(){
	if(!encodeThreads.empty()){
		stopThreads();
	}
}

void ofxTextureRecorder::setup(int w, int h){
	setup(Settings(w,h));
}

void ofxTextureRecorder::setup(const ofTexture & tex){
	setup(Settings(tex));
}

void ofxTextureRecorder::setup(const ofTextureData & texData){
	setup(Settings(texData));
}

void ofxTextureRecorder::setup(const Settings & settings){
	if(!encodeThreads.empty()){
		stopThreads();
	}
	width = settings.w;
	height = settings.h;
	pixelFormat = settings.pixelFormat;
	imageFormat = settings.imageFormat;
	folderPath = settings.folderPath;
	glType = settings.glType;
	isVideo = false;
	maxMemoryUsage = settings.maxMemoryUsage;
	if (!folderPath.empty()) folderPath = ofFilePath::addTrailingSlash(folderPath);

	frame = 0;
    firstFrame = true;
	auto bufferSize = 0;
	switch(glType){
		case GL_UNSIGNED_BYTE:
			bufferSize = ofPixels::bytesFromPixelFormat(width,height,pixelFormat);
		break;
		case GL_SHORT:
		case GL_UNSIGNED_SHORT:
			bufferSize = ofShortPixels::bytesFromPixelFormat(width,height,pixelFormat);
		break;
		case GL_FLOAT:
			bufferSize = ofFloatPixels::bytesFromPixelFormat(width,height,pixelFormat);
		break;
		case GL_HALF_FLOAT:
			bufferSize = ofFloatPixels::bytesFromPixelFormat(width,height,pixelFormat)/2;
		break;
	}

	// number of gpu buffers to copy the texture
	size_t numGpuBuffers = 2;
	for(size_t i=0; i<std::min(numGpuBuffers, size_t(2)); i++){
		pixelBuffers.emplace_back();
		pixelBuffers.back().allocate(bufferSize, GL_DYNAMIC_READ);
		buffersReady.push(i);
		GLuint copyQuery;
		glGenQueries(1,&copyQuery);
		copyQueryReady.push(copyQuery);
	}
	auto numChannels = 0;
	switch(pixelFormat){
		case OF_PIXELS_RGB:
			numChannels = 3;
		break;
		case OF_PIXELS_RGBA:
			numChannels = 4;
		break;
		case OF_PIXELS_GRAY:
			numChannels = 1;
		break;
	};
	size = width * height * numChannels;
	createThreads(settings.numThreads);
}

#if OFX_VIDEO_RECORDER
void ofxTextureRecorder::setup(const ofxTextureRecorder::VideoSettings & settings){
	if(!encodeThreads.empty()){
		stopThreads();
	}
	width = settings.w;
	height = settings.h;
	pixelFormat = settings.pixelFormat;
	folderPath = "";
	glType = settings.glType;
	isVideo = true;
	maxMemoryUsage = settings.maxMemoryUsage;

	frame = 0;
	firstFrame = true;
	auto bufferSize = 0;
	switch(glType){
		case GL_UNSIGNED_BYTE:
			bufferSize = ofPixels::bytesFromPixelFormat(width,height,pixelFormat);
		break;
		case GL_SHORT:
		case GL_UNSIGNED_SHORT:
			bufferSize = ofShortPixels::bytesFromPixelFormat(width,height,pixelFormat);
		break;
		case GL_FLOAT:
			bufferSize = ofFloatPixels::bytesFromPixelFormat(width,height,pixelFormat);
		break;
		case GL_HALF_FLOAT:
			bufferSize = ofFloatPixels::bytesFromPixelFormat(width,height,pixelFormat)/2;
		break;
	}

	// number of gpu buffers to copy the texture
	size_t numGpuBuffers = 2;
	for(size_t i=0; i<std::min(numGpuBuffers, size_t(2)); i++){
		pixelBuffers.emplace_back();
		pixelBuffers.back().allocate(bufferSize, GL_DYNAMIC_READ);
		buffersReady.push(i);
		GLuint copyQuery;
		glGenQueries(1,&copyQuery);
		copyQueryReady.push(copyQuery);
	}
	auto numChannels = 0;
	switch(pixelFormat){
		case OF_PIXELS_RGB:
			numChannels = 3;
		break;
		case OF_PIXELS_RGBA:
			numChannels = 4;
		break;
		case OF_PIXELS_GRAY:
			numChannels = 1;
		break;
	};
	size = width * height * numChannels;

	string absFilePath = ofFilePath::getAbsolutePath(settings.videoPath);
	string moviePath = ofFilePath::getAbsolutePath(settings.videoPath);

	stringstream outputSettings;
	outputSettings
	<< " " << settings.extrasettings << " "
	<< " -c:v " << settings.videoCodec
	<< " -b:v " << settings.bitrate
	<< " \"" << absFilePath << "\"";

	videoRecorder.setupCustomOutput(width, height, settings.fps, 0, 0, outputSettings.str(), false, false);
	videoRecorder.start();
	createThreads(1);
}
#endif

void ofxTextureRecorder::save(const ofTexture & tex){
	save(tex, frame++);
}

void ofxTextureRecorder::save(const ofTexture & tex, int frame_){
	if(buffersReady.size()<std::max(pixelBuffers.size()/2 + 1,size_t(2))){
		{
			GLuint time;
			auto copyQuery = copyQueryCopying.front();
			glGetQueryObjectuiv(copyQuery, GL_QUERY_RESULT, &time);

			copyTextureTime = copyTextureTime * 0.9 + time/1000. * 0.1;
			copyQueryReady.push(copyQuery);
		}
		size_t back = 0;
		if(!buffersReady.empty()){
			back = buffersReady.front();
			buffersReady.pop();
		}else if(channelReady.receive(back)){
			pixelBuffers[back].unmap();
		}else{
			return;
		}
		auto copyQuery = copyQueryReady.front();
		copyQueryReady.pop();
		glBeginQuery(GL_TIME_ELAPSED, copyQuery);
		tex.copyTo(pixelBuffers[back]);
		glEndQuery(GL_TIME_ELAPSED);
		copyQueryCopying.push(copyQuery);

		if(!buffersCopying.empty()){
			auto front = buffersCopying.front();
			buffersCopying.pop();
			pixelBuffers[front].bind(GL_PIXEL_UNPACK_BUFFER);
			auto pixels = pixelBuffers[front].map<unsigned char>(GL_READ_ONLY);
			if(pixels){
				if(isVideo){
					Buffer buffer{front, "", pixels};
					channel.send(buffer);
				}else{
					std::ostringstream oss;
					oss << folderPath << ofToString(frame_, 5, '0') << "." << ofImageFormatExtension(imageFormat);
					Buffer buffer{front, oss.str(), pixels};
					channel.send(buffer);
				}
			}else{
				ofLogError(__FUNCTION__) << "Couldn't map buffer";
			}
		}
		buffersCopying.push(back);
	}else{
		auto back = buffersReady.front();
		buffersReady.pop();
		auto copyQuery = copyQueryReady.front();
		copyQueryReady.pop();
		glBeginQuery(GL_TIME_ELAPSED, copyQuery);
		tex.copyTo(pixelBuffers[back]);
		glEndQuery(GL_TIME_ELAPSED);
		buffersCopying.push(back);
		copyQueryCopying.push(copyQuery);
	}
}

void ofxTextureRecorder::stopThreads(){
	channel.close();
	channelReady.close();

	switch(glType){
		case GL_UNSIGNED_BYTE:
			while(!pixelsChannel.empty()){
				ofSleepMillis(100);
			}
			pixelsChannel.close();
		break;
		case GL_SHORT:
		case GL_UNSIGNED_SHORT:
			while(!shortPixelsChannel.empty()){
				ofSleepMillis(100);
			}
			shortPixelsChannel.close();
		break;
		case GL_FLOAT:
			while(!floatPixelsChannel.empty()){
				ofSleepMillis(100);
			}
			floatPixelsChannel.close();
		break;
		case GL_HALF_FLOAT:
			while(!halffloatPixelsChannel.empty()){
				ofSleepMillis(100);
			}
			halffloatPixelsChannel.close();
			while(!floatPixelsChannel.empty()){
				ofSleepMillis(100);
			}
			floatPixelsChannel.close();
			for(auto &t: halfDecodingThreads){
				t.join();
			}
		break;
	}

	while(!encodedChannel.empty()){
		ofSleepMillis(100);
	}
	encodedChannel.close();
	for(auto &t: encodeThreads){
		t.join();
	}
	encodeThreads.clear();
	downloadThread.join();
	saveThread.join();
#if OFX_VIDEO_RECORDER
	if(isVideo){
		videoRecorder.close();
	}
#endif
}

void ofxTextureRecorder::createThreads(size_t numThreads){
	for(size_t i=0; i<numThreads; i++){
		switch(glType){
			case GL_UNSIGNED_BYTE:{
				ofPixels pixels;
				pixels.allocate(width, height, pixelFormat);
				returnPixelsChannel.send(std::move(pixels));
				poolSize += 1;
			}
			case GL_SHORT:
			case GL_UNSIGNED_SHORT:{
				ofShortPixels pixels;
				pixels.allocate(width, height, pixelFormat);
				returnShortPixelsChannel.send(std::move(pixels));
				shortPoolSize += 1;
			}
			case GL_FLOAT:{
				ofFloatPixels pixels;
				pixels.allocate(width, height, pixelFormat);
				returnFloatPixelsChannel.send(std::move(pixels));
				floatPoolSize += 1;
			}
			case GL_HALF_FLOAT:{
				std::vector<half_float::half> pixels;
				pixels.resize(size);
				returnHalfFloatPixelsChannel.send(std::move(pixels));
				halfFloatPoolSize += 1;
			}
		}
	}

	downloadThread = std::thread([&]{
		Buffer buffer;
		switch(glType){
			case GL_UNSIGNED_BYTE:{
				while(channel.receive(buffer)){
					auto then = ofGetElapsedTimeMicros();
					ofPixels pixels = getBuffer();
					pixels.setFromPixels((unsigned char*)buffer.data, width, height, pixelFormat);
					channelReady.send(buffer.id);
					auto now = ofGetElapsedTimeMicros();
					timeDownload = timeDownload * 0.9 + (now - then) * 0.1;
					pixelsChannel.send(std::make_pair(buffer.path, std::move(pixels)));
				}
			}break;
			case GL_SHORT:
			case GL_UNSIGNED_SHORT:{
				while(channel.receive(buffer)){
					auto then = ofGetElapsedTimeMicros();
					ofShortPixels pixels = getShortBuffer();
					pixels.setFromPixels((unsigned short*)buffer.data, width, height, pixelFormat);
					channelReady.send(buffer.id);
					auto now = ofGetElapsedTimeMicros();
					timeDownload = timeDownload * 0.9 + (now - then) * 0.1;
					shortPixelsChannel.send(std::make_pair(buffer.path, std::move(pixels)));
				}
			}break;
			case GL_FLOAT:{
				while(channel.receive(buffer)){
					auto then = ofGetElapsedTimeMicros();
					ofFloatPixels pixels = getFloatBuffer();
					pixels.setFromPixels((float*)buffer.data, width, height, pixelFormat);
					channelReady.send(buffer.id);
					auto now = ofGetElapsedTimeMicros();
					timeDownload = timeDownload * 0.9 + (now - then) * 0.1;
					floatPixelsChannel.send(std::make_pair(buffer.path, std::move(pixels)));
				}
			}break;
			case GL_HALF_FLOAT:{
				while(channel.receive(buffer)){
					auto then = ofGetElapsedTimeMicros();
					std::vector<half_float::half> halfdata = getHalfFloatBuffer();
					auto halfptr = (half_float::half*)buffer.data;
					memcpy(halfdata.data(), halfptr, size * sizeof(half_float::half));
					channelReady.send(buffer.id);
					auto now = ofGetElapsedTimeMicros();
					timeDownload = timeDownload * 0.9 + (now - then) * 0.1;
					halffloatPixelsChannel.send(std::make_pair(buffer.path, std::move(halfdata)));
				}
			}break;
		}
	});

	if(glType==GL_HALF_FLOAT){
		for(size_t i=0;i<numThreads/2;i++){
			halfDecodingThreads.emplace_back(([&]{
				std::pair<std::string, std::vector<half_float::half>> halfdata;
				while(halffloatPixelsChannel.receive(halfdata)){
					auto then = ofGetElapsedTimeMicros();
					ofFloatPixels pixels = getFloatBuffer();
					auto halfptr = halfdata.second.data();
					for(auto & p: pixels){
						p = *halfptr++;
					}
					returnHalfFloatPixelsChannel.send(std::move(halfdata.second));
					auto now = ofGetElapsedTimeMicros();
					halfDecodingTime = halfDecodingTime * 0.9 + (now - then) * 0.1;
					floatPixelsChannel.send(std::make_pair(halfdata.first, std::move(pixels)));
				}
			}));
		}
	}


	ofLogNotice(__FUNCTION__) << "Initializing with " << numThreads << " encoding threads";
	for(size_t i=0;i<numThreads;i++){
		encodeThreads.emplace_back([&]{
			if(isVideo){

#if OFX_VIDEO_RECORDER
				ofPixels rgb8Pixels;
				switch(glType){
					case GL_UNSIGNED_BYTE:{
						std::pair<std::string, ofPixels> pixels;
						while(pixelsChannel.receive(pixels)){
							auto then = ofGetElapsedTimeMicros();
							rgb8Pixels = pixels.second;
							rgb8Pixels.setNumChannels(3);
							videoRecorder.addFrame(rgb8Pixels);
							returnPixelsChannel.send(std::move(pixels.second));
							auto now = ofGetElapsedTimeMicros();
							encodingTime = halfDecodingTime * 0.9 + (now - then) * 0.1;
						}
					}break;
					case GL_SHORT:
					case GL_UNSIGNED_SHORT:{
						std::pair<std::string, ofShortPixels> pixels;
						while(shortPixelsChannel.receive(pixels)){
							auto then = ofGetElapsedTimeMicros();
							rgb8Pixels = pixels.second;
							rgb8Pixels.setNumChannels(3);
							videoRecorder.addFrame(rgb8Pixels);
							returnShortPixelsChannel.send(std::move(pixels.second));
							auto now = ofGetElapsedTimeMicros();
							encodingTime = halfDecodingTime * 0.9 + (now - then) * 0.1;
						}
					}break;
					case GL_FLOAT:
					case GL_HALF_FLOAT:{
						std::pair<std::string, ofFloatPixels> pixels;
						while(floatPixelsChannel.receive(pixels)){
							auto then = ofGetElapsedTimeMicros();
							rgb8Pixels = pixels.second;
							rgb8Pixels.setNumChannels(3);
							videoRecorder.addFrame(rgb8Pixels);
							returnFloatPixelsChannel.send(std::move(pixels.second));
							auto now = ofGetElapsedTimeMicros();
							encodingTime = halfDecodingTime * 0.9 + (now - then) * 0.1;
						}
					}break;
				}
#endif

			}else{
				switch(glType){
					case GL_UNSIGNED_BYTE:{
						std::pair<std::string, ofPixels> pixels;
						ofBuffer buffer;
						while(pixelsChannel.receive(pixels)){
							auto then = ofGetElapsedTimeMicros();
							buffer.clear();
							ofSaveImage(pixels.second, buffer, imageFormat, OF_IMAGE_QUALITY_BEST);
							returnPixelsChannel.send(std::move(pixels.second));
							auto now = ofGetElapsedTimeMicros();
							encodingTime = halfDecodingTime * 0.9 + (now - then) * 0.1;
							encodedChannel.send(std::make_pair(pixels.first, std::move(buffer)));
						}
					}break;
					case GL_SHORT:
					case GL_UNSIGNED_SHORT:{
						std::pair<std::string, ofShortPixels> pixels;
						ofBuffer buffer;
						while(shortPixelsChannel.receive(pixels)){
							auto then = ofGetElapsedTimeMicros();
							buffer.clear();
							ofSaveImage(pixels.second, buffer, imageFormat, OF_IMAGE_QUALITY_BEST);
							returnShortPixelsChannel.send(std::move(pixels.second));
							auto now = ofGetElapsedTimeMicros();
							encodingTime = halfDecodingTime * 0.9 + (now - then) * 0.1;
							encodedChannel.send(std::make_pair(pixels.first, std::move(buffer)));
						}
					}break;
					case GL_FLOAT:
					case GL_HALF_FLOAT:{
						std::pair<std::string, ofFloatPixels> pixels;
						ofBuffer buffer;
						while(floatPixelsChannel.receive(pixels)){
							auto then = ofGetElapsedTimeMicros();
							buffer.clear();
							ofSaveImage(pixels.second, buffer, imageFormat, OF_IMAGE_QUALITY_BEST);
							returnFloatPixelsChannel.send(std::move(pixels.second));
							auto now = ofGetElapsedTimeMicros();
							encodingTime = halfDecodingTime * 0.9 + (now - then) * 0.1;
							encodedChannel.send(std::make_pair(pixels.first, std::move(buffer)));
						}
					}break;
				}
			}
		});
	}
	saveThread = std::thread([&]{
		std::pair<std::string, ofBuffer> encoded;
		while(encodedChannel.receive(encoded)){
			auto then = ofGetElapsedTimeMicros();
			ofFile file(encoded.first, ofFile::WriteOnly);
			file.writeFromBuffer(encoded.second);
			auto now = ofGetElapsedTimeMicros();
			saveTime = halfDecodingTime * 0.9 + (now - then) * 0.1;
		}
	});
}

ofPixels ofxTextureRecorder::getBuffer(){
	ofPixels pixels;
	if(!returnPixelsChannel.tryReceive(pixels)){
		if(poolSize * ofPixels::bytesFromPixelFormat(width, height, pixelFormat) < maxMemoryUsage){
			pixels.allocate(width, height, pixelFormat);
			poolSize += 1;
		}else{
			returnPixelsChannel.receive(pixels);
		}
	}
	return pixels;
};

ofShortPixels ofxTextureRecorder::getShortBuffer(){
	ofShortPixels pixels;
	if(!returnShortPixelsChannel.tryReceive(pixels)){
		if(shortPoolSize * ofShortPixels::bytesFromPixelFormat(width, height, pixelFormat) < maxMemoryUsage){
			pixels.allocate(width, height, pixelFormat);
			shortPoolSize += 1;
		}else{
			returnShortPixelsChannel.receive(pixels);
		}
	}
	return pixels;
};

ofFloatPixels ofxTextureRecorder::getFloatBuffer(){
	if(glType==GL_FLOAT){
		ofFloatPixels pixels;
		if(!returnFloatPixelsChannel.tryReceive(pixels)){
			if(floatPoolSize * ofFloatPixels::bytesFromPixelFormat(width, height, pixelFormat) < maxMemoryUsage){
				pixels.allocate(width, height, pixelFormat);
				floatPoolSize += 1;
			}else{
				returnFloatPixelsChannel.receive(pixels);
			}
		}
		return pixels;
	}else{
		ofFloatPixels pixels;
		if(!returnFloatPixelsChannel.tryReceive(pixels)){
			auto floatSize = floatPoolSize * ofFloatPixels::bytesFromPixelFormat(width, height, pixelFormat);
			auto halfFloatSize = halfFloatPoolSize * size * sizeof(half_float::half);
			if(halfFloatSize + floatSize < maxMemoryUsage){
				pixels.allocate(width, height, pixelFormat);
				floatPoolSize += 1;
			}else{
				returnFloatPixelsChannel.receive(pixels);
			}
		}
		return pixels;
	}
};

std::vector<half_float::half> ofxTextureRecorder::getHalfFloatBuffer(){
	std::vector<half_float::half> pixels;
	if(!returnHalfFloatPixelsChannel.tryReceive(pixels)){
		auto floatSize = floatPoolSize * ofFloatPixels::bytesFromPixelFormat(width, height, pixelFormat);
		auto halfFloatSize = halfFloatPoolSize * size * sizeof(half_float::half);
		if(halfFloatSize + floatSize < maxMemoryUsage){
			pixels.resize(size);
			halfFloatPoolSize += 1;
		}else{
			returnHalfFloatPixelsChannel.receive(pixels);
		}
	}
	return pixels;
};

uint64_t ofxTextureRecorder::getAvgTimeEncode() const{
	return encodingTime;
}

uint64_t ofxTextureRecorder::getAvgTimeSave() const{
	return saveTime;
}

uint64_t ofxTextureRecorder::getAvgTimeGpuDownload() const{
	return timeDownload;
}

uint64_t ofxTextureRecorder::getAvgTimeTextureCopy() const{
	return copyTextureTime;
}

ofxTextureRecorder::Settings::Settings(int w, int h)
:w(w)
,h(h){}

ofxTextureRecorder::Settings::Settings(const ofTexture & tex)
	:ofxTextureRecorder::Settings(tex.getTextureData()){}

ofxTextureRecorder::Settings::Settings(const ofTextureData & texData)
:w(texData.width)
,h(texData.height)
,textureInternalFormat(texData.glInternalFormat){
	switch(ofGetImageTypeFromGLType(texData.glInternalFormat)){
		case OF_IMAGE_COLOR:
			pixelFormat = OF_PIXELS_RGB;
		break;
		case OF_IMAGE_COLOR_ALPHA:
			pixelFormat = OF_PIXELS_RGBA;
		break;
		case OF_IMAGE_GRAYSCALE:
			pixelFormat = OF_PIXELS_GRAY;
		break;
		default:
			ofLogError("ofxTextureRecorder") << "Unsupported texture format";
	}
	glType = ofGetGlTypeFromInternal(texData.glInternalFormat);

}

ofxTextureRecorder::VideoSettings::VideoSettings(int w, int h, float fps)
:w(w)
,h(h)
,fps(fps){}

ofxTextureRecorder::VideoSettings::VideoSettings(const ofTexture & tex, float fps)
	:ofxTextureRecorder::VideoSettings(tex.getTextureData(),fps){}

ofxTextureRecorder::VideoSettings::VideoSettings(const ofTextureData & texData, float fps)
:w(texData.width)
,h(texData.height)
,fps(fps)
,textureInternalFormat(texData.glInternalFormat){
	switch(ofGetImageTypeFromGLType(texData.glInternalFormat)){
		case OF_IMAGE_COLOR:
			pixelFormat = OF_PIXELS_RGB;
		break;
		case OF_IMAGE_COLOR_ALPHA:
			pixelFormat = OF_PIXELS_RGBA;
		break;
		case OF_IMAGE_GRAYSCALE:
			pixelFormat = OF_PIXELS_GRAY;
		break;
		default:
			ofLogError("ofxTextureRecorder") << "Unsupported texture format";
	}
	glType = ofGetGlTypeFromInternal(texData.glInternalFormat);

}
