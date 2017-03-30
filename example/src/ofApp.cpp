#include "ofApp.h"

//--------------------------------------------------------------
void ofApp::setup(){
	fbo.allocate(ofGetWidth(), ofGetHeight(), GL_RGB);

	ofxTextureRecorder::Settings settings(fbo.getTexture());
	settings.imageFormat = OF_IMAGE_FORMAT_JPEG;
	settings.numThreads = 12;
	settings.maxMemoryUsage = 9000000000;
	recorder.setup(settings);
}

//--------------------------------------------------------------
void ofApp::update(){
	fbo.begin();
	ofClear(0,255);
	ofDrawCircle(ofGetFrameNum(), ofGetWidth()/2, 50);
	fbo.end();
	if(ofGetFrameNum()>0){
		recorder.save(fbo.getTexture());
	}
	if(ofGetFrameNum() > ofGetWidth() + 50){
		ofExit(0);
	}
}

//--------------------------------------------------------------
void ofApp::draw(){
	fbo.draw(0,0);

	if(ofGetFrameNum()%60==0){
		cout << ofGetFrameRate() << endl;
		cout << "texture copy: " << recorder.getAvgTimeTextureCopy() << endl;
		cout << "gpu download: " << recorder.getAvgTimeGpuDownload() << endl;
		cout << "image encoding: " << recorder.getAvgTimeEncode() << endl;
		cout << "file save: " << recorder.getAvgTimeSave() << endl;
	}

}

//--------------------------------------------------------------
void ofApp::keyPressed(int key){

}

//--------------------------------------------------------------
void ofApp::keyReleased(int key){

}

//--------------------------------------------------------------
void ofApp::mouseMoved(int x, int y ){

}

//--------------------------------------------------------------
void ofApp::mouseDragged(int x, int y, int button){

}

//--------------------------------------------------------------
void ofApp::mousePressed(int x, int y, int button){

}

//--------------------------------------------------------------
void ofApp::mouseReleased(int x, int y, int button){

}

//--------------------------------------------------------------
void ofApp::mouseEntered(int x, int y){

}

//--------------------------------------------------------------
void ofApp::mouseExited(int x, int y){

}

//--------------------------------------------------------------
void ofApp::windowResized(int w, int h){

}

//--------------------------------------------------------------
void ofApp::gotMessage(ofMessage msg){

}

//--------------------------------------------------------------
void ofApp::dragEvent(ofDragInfo dragInfo){ 

}
