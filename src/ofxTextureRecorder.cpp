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
	if (!folderPath.empty()) folderPath = ofFilePath::addTrailingSlash(folderPath);

	frame = 0;
    firstFrame = true;
	switch(glType){
		case GL_UNSIGNED_BYTE:
			pixelBufferBack.allocate(ofPixels::bytesFromPixelFormat(width,height,pixelFormat), GL_DYNAMIC_READ);
			pixelBufferFront.allocate(ofPixels::bytesFromPixelFormat(width,height,pixelFormat), GL_DYNAMIC_READ);
		break;
		case GL_SHORT:
		case GL_UNSIGNED_SHORT:
			pixelBufferBack.allocate(ofShortPixels::bytesFromPixelFormat(width,height,pixelFormat), GL_DYNAMIC_READ);
			pixelBufferFront.allocate(ofShortPixels::bytesFromPixelFormat(width,height,pixelFormat), GL_DYNAMIC_READ);
		break;
		case GL_FLOAT:
		case GL_HALF_FLOAT:
			pixelBufferBack.allocate(ofFloatPixels::bytesFromPixelFormat(width,height,pixelFormat), GL_DYNAMIC_READ);
			pixelBufferFront.allocate(ofFloatPixels::bytesFromPixelFormat(width,height,pixelFormat), GL_DYNAMIC_READ);
		break;
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

void ofxTextureRecorder::save(const ofTexture & tex){
	save(tex, frame++);
}

void ofxTextureRecorder::save(const ofTexture & tex, int frame_){
    if(!firstFrame){
        bool ready;
        channelReady.receive(ready);
		pixelBufferBack.unmap();
    }
    firstFrame = false;
    tex.copyTo(pixelBufferBack);

    //pixelBufferFront.invalidate();
	//ofSetPixelStoreiAlignment(GL_PIXEL_UNPACK_BUFFER, width, 1, 3);
    pixelBufferFront.bind(GL_PIXEL_UNPACK_BUFFER);
    auto pixels = pixelBufferFront.map<unsigned char>(GL_READ_ONLY);
    if(pixels){
		std::ostringstream oss;
		oss << folderPath << ofToString(frame_, 5, '0') << "." << ofImageFormatExtension(imageFormat);
		channel.send(std::make_pair(oss.str(), pixels));
    }else{
        ofLogError(__FUNCTION__) << "Couldn't map buffer";
    }
    swap(pixelBufferBack,pixelBufferFront);
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
		std::pair<std::string, unsigned char *> data;
		switch(glType){
			case GL_UNSIGNED_BYTE:{
				while(channel.receive(data)){
					ofPixels pixels = getBuffer();
					pixels.setFromPixels(data.second, width, height, pixelFormat);
					channelReady.send(true);
					pixelsChannel.send(std::make_pair(data.first, std::move(pixels)));
				}
			}break;
			case GL_SHORT:
			case GL_UNSIGNED_SHORT:{
				while(channel.receive(data)){
					ofShortPixels pixels = getShortBuffer();
					pixels.setFromPixels((unsigned short*)data.second, width, height, pixelFormat);
					channelReady.send(true);
					shortPixelsChannel.send(std::make_pair(data.first, std::move(pixels)));
				}
			}break;
			case GL_FLOAT:{
				while(channel.receive(data)){
					ofFloatPixels pixels = getFloatBuffer();
					pixels.setFromPixels((float*)data.second, width, height, pixelFormat);
					channelReady.send(true);
					floatPixelsChannel.send(std::make_pair(data.first, std::move(pixels)));
				}
			}break;
			case GL_HALF_FLOAT:{
				while(channel.receive(data)){
					std::vector<half_float::half> halfdata = getHalfFloatBuffer();
					auto halfptr = (half_float::half*)data.second;
					halfdata.assign(halfptr, halfptr + size);
					channelReady.send(true);
					halffloatPixelsChannel.send(std::make_pair(data.first, std::move(halfdata)));
				}
			}break;
		}
	});

	if(glType==GL_HALF_FLOAT){
		for(size_t i=0;i<numThreads/2;i++){
			halfDecodingThreads.emplace_back(([&]{
				std::pair<std::string, std::vector<half_float::half>> halfdata;
				while(halffloatPixelsChannel.receive(halfdata)){
					ofFloatPixels pixels = getFloatBuffer();
					auto halfptr = halfdata.second.data();
					for(auto & p: pixels){
						p = *halfptr++;
					}
					returnHalfFloatPixelsChannel.send(std::move(halfdata.second));
					floatPixelsChannel.send(std::make_pair(halfdata.first, std::move(pixels)));
				}
			}));
		}
	}


	ofLogNotice(__FUNCTION__) << "Initializing with " << numThreads << " encoding threads";
	for(size_t i=0;i<numThreads;i++){
		encodeThreads.emplace_back([&]{
			switch(glType){
				case GL_UNSIGNED_BYTE:{
					std::pair<std::string, ofPixels> pixels;
					while(pixelsChannel.receive(pixels)){
						ofBuffer buffer;
						ofSaveImage(pixels.second, buffer, imageFormat, OF_IMAGE_QUALITY_BEST);
						returnPixelsChannel.send(std::move(pixels.second));
						encodedChannel.send(std::make_pair(pixels.first, std::move(buffer)));
					}
				}break;
				case GL_SHORT:
				case GL_UNSIGNED_SHORT:{
					std::pair<std::string, ofShortPixels> pixels;
					while(shortPixelsChannel.receive(pixels)){
						ofBuffer buffer;
						ofSaveImage(pixels.second, buffer, imageFormat, OF_IMAGE_QUALITY_BEST);
						returnShortPixelsChannel.send(std::move(pixels.second));
						encodedChannel.send(std::make_pair(pixels.first, std::move(buffer)));
					}
				}break;
				case GL_FLOAT:
				case GL_HALF_FLOAT:{
					std::pair<std::string, ofFloatPixels> pixels;
					ofBuffer buffer;
					while(floatPixelsChannel.receive(pixels)){
						buffer.clear();
						ofSaveImage(pixels.second, buffer, imageFormat, OF_IMAGE_QUALITY_BEST);
						returnFloatPixelsChannel.send(std::move(pixels.second));
						encodedChannel.send(std::make_pair(pixels.first, std::move(buffer)));
					}
				}break;
			}
		});
	}
	saveThread = std::thread([&]{
		std::pair<std::string, ofBuffer> encoded;
		while(encodedChannel.receive(encoded)){
			ofFile file(encoded.first, ofFile::WriteOnly);
			file.writeFromBuffer(encoded.second);
		}
	});
}

