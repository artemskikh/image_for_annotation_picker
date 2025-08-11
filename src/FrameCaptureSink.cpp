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

    // Reduce logging frequency to avoid UI overhead during video playback
    static qint64 lastLogTime = 0;
    static int frameCount = 0;
    frameCount++;

    qint64 currentTime = QDateTime::currentMSecsSinceEpoch();

    // Only log every 2 seconds or every 60 frames, whichever comes first
    if (frame.isValid() && (currentTime - lastLogTime > 2000 || frameCount >= 60))
    {
        LOG_DEBUG("Frame received: {}x{}, format: {} (processed {} frames)",
                  frame.size().width(), frame.size().height(),
                  (int)frame.pixelFormat(), frameCount);
        lastLogTime = currentTime;
        frameCount = 0;
    }

    // Emit signal to notify that a new frame is available
    emit frameAvailable();
}
