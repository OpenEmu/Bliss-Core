
#include <stdio.h>
#include <string.h>
#include "VideoBus.h"

VideoBus::VideoBus()
  : pixelBuffer(NULL),
    pixelBufferSize(0),
    pixelBufferRowSize(0),
    pixelBufferWidth(0),
    pixelBufferHeight(0),
    videoProducerCount(0)
{
}

VideoBus::~VideoBus()
{
    if (pixelBuffer) {
        delete[] pixelBuffer;
    }

    for (UINT32 i = 0; i < videoProducerCount; i++)
        videoProducers[i]->setPixelBuffer(NULL, 0);
}

void VideoBus::addVideoProducer(VideoProducer* p)
{
    videoProducers[videoProducerCount] = p;
    videoProducers[videoProducerCount]->setPixelBuffer(pixelBuffer, pixelBufferRowSize);
    videoProducerCount++;
}

void VideoBus::removeVideoProducer(VideoProducer* p)
{
    for (UINT32 i = 0; i < videoProducerCount; i++) {
        if (videoProducers[i] == p) {
			videoProducers[i]->setPixelBuffer(NULL, 0);

            for (UINT32 j = i; j < (videoProducerCount-1); j++)
                videoProducers[j] = videoProducers[j+1];
            videoProducerCount--;
            return;
        }
    }
}

void VideoBus::removeAll()
{
    while (videoProducerCount)
        removeVideoProducer(videoProducers[0]);
}

void VideoBus::init(UINT32 width, UINT32 height)
{
	VideoBus::release();

	pixelBufferWidth = width;
	pixelBufferHeight = height;
	pixelBufferRowSize = width * sizeof(UINT32);
	pixelBufferSize = width * height * sizeof(UINT32);
	pixelBuffer = new UINT32[width * height];

	if ( pixelBuffer ) {
		memset(pixelBuffer, 0, pixelBufferSize);
	}

	for (UINT32 i = 0; i < videoProducerCount; i++)
		videoProducers[i]->setPixelBuffer(pixelBuffer, pixelBufferRowSize);
}

void VideoBus::render()
{
    //tell each of the video producers that they can now output their
    //video contents onto the video device
    for (UINT32 i = 0; i < videoProducerCount; i++)
        videoProducers[i]->render();
}

void VideoBus::release()
{
	if (pixelBuffer) {
		for (UINT32 i = 0; i < videoProducerCount; i++)
			videoProducers[i]->setPixelBuffer(NULL, 0);

		pixelBufferWidth = 0;
		pixelBufferHeight = 0;
		pixelBufferRowSize = 0;
		pixelBufferSize = 0;
		delete[] pixelBuffer;
		pixelBuffer = NULL;
	}
}
