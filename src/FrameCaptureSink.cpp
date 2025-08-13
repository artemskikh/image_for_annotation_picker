#include "FrameCaptureSink.h"
#include "Logger.h"
#include <QDateTime>

FrameCaptureSink::FrameCaptureSink(QObject *parent)
    : QVideoSink(parent)
{
    // Connect to our own videoFrameChanged signal to capture frames
    connect(this, &QVideoSink::videoFrameChanged, this, &FrameCaptureSink::onFrameChanged);
}

void FrameCaptureSink::onFrameChanged(const QVideoFrame &frame)
{
    // Store the current frame for later retrieval
    m_currentFrame = frame;

    // NOTE: Disabled all logging in frame capture to eliminate potential UI overhead
    // This method is called 30-60 times per second during video playback

    // NOTE: Commenting out unused signal emission that was causing UI hangups
    // This was being emitted 30-60 times per second during playback with no benefit
    // emit frameAvailable();
}
