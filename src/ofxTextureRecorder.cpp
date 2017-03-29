/*
 * ofxTextureRecorder.cpp
 *
 *  Created on: Oct 14, 2014
 *      Author: arturo
 */

#include "ofxTextureRecorder.h"
#include "half.hpp"

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

	while(!pixelsChannel.empty()){
		ofSleepMillis(100);
	}
	pixelsChannel.close();
	shortPixelsChannel.close();
	floatPixelsChannel.close();

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
	downloadThread = std::thread([&]{
		std::pair<std::string, unsigned char *> data;
		switch(glType){
			case GL_UNSIGNED_BYTE:{
				ofPixels pixels;
				pixels.allocate(width, height, pixelFormat);
				while(channel.receive(data)){
					pixels.setFromPixels(data.second, width, height, pixelFormat);
					channelReady.send(true);
					pixelsChannel.send(std::make_pair(data.first, std::move(pixels)));
				}
			}break;
			case GL_SHORT:
			case GL_UNSIGNED_SHORT:{
				ofShortPixels pixels;
				pixels.allocate(width, height, pixelFormat);
				while(channel.receive(data)){
					pixels.setFromPixels((unsigned short*)data.second, width, height, pixelFormat);
					channelReady.send(true);
					shortPixelsChannel.send(std::make_pair(data.first, std::move(pixels)));
				}
			}break;
			case GL_FLOAT:
			case GL_HALF_FLOAT:{
				ofFloatPixels pixels;
				pixels.allocate(width, height, pixelFormat);
				while(channel.receive(data)){
					if(glType == GL_FLOAT){
						pixels.setFromPixels((float*)data.second, width, height, pixelFormat);
					}else{
						auto halfdata = (half_float::half*)data.second;
						for(auto & p: pixels){
							p = *halfdata++;
						}
					}
					channelReady.send(true);
					floatPixelsChannel.send(std::make_pair(data.first, pixels));
				}
			}break;
		}
	});


	ofLogNotice(__FUNCTION__) << "Initializing with " << numThreads << " encoding threads";
	for(size_t i=0;i<numThreads;i++){
		encodeThreads.emplace_back([&]{
			switch(glType){
				case GL_UNSIGNED_BYTE:{
					std::pair<std::string, ofPixels> pixels;
					while(pixelsChannel.receive(pixels)){
						ofBuffer buffer;
						ofSaveImage(pixels.second, buffer, imageFormat, OF_IMAGE_QUALITY_BEST);
						encodedChannel.send(std::make_pair(pixels.first, std::move(buffer)));
					}
				}break;
				case GL_SHORT:
				case GL_UNSIGNED_SHORT:{
					std::pair<std::string, ofShortPixels> pixels;
					while(shortPixelsChannel.receive(pixels)){
						ofBuffer buffer;
						ofSaveImage(pixels.second, buffer, imageFormat, OF_IMAGE_QUALITY_BEST);
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
