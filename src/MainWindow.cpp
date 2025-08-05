#include "MainWindow.h"
#include <QApplication>
#include <QDir>
#include <QStandardPaths>
#include <QDateTime>
#include <QPixmap>
#include <QVideoFrame>
#include <QVideoSink>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), m_centralWidget(nullptr), m_mainSplitter(nullptr), m_videoWidget(nullptr), m_videoDisplay(nullptr), m_mediaPlayer(nullptr), m_controlsWidget(nullptr), m_playPauseBtn(nullptr), m_previousFrameBtn(nullptr), m_nextFrameBtn(nullptr), m_saveFrameBtn(nullptr), m_positionSlider(nullptr), m_timeLabel(nullptr), m_durationLabel(nullptr), m_frameListWidget(nullptr), m_frameList(nullptr), m_removeFrameBtn(nullptr), m_exportFramesBtn(nullptr), m_clearFramesBtn(nullptr), m_frameCountLabel(nullptr), m_settingsGroup(nullptr), m_outputDirEdit(nullptr), m_browseDirBtn(nullptr), m_imageFormatCombo(nullptr), m_openVideoAction(nullptr), m_exitAction(nullptr), m_aboutAction(nullptr), m_progressBar(nullptr), m_frameStepTimer(nullptr), m_isSteppingForward(false), m_isSteppingBackward(false), m_stepInterval(100), m_videoDuration(0), m_isPlaying(false)
{
    setupUI();
    setupMenuBar();
    connectSignals();
    updateControls();

    // Setup frame stepping timer
    m_frameStepTimer = new QTimer(this);
    m_frameStepTimer->setSingleShot(false);
    connect(m_frameStepTimer, &QTimer::timeout, this, &MainWindow::onFrameStepTimer);

    // Set default output directory
    m_outputDirectory = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/AnnotationFrames";
    m_outputDirEdit->setText(m_outputDirectory);

    // Create output directory if it doesn't exist
    QDir().mkpath(m_outputDirectory);

    setWindowTitle("Image Annotation Picker");
    resize(1200, 800);

    // Show keyboard shortcuts in status bar initially
    statusBar()->showMessage("Keyboard shortcuts: ← → (frame navigation), Space (play/pause), Ctrl+S (save frame)", 5000);
}

MainWindow::~MainWindow()
{
    if (m_mediaPlayer)
    {
        m_mediaPlayer->stop();
    }
}

