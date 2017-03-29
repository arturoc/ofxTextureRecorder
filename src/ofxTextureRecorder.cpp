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

void ofxTextureRecorder::setup(const Settings & settings){
	createThreads(settings.numThreads);
	width = settings.w;
	height = settings.h;
	pixelFormat = settings.pixelFormat;
	imageFormat = settings.imageFormat;
	folderPath = settings.folderPath;

	if (!folderPath.empty()) folderPath = ofFilePath::addTrailingSlash(folderPath);

	frame = 0;
    firstFrame = true;
	pixelBufferBack.allocate(ofPixels::bytesFromPixelFormat(width,height,pixelFormat), GL_DYNAMIC_READ);
	pixelBufferFront.allocate(ofPixels::bytesFromPixelFormat(width,height,pixelFormat), GL_DYNAMIC_READ);
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
	if(!encodeThreads.empty()){
		stopThreads();
	}

	downloadThread = std::thread([&]{
		std::pair<std::string, unsigned char *> data;
		while(channel.receive(data)){
			ofPixels pixels;
			pixels.setFromPixels(data.second, width, height, pixelFormat);
			channelReady.send(true);
			pixelsChannel.send(std::make_pair(data.first, std::move(pixels)));
		}
	});
	ofLogNotice(__FUNCTION__) << "Initializing with " << numThreads << " encoding threads";
	for(size_t i=0;i<numThreads;i++){
		encodeThreads.emplace_back([&]{
			std::pair<std::string, ofPixels> pixels;
			while(pixelsChannel.receive(pixels)){
				ofBuffer buffer;
				ofSaveImage(pixels.second, buffer, imageFormat, OF_IMAGE_QUALITY_BEST);
				encodedChannel.send(std::make_pair(pixels.first, std::move(buffer)));
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
