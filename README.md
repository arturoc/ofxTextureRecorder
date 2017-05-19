# ofxTextureRecorder

[![Build status linux / osx](https://travis-ci.org/arturoc/ofxTextureRecorder.svg?branch=master)](https://travis-ci.org/arturoc/ofxTextureRecorder.svg?branch=master)
[![Build status windows](https://ci.appveyor.com/api/projects/status/7p3nnfjb6d1xlnni/branch/master?svg=true)](https://ci.appveyor.com/project/arturoc/ofxtexturerecorder/branch/master)

Fast recording of textures to disk. You can draw to an fbo to record the full screen.

It uses a PBO to download from the graphics card without blocking the main thread and several threads for encoding as fast as possible to the chosen format.

The number of threads is the hardware concurrency value (usually number of cores * 2) - 2 so there's at 2 cores free for other tasks.