void MainWindow::setupUI()
{
    m_centralWidget = new QWidget;
    setCentralWidget(m_centralWidget);

    // Create main splitter
    m_mainSplitter = new QSplitter(Qt::Horizontal);

    // Setup video section
    m_videoWidget = new QWidget;
    QVBoxLayout *videoLayout = new QVBoxLayout(m_videoWidget);

    m_videoDisplay = new QVideoWidget;
    m_videoDisplay->setMinimumSize(640, 480);
    m_videoDisplay->setFocusPolicy(Qt::NoFocus); // Prevent stealing keyboard focus
    m_mediaPlayer = new QMediaPlayer;
    m_mediaPlayer->setVideoOutput(m_videoDisplay);

    videoLayout->addWidget(m_videoDisplay);

    // Setup controls
    m_controlsWidget = new QWidget;
    QVBoxLayout *controlsLayout = new QVBoxLayout(m_controlsWidget);

    // Playback controls
    QHBoxLayout *playbackLayout = new QHBoxLayout;
    m_playPauseBtn = new QPushButton("Play");
    m_previousFrameBtn = new QPushButton("Previous Frame");
    m_nextFrameBtn = new QPushButton("Next Frame");
    m_saveFrameBtn = new QPushButton("Save Current Frame");

    playbackLayout->addWidget(m_playPauseBtn);
    playbackLayout->addWidget(m_previousFrameBtn);
    playbackLayout->addWidget(m_nextFrameBtn);
    playbackLayout->addWidget(m_saveFrameBtn);
    playbackLayout->addStretch();

    // Position slider and time labels
    QHBoxLayout *positionLayout = new QHBoxLayout;
    m_timeLabel = new QLabel("00:00");
    m_positionSlider = new QSlider(Qt::Horizontal);
    m_durationLabel = new QLabel("00:00");

    positionLayout->addWidget(m_timeLabel);
    positionLayout->addWidget(m_positionSlider);
    positionLayout->addWidget(m_durationLabel);

    controlsLayout->addLayout(playbackLayout);
    controlsLayout->addLayout(positionLayout);

    // Add video and controls to video section
    QVBoxLayout *leftLayout = new QVBoxLayout;
    QWidget *leftWidget = new QWidget;
    leftWidget->setLayout(leftLayout);
    leftLayout->addWidget(m_videoWidget);
    leftLayout->addWidget(m_controlsWidget);

    // Setup frame list section
    m_frameListWidget = new QWidget;
    m_frameListWidget->setMinimumWidth(300);
    QVBoxLayout *frameLayout = new QVBoxLayout(m_frameListWidget);

    QLabel *frameListTitle = new QLabel("Selected Frames");
    frameListTitle->setStyleSheet("font-weight: bold; font-size: 14px;");
    m_frameList = new QListWidget;
    m_frameCountLabel = new QLabel("Frames: 0");

    // Frame list controls
    QHBoxLayout *frameControlsLayout = new QHBoxLayout;
    m_removeFrameBtn = new QPushButton("Remove");
    m_exportFramesBtn = new QPushButton("Export All");
    m_clearFramesBtn = new QPushButton("Clear All");

    frameControlsLayout->addWidget(m_removeFrameBtn);
    frameControlsLayout->addWidget(m_exportFramesBtn);
    frameControlsLayout->addWidget(m_clearFramesBtn);

    // Settings group
    m_settingsGroup = new QGroupBox("Export Settings");
    QVBoxLayout *settingsLayout = new QVBoxLayout(m_settingsGroup);

    QHBoxLayout *outputDirLayout = new QHBoxLayout;
    QLabel *outputDirLabel = new QLabel("Output Directory:");
    m_outputDirEdit = new QLineEdit;
    m_outputDirEdit->setReadOnly(true); // Make read-only
    m_browseDirBtn = new QPushButton("Browse");

    outputDirLayout->addWidget(outputDirLabel);
    outputDirLayout->addWidget(m_outputDirEdit);
    outputDirLayout->addWidget(m_browseDirBtn);

    QHBoxLayout *formatLayout = new QHBoxLayout;
    QLabel *formatLabel = new QLabel("Image Format:");
    m_imageFormatCombo = new QComboBox;
    m_imageFormatCombo->addItems({"PNG", "JPEG", "BMP", "TIFF"});

    formatLayout->addWidget(formatLabel);
    formatLayout->addWidget(m_imageFormatCombo);
    formatLayout->addStretch();

    settingsLayout->addLayout(outputDirLayout);
    settingsLayout->addLayout(formatLayout);

    frameLayout->addWidget(frameListTitle);
    frameLayout->addWidget(m_frameList);
    frameLayout->addWidget(m_frameCountLabel);
    frameLayout->addLayout(frameControlsLayout);
    frameLayout->addWidget(m_settingsGroup);

    // Add widgets to splitter
    m_mainSplitter->addWidget(leftWidget);
    m_mainSplitter->addWidget(m_frameListWidget);
    m_mainSplitter->setSizes({800, 400});

    // Main layout
    QVBoxLayout *mainLayout = new QVBoxLayout(m_centralWidget);
    mainLayout->addWidget(m_mainSplitter);

    // Make sure the main window can receive keyboard events
    setFocusPolicy(Qt::StrongFocus);
    setFocus(); // Ensure initial focus

    // Prevent other widgets from stealing focus
    m_positionSlider->setFocusPolicy(Qt::NoFocus);
    m_frameList->setFocusPolicy(Qt::NoFocus);

    // Status bar
    m_progressBar = new QProgressBar;
    m_progressBar->setVisible(false);
    statusBar()->addPermanentWidget(m_progressBar);
}

