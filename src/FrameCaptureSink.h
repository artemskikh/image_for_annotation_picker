#ifndef FRAMECAPTURESINK_H
#define FRAMECAPTURESINK_H

#include <QVideoSink>
#include <QVideoFrame>
#include <QObject>

/**
 * Custom QVideoSink implementation for capturing video frames
 * from the media player for saving to disk.
 */
class FrameCaptureSink : public QVideoSink
{
    Q_OBJECT

public:
    explicit FrameCaptureSink(QObject *parent = nullptr);

    /**
     * Get the most recently received video frame
     * @return The current QVideoFrame, may be invalid if no frame received yet
     */
    QVideoFrame getCurrentFrame() const { return m_currentFrame; }

public slots:
    /**
     * Slot called when a new video frame is available
     * @param frame The new video frame
     */
    void onFrameChanged(const QVideoFrame &frame);

signals:
    /**
     * Signal emitted when a new frame becomes available
     */
    void frameAvailable();

private:
    QVideoFrame m_currentFrame;
};

#endif // FRAMECAPTURESINK_H
