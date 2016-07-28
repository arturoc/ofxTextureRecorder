/*
 * ofxTextureRecorder.cpp
 *
 *  Created on: Oct 14, 2014
 *      Author: arturo
 */

#include "ofxTextureRecorder.h"

ofxTextureRecorder::ofxTextureRecorder()
:firstFrame(true){
    downloadThread = std::thread([&]{
        unsigned char * p;
        while(channel.receive(p)){
            ofPixels pixels;
            pixels.setFromPixels(p,width,height,pixelFormat);
            channelReady.send(true);
            char buf[10];
            sprintf(buf, "%05d.%s", frame++, ofImageFormatExtension(imageFormat).c_str());
            std::string name(buf);
            pixelsChannel.send(std::move(std::make_pair(name, std::move(pixels))));
        }
    });
    auto numThreads = std::max(1u,std::thread::hardware_concurrency() - 2);
    ofLogNotice() << "Initializing with " << numThreads << " encoding threads " << endl;
    for(size_t i=0;i<numThreads;i++){
        encodeThreads.emplace_back([&]{
            std::pair<std::string, ofPixels> pixels;
            while(pixelsChannel.receive(pixels)){
                ofBuffer buffer;
                ofSaveImage(pixels.second, buffer, imageFormat, OF_IMAGE_QUALITY_BEST);
                encodedChannel.send(std::move(std::make_pair(pixels.first, std::move(buffer))));
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

ofxTextureRecorder::~ofxTextureRecorder(){
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
    downloadThread.join();
    saveThread.join();
}

void ofxTextureRecorder::setup(int w, int h, ofPixelFormat pixelFormat_, ofImageFormat imageFormat_){
    width = w;
    height = h;
    pixelFormat = pixelFormat_;
    imageFormat = imageFormat_;
    firstFrame = true;
    pixelBufferBack.allocate(ofPixels::bytesFromPixelFormat(w,h,pixelFormat), GL_DYNAMIC_READ);
    pixelBufferFront.allocate(ofPixels::bytesFromPixelFormat(w,h,pixelFormat), GL_DYNAMIC_READ);
}

void ofxTextureRecorder::save(const ofTexture & tex){
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
        channel.send(pixels);
    }else{
        ofLogError("ImageSaverThread") << "Couldn't map buffer";
    }
    swap(pixelBufferBack,pixelBufferFront);
}