void MainWindow::setupMenuBar()
{
    // File menu
    QMenu *fileMenu = menuBar()->addMenu("&File");

    m_openVideoAction = new QAction("&Open Video...", this);
    m_openVideoAction->setShortcut(QKeySequence::Open);
    fileMenu->addAction(m_openVideoAction);

    fileMenu->addSeparator();

    m_exitAction = new QAction("E&xit", this);
    m_exitAction->setShortcut(QKeySequence::Quit);
    fileMenu->addAction(m_exitAction);

    // Help menu
    QMenu *helpMenu = menuBar()->addMenu("&Help");

    m_keyboardShortcutsAction = new QAction("&Keyboard Shortcuts", this);
    helpMenu->addAction(m_keyboardShortcutsAction);

    m_logLevelAction = new QAction("&Log Level...", this);
    helpMenu->addAction(m_logLevelAction);

    helpMenu->addSeparator();

    m_aboutAction = new QAction("&About", this);
    helpMenu->addAction(m_aboutAction);
}

void MainWindow::connectSignals()
{
    // Menu actions
    connect(m_openVideoAction, &QAction::triggered, this, &MainWindow::openVideo);
    connect(m_exitAction, &QAction::triggered, this, &QWidget::close);
    connect(m_aboutAction, &QAction::triggered, [this]()
            { QMessageBox::about(this, "About",
                                 "Image Annotation Picker v1.0\n\n"
                                 "A tool to help go through video frame by frame\n"
                                 "and pick & choose what images to save for datasets.\n\n"
                                 "Built with Qt " QT_VERSION_STR "\n"
                                 "Qt is licensed under LGPL v3\n"
                                 "© The Qt Company Ltd.\n\n"
                                 "This application is licensed under MIT License\n"
                                 "See LICENSE file for details."); });

    connect(m_keyboardShortcutsAction, &QAction::triggered, [this]()
            { QMessageBox::information(this, "Keyboard Shortcuts",
                                       "Available keyboard shortcuts:\n\n"
                                       "← → (Left/Right arrows): Navigate frames\n"
                                       "  • Single press: Move one frame\n"
                                       "  • Hold: Accelerated frame stepping\n\n"
                                       "Space: Play/Pause video\n"
                                       "Ctrl+S: Save current frame\n\n"
                                       "Note: Click on the main window area to ensure\n"
                                       "keyboard focus is on the video player."); });

    connect(m_logLevelAction, &QAction::triggered, [this]()
            {
        QStringList levels = {"Trace", "Debug", "Info", "Warning", "Error", "Critical"};
        bool ok;
        QString level = QInputDialog::getItem(this, "Log Level",
                                            "Select logging level:", levels, 2, false, &ok);
        if (ok) {
            if (level == "Trace") Logger::setLevel(spdlog::level::trace);
            else if (level == "Debug") Logger::setLevel(spdlog::level::debug);
            else if (level == "Info") Logger::setLevel(spdlog::level::info);
            else if (level == "Warning") Logger::setLevel(spdlog::level::warn);
            else if (level == "Error") Logger::setLevel(spdlog::level::err);
            else if (level == "Critical") Logger::setLevel(spdlog::level::critical);
            LOG_INFO("Log level changed to: {}", level.toStdString());
        } });

    // Control buttons
    connect(m_playPauseBtn, &QPushButton::clicked, this, &MainWindow::playPause);
    connect(m_previousFrameBtn, &QPushButton::clicked, this, &MainWindow::previousFrame);
    connect(m_nextFrameBtn, &QPushButton::clicked, this, &MainWindow::nextFrame);
    connect(m_saveFrameBtn, &QPushButton::clicked, this, &MainWindow::saveCurrentFrame);

    // Slider
    connect(m_positionSlider, &QSlider::valueChanged, this, &MainWindow::seekToPosition);

    // Media player
    connect(m_mediaPlayer, &QMediaPlayer::positionChanged, this, &MainWindow::onPositionChanged);
    connect(m_mediaPlayer, &QMediaPlayer::durationChanged, this, &MainWindow::onDurationChanged);
    connect(m_mediaPlayer, &QMediaPlayer::mediaStatusChanged, this, &MainWindow::onMediaStatusChanged);

    // Frame list controls
    connect(m_removeFrameBtn, &QPushButton::clicked, this, &MainWindow::removeSelectedFrame);
    connect(m_exportFramesBtn, &QPushButton::clicked, this, &MainWindow::exportSelectedFrames);
    connect(m_clearFramesBtn, &QPushButton::clicked, this, &MainWindow::clearSelectedFrames);

    // Settings
    connect(m_browseDirBtn, &QPushButton::clicked, [this]()
            {
        QString dir = QFileDialog::getExistingDirectory(this, "Select Output Directory", m_outputDirectory);
        if (!dir.isEmpty()) {
            m_outputDirectory = dir;
            m_outputDirEdit->setText(dir);
        } });
}