ofPixels ofxTextureRecorder::getBuffer(){
	ofPixels pixels;
	if(!returnPixelsChannel.tryReceive(pixels)){
		if(poolSize<encodeThreads.size()*2){
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
		if(shortPoolSize<encodeThreads.size()*2){
			pixels.allocate(width, height, pixelFormat);
			shortPoolSize += 1;
		}else{
			returnShortPixelsChannel.receive(pixels);
		}
	}
	return pixels;
};

ofFloatPixels ofxTextureRecorder::getFloatBuffer(){
	ofFloatPixels pixels;
	if(!returnFloatPixelsChannel.tryReceive(pixels)){
		if(floatPoolSize<encodeThreads.size()*2){
			pixels.allocate(width, height, pixelFormat);
			floatPoolSize += 1;
		}else{
			returnFloatPixelsChannel.receive(pixels);
		}
	}
	return pixels;
};

std::vector<half_float::half> ofxTextureRecorder::getHalfFloatBuffer(){
	std::vector<half_float::half> pixels;
	if(!returnHalfFloatPixelsChannel.tryReceive(pixels)){
		if(halfFloatPoolSize<encodeThreads.size()*2){
			pixels.resize(size);
			halfFloatPoolSize += 1;
		}else{
			returnHalfFloatPixelsChannel.receive(pixels);
		}
	}
	return pixels;
};

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
			cout << "rgba" << endl;
		break;
		case OF_IMAGE_GRAYSCALE:
			pixelFormat = OF_PIXELS_GRAY;
		break;
		default:
			ofLogError("ofxTextureRecorder") << "Unsupported texture format";
	}
	glType = ofGetGlTypeFromInternal(texData.glInternalFormat);

}

template<class T>
void ofxTextureRecorder::PixelsPool<T>::setup(size_t initialSize, size_t maxMemory, size_t memoryPerBuffer, std::function<T()> allocateBuffer){
	this->initialSize = initialSize;
	this->maxMemory = maxMemory;
	this->memoryPerBuffer = memoryPerBuffer;
	this->allocateBuffer = allocateBuffer;
}

template<class T>
T ofxTextureRecorder::PixelsPool<T>::getBuffer(){
	T pixels = this->allocateBuffer();
	if(!returnChannel.tryReceive(pixels)){
		if(poolSize * memoryPerBuffer < maxMemory){
			poolSize += 1;
			return allocateBuffer();
		}else{
			returnChannel.receive(pixels);
		}
	}
	return pixels;
}

template<class T>
void ofxTextureRecorder::PixelsPool<T>::returnBuffer(T&&buffer){
	returnChannel.send(buffer);
}
