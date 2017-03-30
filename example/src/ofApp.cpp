#include "ofApp.h"

//--------------------------------------------------------------
void ofApp::setup(){
	fbo.allocate(3600, 1080, GL_RGB16F);

	ofxTextureRecorder::Settings settings(fbo.getTexture());
	settings.imageFormat = OF_IMAGE_FORMAT_PNG;
	settings.numThreads = 12;
	recorder.setup(settings);
}

//--------------------------------------------------------------
void ofApp::update(){
	fbo.begin();
	ofClear(0,255);
	ofDrawCircle(ofGetFrameNum(), ofGetWidth()/2, 50);
	fbo.end();
	recorder.save(fbo.getTexture());
	if(ofGetFrameNum() > ofGetWidth() + 50){
		ofExit(0);
	}
}

//--------------------------------------------------------------
void ofApp::draw(){
	fbo.draw(0,0);

	if(ofGetFrameNum()%60==0){
		cout << ofGetFrameRate() << endl;
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