void MainWindow::updateControls()
{
    bool hasVideo = !m_currentVideoPath.isEmpty();
    bool canSeek = hasVideo && m_videoDuration > 0;

    m_playPauseBtn->setEnabled(hasVideo);
    m_previousFrameBtn->setEnabled(canSeek);
    m_nextFrameBtn->setEnabled(canSeek);
    m_saveFrameBtn->setEnabled(hasVideo);
    m_positionSlider->setEnabled(canSeek);

    m_removeFrameBtn->setEnabled(m_frameList->currentRow() >= 0);
    m_exportFramesBtn->setEnabled(m_frameList->count() > 0);
    m_clearFramesBtn->setEnabled(m_frameList->count() > 0);
}

void MainWindow::openVideo()
{
    QString fileName = QFileDialog::getOpenFileName(this,
                                                    "Open Video File", QString(),
                                                    "Video Files (*.mp4 *.avi *.mov *.mkv *.wmv *.flv *.webm)");

    if (!fileName.isEmpty())
    {
        LOG_INFO("Opening video file: {}", fileName.toStdString());
        m_currentVideoPath = fileName;
        m_mediaPlayer->setSource(QUrl::fromLocalFile(fileName));
        statusBar()->showMessage("Loaded: " + QFileInfo(fileName).fileName(), 3000);
        updateControls();
    }
    else
    {
        LOG_DEBUG("Video file selection cancelled");
    }
}

void MainWindow::saveCurrentFrame()
{
    if (m_currentVideoPath.isEmpty())
    {
        QMessageBox::warning(this, "Warning", "No video loaded.");
        return;
    }

    // For now, we'll add a placeholder entry to the list
    // In a real implementation, you'd capture the actual frame from the video
    qint64 currentPos = m_mediaPlayer->position();
    QString timestamp = formatTime(currentPos);
    QString frameName = QString("Frame_%1_%2.%3")
                            .arg(QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss"))
                            .arg(currentPos)
                            .arg(m_imageFormatCombo->currentText().toLower());

    addFrameToList(frameName, currentPos);
    statusBar()->showMessage("Frame saved: " + timestamp, 2000);
}

void MainWindow::playPause()
{
    LOG_DEBUG("playPause() called - current state: {}", m_isPlaying ? "playing" : "paused");

    if (m_isPlaying)
    {
        m_mediaPlayer->pause();
        m_playPauseBtn->setText("Play");
        m_isPlaying = false;
        LOG_INFO("Video paused");
    }
    else
    {
        m_mediaPlayer->play();
        m_playPauseBtn->setText("Pause");
        m_isPlaying = true;
        LOG_INFO("Video playing");
    }
}

void MainWindow::nextFrame()
{
    // Jump forward by approximately one frame (assuming 30 fps)
    qint64 currentPos = m_mediaPlayer->position();
    qint64 frameTime = 1000 / 30; // ~33ms per frame for 30fps
    qint64 newPos = qMin(currentPos + frameTime, m_videoDuration);

    LOG_DEBUG("nextFrame() - current: {}ms, frameTime: {}ms, new: {}ms", currentPos, frameTime, newPos);

    m_mediaPlayer->setPosition(newPos);
}

void MainWindow::previousFrame()
{
    // Jump backward by approximately one frame
    qint64 currentPos = m_mediaPlayer->position();
    qint64 frameTime = 1000 / 30;
    qint64 newPos = qMax(currentPos - frameTime, 0LL);

    LOG_DEBUG("previousFrame() - current: {}ms, frameTime: {}ms, new: {}ms", currentPos, frameTime, newPos);

    m_mediaPlayer->setPosition(newPos);
}

void MainWindow::seekToPosition(int position)
{
    LOG_DEBUG("seekToPosition called with position: {}ms", position);
    m_mediaPlayer->setPosition(position);

    // Ensure main window gets focus back after slider interaction
    setFocus();
}

void MainWindow::onPositionChanged(qint64 position)
{
    if (!m_positionSlider->isSliderDown())
    {
        m_positionSlider->setValue(static_cast<int>(position));
    }
    m_timeLabel->setText(formatTime(position));

    // Only log position changes occasionally to avoid spam
    static qint64 lastLoggedPosition = -1;
    if (abs(position - lastLoggedPosition) > 1000)
    { // Log every second
        LOG_TRACE("Position changed to: {}ms ({})", position, formatTime(position).toStdString());
        lastLoggedPosition = position;
    }
}

void MainWindow::onDurationChanged(qint64 duration)
{
    LOG_INFO("Video duration: {}ms ({})", duration, formatTime(duration).toStdString());
    m_videoDuration = duration;
    m_positionSlider->setRange(0, static_cast<int>(duration));
    m_durationLabel->setText(formatTime(duration));
    updateControls();
}

void MainWindow::onMediaStatusChanged(QMediaPlayer::MediaStatus status)
{
    QString statusStr;
    switch (status)
    {
    case QMediaPlayer::NoMedia:
        statusStr = "NoMedia";
        break;
    case QMediaPlayer::LoadingMedia:
        statusStr = "LoadingMedia";
        break;
    case QMediaPlayer::LoadedMedia:
        statusStr = "LoadedMedia";
        break;
    case QMediaPlayer::StalledMedia:
        statusStr = "StalledMedia";
        break;
    case QMediaPlayer::BufferingMedia:
        statusStr = "BufferingMedia";
        break;
    case QMediaPlayer::BufferedMedia:
        statusStr = "BufferedMedia";
        break;
    case QMediaPlayer::EndOfMedia:
        statusStr = "EndOfMedia";
        break;
    case QMediaPlayer::InvalidMedia:
        statusStr = "InvalidMedia";
        break;
    default:
        statusStr = "Unknown";
        break;
    }

    LOG_INFO("Media status changed to: {}", statusStr.toStdString());

    switch (status)
    {
    case QMediaPlayer::LoadedMedia:
        statusBar()->showMessage("Video loaded successfully", 2000);
        // Ensure main window has focus for keyboard events
        setFocus();
        break;
    case QMediaPlayer::InvalidMedia:
        statusBar()->showMessage("Invalid media file", 3000);
        QMessageBox::warning(this, "Error", "Could not load the video file.");
        LOG_ERROR("Failed to load video file - invalid media");
        break;
    default:
        break;
    }
}

void MainWindow::removeSelectedFrame()
{
    int currentRow = m_frameList->currentRow();
    if (currentRow >= 0)
    {
        delete m_frameList->takeItem(currentRow);
        m_frameCountLabel->setText(QString("Frames: %1").arg(m_frameList->count()));
        updateControls();
    }
}

void MainWindow::exportSelectedFrames()
{
    if (m_frameList->count() == 0)
    {
        QMessageBox::information(this, "Information", "No frames to export.");
        return;
    }

    // Create output directory if it doesn't exist
    QDir outputDir(m_outputDirectory);
    if (!outputDir.exists())
    {
        outputDir.mkpath(".");
    }

    // In a real implementation, you would export the actual captured frames
    QMessageBox::information(this, "Export",
                             QString("Would export %1 frames to:\n%2")
                                 .arg(m_frameList->count())
                                 .arg(m_outputDirectory));
}

void MainWindow::clearSelectedFrames()
{
    if (m_frameList->count() > 0)
    {
        QMessageBox::StandardButton reply = QMessageBox::question(this,
                                                                  "Clear Frames", "Are you sure you want to clear all selected frames?",
                                                                  QMessageBox::Yes | QMessageBox::No);

        if (reply == QMessageBox::Yes)
        {
            m_frameList->clear();
            m_frameCountLabel->setText("Frames: 0");
            updateControls();
        }
    }
}

void MainWindow::addFrameToList(const QString &framePath, qint64 timestamp)
{
    QString displayText = QString("%1 - %2")
                              .arg(formatTime(timestamp))
                              .arg(QFileInfo(framePath).fileName());

    QListWidgetItem *item = new QListWidgetItem(displayText);
    item->setData(Qt::UserRole, framePath);
    item->setData(Qt::UserRole + 1, timestamp);

    m_frameList->addItem(item);
    m_frameCountLabel->setText(QString("Frames: %1").arg(m_frameList->count()));
    updateControls();
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    LOG_DEBUG("Key press event: key={}, modifiers={}, repeat={}", event->key(), event->modifiers(), event->isAutoRepeat());

    if (!m_currentVideoPath.isEmpty() && m_videoDuration > 0)
    {
        switch (event->key())
        {
        case Qt::Key_Left:
            LOG_DEBUG("Left arrow key pressed - stepping backward: {}, stepping forward: {}, timer active: {}, playing: {}",
                      m_isSteppingBackward, m_isSteppingForward, m_frameStepTimer->isActive(), m_isPlaying);

            // Stop any ongoing forward stepping first
            if (m_isSteppingForward)
            {
                LOG_DEBUG("Stopping forward stepping before starting backward");
                m_frameStepTimer->stop();
                m_isSteppingForward = false;
            }

            if (!m_isSteppingBackward && !m_frameStepTimer->isActive())
            {
                // If video is playing, pause it first
                if (m_isPlaying)
                {
                    LOG_INFO("Pausing video for frame stepping");
                    m_mediaPlayer->pause();
                    m_playPauseBtn->setText("Play");
                    m_isPlaying = false;
                }

                previousFrame();
                m_isSteppingBackward = true;
                m_stepInterval = 100;         // Start with 100ms interval
                m_frameStepTimer->start(300); // Initial delay before rapid stepping
                LOG_DEBUG("Started backward frame stepping");
            }
            event->accept();
            return;

        case Qt::Key_Right:
            LOG_DEBUG("Right arrow key pressed - stepping forward: {}, stepping backward: {}, timer active: {}, playing: {}",
                      m_isSteppingForward, m_isSteppingBackward, m_frameStepTimer->isActive(), m_isPlaying);

            // Stop any ongoing backward stepping first
            if (m_isSteppingBackward)
            {
                LOG_DEBUG("Stopping backward stepping before starting forward");
                m_frameStepTimer->stop();
                m_isSteppingBackward = false;
            }

            if (!m_isSteppingForward && !m_frameStepTimer->isActive())
            {
                // If video is playing, pause it first
                if (m_isPlaying)
                {
                    LOG_INFO("Pausing video for frame stepping");
                    m_mediaPlayer->pause();
                    m_playPauseBtn->setText("Play");
                    m_isPlaying = false;
                }

                nextFrame();
                m_isSteppingForward = true;
                m_stepInterval = 100;         // Start with 100ms interval
                m_frameStepTimer->start(300); // Initial delay before rapid stepping
                LOG_DEBUG("Started forward frame stepping");
            }
            event->accept();
            return;

        case Qt::Key_Space:
            LOG_DEBUG("Space key pressed");
            playPause();
            event->accept();
            return;

        case Qt::Key_S:
            if (event->modifiers() & Qt::ControlModifier)
            {
                LOG_DEBUG("Ctrl+S pressed - saving frame");
                saveCurrentFrame();
                event->accept();
                return;
            }
            break;
        }
    }
    else
    {
        LOG_DEBUG("Key event ignored - no video loaded or invalid duration");
    }

    QMainWindow::keyPressEvent(event);
}

void MainWindow::keyReleaseEvent(QKeyEvent *event)
{
    LOG_DEBUG("Key release event: key={}", event->key());

    switch (event->key())
    {
    case Qt::Key_Left:
        if (m_isSteppingBackward)
        {
            LOG_DEBUG("Left arrow key released - stopping backward frame stepping");
            m_frameStepTimer->stop();
            m_isSteppingBackward = false;
            m_stepInterval = 100; // Reset interval for next time
            event->accept();
            return;
        }
        break;

    case Qt::Key_Right:
        if (m_isSteppingForward)
        {
            LOG_DEBUG("Right arrow key released - stopping forward frame stepping");
            m_frameStepTimer->stop();
            m_isSteppingForward = false;
            m_stepInterval = 100; // Reset interval for next time
            event->accept();
            return;
        }
        break;
    }

    QMainWindow::keyReleaseEvent(event);
}

void MainWindow::mousePressEvent(QMouseEvent *event)
{
    LOG_DEBUG("Mouse press event - ensuring keyboard focus");
    setFocus();
    QMainWindow::mousePressEvent(event);
}

void MainWindow::onFrameStepTimer()
{
    if (m_isSteppingForward)
    {
        LOG_TRACE("Frame step timer - stepping forward (interval: {}ms)", m_stepInterval);
        nextFrame();
        // Accelerate stepping - reduce interval to minimum of 20ms
        if (m_stepInterval > 20)
        {
            m_stepInterval = qMax(20, m_stepInterval - 10);
            m_frameStepTimer->setInterval(m_stepInterval);
            LOG_TRACE("Accelerated stepping interval to {}ms", m_stepInterval);
        }
    }
    else if (m_isSteppingBackward)
    {
        LOG_TRACE("Frame step timer - stepping backward (interval: {}ms)", m_stepInterval);
        previousFrame();
        // Accelerate stepping - reduce interval to minimum of 20ms
        if (m_stepInterval > 20)
        {
            m_stepInterval = qMax(20, m_stepInterval - 10);
            m_frameStepTimer->setInterval(m_stepInterval);
            LOG_TRACE("Accelerated stepping interval to {}ms", m_stepInterval);
        }
    }
    else
    {
        LOG_DEBUG("Frame step timer - stopping (no active stepping)");
        m_frameStepTimer->stop();
    }
}

QString MainWindow::formatTime(qint64 milliseconds)
{
    qint64 seconds = milliseconds / 1000;
    qint64 minutes = seconds / 60;
    seconds %= 60;
    qint64 hours = minutes / 60;
    minutes %= 60;

    if (hours > 0)
    {
        return QString("%1:%2:%3")
            .arg(hours, 2, 10, QChar('0'))
            .arg(minutes, 2, 10, QChar('0'))
            .arg(seconds, 2, 10, QChar('0'));
    }
    else
    {
        return QString("%1:%2")
            .arg(minutes, 2, 10, QChar('0'))
            .arg(seconds, 2, 10, QChar('0'));
    }
}
