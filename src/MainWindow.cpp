#include "MainWindow.h"
#include <QApplication>
#include <QDir>
#include <QStandardPaths>
#include <QDateTime>
#include <QPixmap>
#include <QVideoFrame>
#include <QVideoSink>
#include <QPainter>
#include <QTimer>
#include <QFileInfo>
#include <QSettings>
#include <QEvent>
#include <QFont>
#include <QTime>
#include <QProcess>
#include <QRegularExpression>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), m_centralWidget(nullptr), m_mainSplitter(nullptr), m_videoWidget(nullptr), m_videoDisplay(nullptr), m_mediaPlayer(nullptr), m_frameCaptureSink(nullptr), m_controlsWidget(nullptr), m_playPauseBtn(nullptr), m_previousFrameBtn(nullptr), m_nextFrameBtn(nullptr), m_saveFrameBtn(nullptr), m_positionSlider(nullptr), m_timeLabel(nullptr), m_durationLabel(nullptr), m_frameListWidget(nullptr), m_frameList(nullptr), m_removeFrameBtn(nullptr), m_exportFramesBtn(nullptr), m_clearFramesBtn(nullptr), m_frameCountLabel(nullptr), m_settingsGroup(nullptr), m_outputDirEdit(nullptr), m_browseDirBtn(nullptr), m_imageFormatCombo(nullptr), m_openVideoAction(nullptr), m_exitAction(nullptr), m_aboutAction(nullptr), m_progressBar(nullptr), m_filePathLabel(nullptr), m_frameStepTimer(nullptr), m_isSteppingForward(false), m_isSteppingBackward(false), m_stepInterval(200), m_videoDuration(0), m_isPlaying(false), m_toggleFrameListBtn(nullptr), m_frameCaptureMethod(CAPTURE_QT_SINK), m_ffmpegAvailable(false), m_lastPositionUpdate(0), m_lastUIUpdate(0)
{
    setupUI();
    setupMenuBar();
    connectSignals();
    updateControls();

    // Setup frame stepping timer
    m_frameStepTimer = new QTimer(this);
    m_frameStepTimer->setSingleShot(false);
    connect(m_frameStepTimer, &QTimer::timeout, this, &MainWindow::onFrameStepTimer);

    // Load settings before setting default values
    loadSettings();

    // Check ffmpeg availability and set default capture method
    m_ffmpegAvailable = checkFFmpegAvailable();
    m_frameCaptureMethod = m_ffmpegAvailable ? CAPTURE_FFMPEG : CAPTURE_QT_SINK;

    LOG_INFO("FFmpeg available: {}, using capture method: {}",
             m_ffmpegAvailable,
             m_frameCaptureMethod == CAPTURE_FFMPEG ? "FFmpeg" : "Qt Sink");

    // Set default output directory if not loaded from settings
    if (m_outputDirectory.isEmpty())
    {
        m_outputDirectory = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/AnnotationFrames";
    }
    m_outputDirEdit->setText(m_outputDirectory);

    // Create output directory if it doesn't exist
    QDir().mkpath(m_outputDirectory);

    // Auto-load last opened video if it exists
    if (!m_lastVideoPath.isEmpty() && QFileInfo::exists(m_lastVideoPath))
    {
        LOG_INFO("Auto-loading last video: {}", m_lastVideoPath.toStdString());
        // Use a timer to load the video after the UI is fully initialized
        QTimer::singleShot(100, [this]()
                           {
            m_currentVideoPath = m_lastVideoPath;
            setDefaultFilenamePrefix(m_lastVideoPath);
            updateFilePathDisplay(m_lastVideoPath);
            m_mediaPlayer->setVideoOutput(m_videoDisplay);
            m_mediaPlayer->setSource(QUrl::fromLocalFile(m_lastVideoPath));
            statusBar()->showMessage("Auto-loaded: " + QFileInfo(m_lastVideoPath).fileName(), 3000);
            updateControls(); });
    }

    setWindowTitle("Image Annotation Picker");
    resize(1200, 800);

    // Ensure main window can receive keyboard events
    setFocusPolicy(Qt::StrongFocus);
    setFocus();

    // Show keyboard shortcuts in status bar initially - shortened duration to avoid potential freezing
    statusBar()->showMessage("Keyboard shortcuts: ← → (frame navigation), Space (play/pause), Ctrl+S (save frame)", 2000);
}

MainWindow::~MainWindow()
{
    // Save settings before cleanup
    saveSettings();

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

// Add performance optimizations for smoother playback
#ifdef Q_OS_MACOS
    // On macOS, try to reduce buffer sizes for lower latency
    if (m_mediaPlayer->metaObject()->indexOfProperty("bufferSize") != -1)
    {
        m_mediaPlayer->setProperty("bufferSize", 1024 * 1024); // 1MB buffer instead of default
    }
#endif

    // Create frame capture sink for saving frames
    m_frameCaptureSink = new FrameCaptureSink(this);
    // NOTE: Commenting out unused signal connection that was causing UI hangups
    // This was being called 30-60 times per second during playback with no benefit
    // connect(m_frameCaptureSink, &FrameCaptureSink::frameAvailable, this, &MainWindow::onFrameAvailable);
    LOG_INFO("Created frame capture sink");

    // Connect to the video widget's sink to capture frames
    QVideoSink *displaySink = m_videoDisplay->videoSink();
    if (displaySink)
    {
        connect(displaySink, &QVideoSink::videoFrameChanged, m_frameCaptureSink, &FrameCaptureSink::onFrameChanged);
        LOG_INFO("Connected to display sink for frame capture");
    }
    else
    {
        LOG_ERROR("Failed to get display sink from video widget");
    }
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

    // Frame list title with toggle button
    QHBoxLayout *frameListTitleLayout = new QHBoxLayout;
    QLabel *frameListTitle = new QLabel("Selected Frames");
    frameListTitle->setStyleSheet("font-weight: bold;");
    m_toggleFrameListBtn = new QPushButton("<<");
    m_toggleFrameListBtn->setFixedSize(30, 25);
    m_toggleFrameListBtn->setToolTip("Hide/Show frame list panel");

    frameListTitleLayout->addWidget(frameListTitle);
    frameListTitleLayout->addStretch();
    frameListTitleLayout->addWidget(m_toggleFrameListBtn);
    frameListTitleLayout->setContentsMargins(0, 0, 0, 0);
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

    // Filename prefix layout
    QHBoxLayout *filenamePrefixLayout = new QHBoxLayout;
    QLabel *filenamePrefixLabel = new QLabel("Filename Prefix:");
    m_filenamePrefixEdit = new QLineEdit("frame");
    m_filenamePrefixEdit->setMinimumWidth(150); // Make it wider

    // Put pattern hint on next line to save space
    QLabel *patternHint = new QLabel("Pattern: <prefix>_<timestamp>_<videoposition>ms_<width>_<height>.png");
    patternHint->setStyleSheet("color: gray; font-style: italic; font-size: 10px;");

    filenamePrefixLayout->addWidget(filenamePrefixLabel);
    filenamePrefixLayout->addWidget(m_filenamePrefixEdit);
    filenamePrefixLayout->addStretch();

    settingsLayout->addLayout(outputDirLayout);
    settingsLayout->addLayout(formatLayout);
    settingsLayout->addLayout(filenamePrefixLayout);
    settingsLayout->addWidget(patternHint);

    frameLayout->addLayout(frameListTitleLayout);
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
    m_filePathLabel = new QLabel("No video loaded");
    m_filePathLabel->setMinimumWidth(200);
    m_filePathLabel->setToolTip("Currently loaded video file");
    statusBar()->addWidget(m_filePathLabel);

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

    // Auto-update button states based on frame list changes (instead of manual updateControls calls)
    connect(m_frameList, &QListWidget::itemSelectionChanged, [this]()
            { m_removeFrameBtn->setEnabled(m_frameList->currentRow() >= 0); });
    connect(m_frameList, &QListWidget::itemChanged, [this]()
            {
        bool hasFrames = m_frameList->count() > 0;
        m_exportFramesBtn->setEnabled(hasFrames);
        m_clearFramesBtn->setEnabled(hasFrames); });
    // Also handle when items are added/removed programmatically
    connect(m_frameList->model(), &QAbstractItemModel::rowsInserted, [this]()
            {
        bool hasFrames = m_frameList->count() > 0;
        m_exportFramesBtn->setEnabled(hasFrames);
        m_clearFramesBtn->setEnabled(hasFrames); });
    connect(m_frameList->model(), &QAbstractItemModel::rowsRemoved, [this]()
            {
        bool hasFrames = m_frameList->count() > 0;
        m_exportFramesBtn->setEnabled(hasFrames);
        m_clearFramesBtn->setEnabled(hasFrames);
        m_removeFrameBtn->setEnabled(m_frameList->currentRow() >= 0); });

    // Settings
    connect(m_browseDirBtn, &QPushButton::clicked, [this]()
            {
        QString dir = QFileDialog::getExistingDirectory(this, "Select Output Directory", m_outputDirectory);
        if (!dir.isEmpty()) {
            m_outputDirectory = dir;
            m_outputDirEdit->setText(dir);

            // Save settings immediately when directory changes
            saveSettings();

            // Scan for existing frames when directory changes
            scanForExistingFrames();

            LOG_INFO("Output directory changed to: {}", dir.toStdString());
        } });

    // Toggle frame list visibility
    connect(m_toggleFrameListBtn, &QPushButton::clicked, [this]()
            {
        bool isVisible = m_frameListWidget->isVisible();
        m_frameListWidget->setVisible(!isVisible);
        m_toggleFrameListBtn->setText(isVisible ? ">>" : "<<");
        m_toggleFrameListBtn->setToolTip(isVisible ? "Show frame list panel" : "Hide frame list panel"); });

    // Add event filter to video widget to restore focus when clicked
    m_videoDisplay->installEventFilter(this);
}

void MainWindow::updateControls()
{
    // Only called when necessary (video load/unload) - no need for frequent performance logging
    bool hasVideo = !m_currentVideoPath.isEmpty();
    bool canSeek = hasVideo && m_videoDuration > 0;

    m_playPauseBtn->setEnabled(hasVideo);
    m_previousFrameBtn->setEnabled(canSeek);
    m_nextFrameBtn->setEnabled(canSeek);
    m_saveFrameBtn->setEnabled(hasVideo);
    m_positionSlider->setEnabled(canSeek);

    // Frame list button states are now handled automatically by signal connections
    m_removeFrameBtn->setEnabled(m_frameList->currentRow() >= 0);
    m_exportFramesBtn->setEnabled(m_frameList->count() > 0);
    m_clearFramesBtn->setEnabled(m_frameList->count() > 0);
}

void MainWindow::openVideo()
{
    // Start file dialog from last video directory or home
    QString startDir = m_lastVideoPath.isEmpty() ? QDir::homePath() : QFileInfo(m_lastVideoPath).absolutePath();

    QString fileName = QFileDialog::getOpenFileName(this,
                                                    "Open Video File",
                                                    startDir,
                                                    "Video Files (*.mp4 *.avi *.mov *.mkv *.wmv *.flv *.webm)");

    if (!fileName.isEmpty())
    {
        LOG_INFO("Opening video file: {}", fileName.toStdString());
        m_currentVideoPath = fileName;
        m_lastVideoPath = fileName; // Remember for next time

        // Save settings immediately when new video is loaded
        saveSettings();

        // Set default filename prefix from video filename
        setDefaultFilenamePrefix(fileName);

        // Update file path display
        updateFilePathDisplay(fileName);

        // Set up dual output: video widget for display and video sink for frame capture
        m_mediaPlayer->setVideoOutput(m_videoDisplay);
        // Note: In Qt6, we can't easily have dual outputs, so we'll use a different approach

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
    qint64 saveStart = QDateTime::currentMSecsSinceEpoch();
    LOG_TRACE("💾 SAVE: saveCurrentFrame() START");

    if (m_currentVideoPath.isEmpty())
    {
        QMessageBox::warning(this, "Warning", "No video loaded.");
        return;
    }

    // Use the new frame capture implementation
    captureCurrentFrame();

    qint64 duration = QDateTime::currentMSecsSinceEpoch() - saveStart;
    if (duration > 5)
    {
        LOG_WARN("💾 SAVE: saveCurrentFrame() took {}ms (may cause UI lag)", duration);
    }
    else
    {
        LOG_TRACE("💾 SAVE: saveCurrentFrame() completed in {}ms", duration);
    }
}

void MainWindow::playPause()
{
    qint64 playPauseStart = QDateTime::currentMSecsSinceEpoch();
    LOG_TRACE("⏯️ PLAY: playPause() START - current state: {}", m_isPlaying ? "playing" : "paused");

    if (m_isPlaying)
    {
        m_mediaPlayer->pause();
        m_playPauseBtn->setText("Play");
        m_isPlaying = false;
        LOG_INFO("⏸️ Video paused");
    }
    else
    {
        m_mediaPlayer->play();
        m_playPauseBtn->setText("Pause");
        m_isPlaying = true;
        LOG_INFO("▶️ Video playing");
    }

    qint64 duration = QDateTime::currentMSecsSinceEpoch() - playPauseStart;
    if (duration > 2)
    {
        LOG_WARN("⏯️ PLAY: playPause() took {}ms (may cause UI lag)", duration);
    }
    else
    {
        LOG_TRACE("⏯️ PLAY: playPause() completed in {}ms", duration);
    }
}

void MainWindow::nextFrame()
{
    qint64 frameStart = QDateTime::currentMSecsSinceEpoch();
    LOG_TRACE("➡️ FRAME: nextFrame() called");

    // Throttle position updates to avoid overwhelming the media player
    qint64 currentTime = QDateTime::currentMSecsSinceEpoch();
    if (currentTime - m_lastPositionUpdate < 30)
    { // Minimum 30ms between updates
        LOG_TRACE("➡️ FRAME: Throttled - only {}ms since last update", currentTime - m_lastPositionUpdate);
        return;
    }
    m_lastPositionUpdate = currentTime;

    // Jump forward by a larger amount to ensure we hit different frames
    // Use 100ms jumps instead of 33ms to avoid getting stuck between keyframes
    qint64 currentPos = m_mediaPlayer->position();
    qint64 frameTime = 100; // 100ms per step for more reliable frame changes
    qint64 newPos = qMin(currentPos + frameTime, m_videoDuration);

    // Only log occasionally to reduce UI overhead during rapid stepping
    static qint64 lastLogTime = 0;
    if (currentTime - lastLogTime > 500)
    { // Log every 500ms max
        LOG_DEBUG("➡️ FRAME: nextFrame() - current: {}ms, frameTime: {}ms, new: {}ms", currentPos, frameTime, newPos);
        lastLogTime = currentTime;
    }

    // Direct position update without additional timers
    LOG_TRACE("➡️ FRAME: Setting media player position to {}ms", newPos);
    m_mediaPlayer->setPosition(newPos);

    qint64 frameEnd = QDateTime::currentMSecsSinceEpoch();
    qint64 duration = frameEnd - frameStart;
    if (duration > 3) // Log if frame operation takes more than 3ms
    {
        LOG_WARN("➡️ FRAME: nextFrame() took {}ms (may cause UI hangup)", duration);
    }
}

void MainWindow::previousFrame()
{
    qint64 frameStart = QDateTime::currentMSecsSinceEpoch();
    LOG_TRACE("⬅️ FRAME: previousFrame() called");

    // Throttle position updates to avoid overwhelming the media player
    qint64 currentTime = QDateTime::currentMSecsSinceEpoch();
    if (currentTime - m_lastPositionUpdate < 30)
    { // Minimum 30ms between updates
        LOG_TRACE("⬅️ FRAME: Throttled - only {}ms since last update", currentTime - m_lastPositionUpdate);
        return;
    }
    m_lastPositionUpdate = currentTime;

    // Jump backward by a larger amount to ensure we hit different frames
    qint64 currentPos = m_mediaPlayer->position();
    qint64 frameTime = 100; // 100ms per step for more reliable frame changes
    qint64 newPos = qMax(currentPos - frameTime, 0LL);

    // Only log occasionally to reduce UI overhead during rapid stepping
    static qint64 lastLogTime = 0;
    if (currentTime - lastLogTime > 500)
    { // Log every 500ms max
        LOG_DEBUG("⬅️ FRAME: previousFrame() - current: {}ms, frameTime: {}ms, new: {}ms", currentPos, frameTime, newPos);
        lastLogTime = currentTime;
    }

    // Direct position update without additional timers
    LOG_TRACE("⬅️ FRAME: Setting media player position to {}ms", newPos);
    m_mediaPlayer->setPosition(newPos);

    qint64 frameEnd = QDateTime::currentMSecsSinceEpoch();
    qint64 duration = frameEnd - frameStart;
    if (duration > 3) // Log if frame operation takes more than 3ms
    {
        LOG_WARN("⬅️ FRAME: previousFrame() took {}ms (may cause UI hangup)", duration);
    }
}

void MainWindow::seekToPosition(int position)
{
    qint64 seekStart = QDateTime::currentMSecsSinceEpoch();
    LOG_TRACE("🎯 SEEK: seekToPosition() START - position: {}ms", position);

    m_mediaPlayer->setPosition(position);

    // Ensure main window gets focus back after slider interaction
    setFocus();

    qint64 duration = QDateTime::currentMSecsSinceEpoch() - seekStart;
    if (duration > 3)
    {
        LOG_WARN("🎯 SEEK: seekToPosition() took {}ms (may cause UI stutter)", duration);
    }
    else
    {
        LOG_TRACE("🎯 SEEK: seekToPosition() completed in {}ms", duration);
    }
}

void MainWindow::onPositionChanged(qint64 position)
{
    // Throttle UI updates to reduce overhead - only update every 300ms (was 200ms)
    qint64 currentTime = QDateTime::currentMSecsSinceEpoch();
    bool shouldUpdateUI = (currentTime - m_lastUIUpdate > 300);

    // Update slider position less frequently to reduce UI overhead
    if (shouldUpdateUI && !m_positionSlider->isSliderDown())
    {
        m_positionSlider->setValue(static_cast<int>(position));
        m_timeLabel->setText(formatTime(position));
        m_lastUIUpdate = currentTime;
    }

    // Minimal logging to avoid overhead - only log every 10 seconds
    static qint64 lastLoggedPosition = -1;
    if (abs(position - lastLoggedPosition) > 10000)
    {
        LOG_DEBUG("📡 POSITION: {}ms", position);
        lastLoggedPosition = position;
    }
}

void MainWindow::onDurationChanged(qint64 duration)
{
    qint64 durationStart = QDateTime::currentMSecsSinceEpoch();
    LOG_TRACE("⏱️ DURATION: onDurationChanged() START - duration: {}ms ({})", duration, formatTime(duration).toStdString());

    m_videoDuration = duration;
    m_positionSlider->setRange(0, static_cast<int>(duration));
    m_durationLabel->setText(formatTime(duration));
    // Update controls when duration is set - this enables frame navigation buttons
    updateControls();

    qint64 durationEnd = QDateTime::currentMSecsSinceEpoch();
    qint64 elapsed = durationEnd - durationStart;
    if (elapsed > 3)
    {
        LOG_WARN("⏱️ DURATION: onDurationChanged() took {}ms (may cause UI lag)", elapsed);
    }
    else
    {
        LOG_TRACE("⏱️ DURATION: onDurationChanged() completed in {}ms", elapsed);
    }
}

void MainWindow::onMediaStatusChanged(QMediaPlayer::MediaStatus status)
{
    qint64 statusStart = QDateTime::currentMSecsSinceEpoch();

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

    LOG_INFO("🎬 MEDIA: Status changed to: {}", statusStr.toStdString());

    switch (status)
    {
    case QMediaPlayer::LoadedMedia:
        statusBar()->showMessage("Video loaded successfully", 2000);
        // Ensure main window has focus for keyboard events
        setFocus();
        // Scan for existing frames when video is loaded
        scanForExistingFrames();
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

void MainWindow::onMediaError(QMediaPlayer::Error error, const QString &errorString)
{
    QString errorTypeStr;
    switch (error)
    {
    case QMediaPlayer::NoError:
        errorTypeStr = "NoError";
        break;
    case QMediaPlayer::ResourceError:
        errorTypeStr = "ResourceError";
        break;
    case QMediaPlayer::FormatError:
        errorTypeStr = "FormatError";
        break;
    case QMediaPlayer::NetworkError:
        errorTypeStr = "NetworkError";
        break;
    case QMediaPlayer::AccessDeniedError:
        errorTypeStr = "AccessDeniedError";
        break;
    default:
        errorTypeStr = "UnknownError";
        break;
    }

    LOG_ERROR("Media player error - Type: {}, Message: {}", errorTypeStr.toStdString(), errorString.toStdString());

    if (error != QMediaPlayer::NoError)
    {
        statusBar()->showMessage(QString("Media Error: %1").arg(errorString), 5000);
        QMessageBox::critical(this, "Media Error",
                              QString("Error Type: %1\nMessage: %2").arg(errorTypeStr, errorString));
    }
}

void MainWindow::removeSelectedFrame()
{
    int currentRow = m_frameList->currentRow();
    if (currentRow >= 0)
    {
        delete m_frameList->takeItem(currentRow);
        m_frameCountLabel->setText(QString("Frames: %1").arg(m_frameList->count()));
        // NOTE: Removed updateControls() - the rowsRemoved signal will handle this automatically
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
            // NOTE: Removed updateControls() - the rowsRemoved signal will handle this automatically
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
    // NOTE: Removed updateControls() - frame list changes don't affect media controls, only list-specific buttons
    // The list selection change signal will handle enabling/disabling remove button automatically
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    qint64 keyStart = QDateTime::currentMSecsSinceEpoch();
    LOG_TRACE("⌨️ KEY: keyPressEvent() START - key={}, modifiers={}, repeat={}", event->key(), event->modifiers(), event->isAutoRepeat());

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
                m_stepInterval = 200;         // Increased from 150ms to 200ms for more conservative stepping
                m_frameStepTimer->start(500); // Increased initial delay from 400ms to 500ms
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
                m_stepInterval = 200;         // Increased from 150ms to 200ms for more conservative stepping
                m_frameStepTimer->start(500); // Increased initial delay from 400ms to 500ms
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

    qint64 duration = QDateTime::currentMSecsSinceEpoch() - keyStart;
    if (duration > 2)
    {
        LOG_WARN("⌨️ KEY: keyPressEvent() took {}ms (may cause UI lag)", duration);
    }
    else
    {
        LOG_TRACE("⌨️ KEY: keyPressEvent() completed in {}ms", duration);
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
            m_stepInterval = 200; // Reset interval for next time
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
            m_stepInterval = 200; // Reset interval for next time
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

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_videoDisplay && event->type() == QEvent::MouseButtonPress)
    {
        LOG_DEBUG("Video widget clicked - restoring keyboard focus to main window");
        setFocus();
        return false; // Let the event continue to be processed
    }
    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::onFrameStepTimer()
{
    qint64 timerStart = QDateTime::currentMSecsSinceEpoch();
    LOG_TRACE("⏰ TIMER: onFrameStepTimer() START - forward: {}, backward: {}, interval: {}ms",
              m_isSteppingForward, m_isSteppingBackward, m_stepInterval);

    if (m_isSteppingForward)
    {
        LOG_TRACE("⏰ TIMER: Calling nextFrame() from timer");
        nextFrame();
        // More conservative acceleration - minimum interval of 100ms (increased from 50ms)
        if (m_stepInterval > 100)
        {
            m_stepInterval = qMax(100, m_stepInterval - 10); // Slower acceleration
            m_frameStepTimer->setInterval(m_stepInterval);
            LOG_TRACE("⏰ TIMER: Accelerated stepping interval to {}ms", m_stepInterval);
        }
    }
    else if (m_isSteppingBackward)
    {
        LOG_TRACE("⏰ TIMER: Calling previousFrame() from timer");
        previousFrame();
        // More conservative acceleration - minimum interval of 100ms (increased from 50ms)
        if (m_stepInterval > 100)
        {
            m_stepInterval = qMax(100, m_stepInterval - 10); // Slower acceleration
            m_frameStepTimer->setInterval(m_stepInterval);
            LOG_TRACE("⏰ TIMER: Accelerated stepping interval to {}ms", m_stepInterval);
        }
    }
    else
    {
        LOG_DEBUG("⏰ TIMER: Frame step timer - stopping (no active stepping)");
        m_frameStepTimer->stop();
    }

    qint64 timerEnd = QDateTime::currentMSecsSinceEpoch();
    qint64 duration = timerEnd - timerStart;
    if (duration > 5) // Log if timer takes more than 5ms
    {
        LOG_WARN("⏰ TIMER: onFrameStepTimer() took {}ms (may cause UI hangup)", duration);
    }
    else
    {
        LOG_TRACE("⏰ TIMER: onFrameStepTimer() completed in {}ms", duration);
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

QString MainWindow::extractFilenamePrefix(const QString &videoPath)
{
    QFileInfo fileInfo(videoPath);
    QString baseName = fileInfo.baseName(); // Gets filename without extension

    // Check if it looks like a YouTube ID (11 characters followed by underscore)
    if (baseName.length() >= 12 && baseName.at(11) == '_')
    {
        QString potentialYouTubeId = baseName.left(11);
        // YouTube IDs are alphanumeric with possible dashes and underscores
        QRegularExpression youtubeIdPattern("^[A-Za-z0-9_-]{11}$");
        if (youtubeIdPattern.match(potentialYouTubeId).hasMatch())
        {
            LOG_INFO("Detected YouTube ID format: {}", potentialYouTubeId.toStdString());
            return potentialYouTubeId;
        }
    }

    // Fallback: use the full filename (without extension) as prefix
    LOG_INFO("Using full filename as prefix: {}", baseName.toStdString());
    return baseName;
}

void MainWindow::setDefaultFilenamePrefix(const QString &videoPath)
{
    if (m_filenamePrefixEdit && !videoPath.isEmpty())
    {
        QString currentPrefix = m_filenamePrefixEdit->text();
        QString prefix = extractFilenamePrefix(videoPath);
        LOG_INFO("Current prefix: '{}', Extracted prefix: '{}'", currentPrefix.toStdString(), prefix.toStdString());

        if (!prefix.isEmpty())
        {
            m_filenamePrefixEdit->setText(prefix);
            LOG_INFO("Successfully set filename prefix to: {}", prefix.toStdString());
        }
    }
    else
    {
        LOG_WARN("Cannot set filename prefix: edit field={}, videoPath='{}'",
                 (m_filenamePrefixEdit != nullptr), videoPath.toStdString());
    }
}

void MainWindow::updateFilePathDisplay(const QString &filePath)
{
    if (!m_filePathLabel)
        return;

    if (filePath.isEmpty())
    {
        m_filePathLabel->setText("No video loaded");
        m_filePathLabel->setToolTip("");
        return;
    }

    QFileInfo fileInfo(filePath);
    QString fileName = fileInfo.fileName();
    QString displayText;

    // If the full path is short enough, show it all
    if (filePath.length() <= 80)
    {
        displayText = filePath;
    }
    // Otherwise prioritize showing the filename
    else if (fileName.length() <= 50)
    {
        QString dir = fileInfo.dir().absolutePath();
        int maxDirLength = 80 - fileName.length() - 4; // Reserve space for filename and ".../"
        if (dir.length() > maxDirLength)
        {
            dir = "..." + dir.right(maxDirLength - 3);
        }
        displayText = dir + "/" + fileName;
    }
    else
    {
        // Very long filename, truncate it
        displayText = "..." + fileName.right(77);
    }

    m_filePathLabel->setText(displayText);
    m_filePathLabel->setToolTip(filePath); // Full path in tooltip
}

void MainWindow::loadSettings()
{
    LOG_INFO("Loading application settings");
    QSettings settings;

    // Load last video path
    QString lastVideoPath = settings.value("lastVideoPath", "").toString();
    if (!lastVideoPath.isEmpty() && QFileInfo::exists(lastVideoPath))
    {
        LOG_INFO("Restored last video path: {}", lastVideoPath.toStdString());
        m_lastVideoPath = lastVideoPath;
    }

    // Load output directory
    QString outputDir = settings.value("outputDirectory", "").toString();
    if (!outputDir.isEmpty() && QDir(outputDir).exists())
    {
        m_outputDirectory = outputDir;
        LOG_INFO("Restored output directory: {}", outputDir.toStdString());
    }

    // Load filename prefix ONLY if no video will be auto-loaded
    // If video will be auto-loaded, let setDefaultFilenamePrefix handle it
    if (m_lastVideoPath.isEmpty())
    {
        QString filenamePrefix = settings.value("filenamePrefix", "frame").toString();
        if (m_filenamePrefixEdit)
        {
            m_filenamePrefixEdit->setText(filenamePrefix);
            LOG_INFO("Restored filename prefix: {}", filenamePrefix.toStdString());
        }
    }
    else
    {
        LOG_INFO("Will auto-extract filename prefix from video file, skipping saved prefix");
    }

    // Load window geometry
    QByteArray geometry = settings.value("geometry").toByteArray();
    if (!geometry.isEmpty())
    {
        restoreGeometry(geometry);
        LOG_INFO("Restored window geometry");
    }
}

void MainWindow::saveSettings()
{
    LOG_INFO("Saving application settings");
    QSettings settings;

    // Save last video path
    if (!m_lastVideoPath.isEmpty())
    {
        settings.setValue("lastVideoPath", m_lastVideoPath);
        LOG_INFO("Saved last video path: {}", m_lastVideoPath.toStdString());
    }

    // Save output directory
    settings.setValue("outputDirectory", m_outputDirectory);
    LOG_INFO("Saved output directory: {}", m_outputDirectory.toStdString());

    // Save filename prefix
    if (m_filenamePrefixEdit)
    {
        QString prefix = m_filenamePrefixEdit->text().trimmed();
        if (prefix.isEmpty())
        {
            prefix = "frame";
        }
        settings.setValue("filenamePrefix", prefix);
        LOG_INFO("Saved filename prefix: {}", prefix.toStdString());
    }

    // Save window geometry
    settings.setValue("geometry", saveGeometry());
    LOG_INFO("Saved window geometry");
}

QString MainWindow::generateFrameFilename()
{
    QString prefix = "frame";
    if (m_filenamePrefixEdit)
    {
        QString userPrefix = m_filenamePrefixEdit->text().trimmed();
        if (!userPrefix.isEmpty())
        {
            prefix = userPrefix;
        }
    }

    // Get current video position in milliseconds
    qint64 videoPosition = m_mediaPlayer ? m_mediaPlayer->position() : 0;

    // Use millisecond precision instead of seconds for better uniqueness
    // Format: {prefix}_{milliseconds}.png
    // This provides much better precision for frame-by-frame captures
    return QString("%1_%2.png")
        .arg(prefix)
        .arg(videoPosition);
}

void MainWindow::captureCurrentFrame()
{
    if (!m_mediaPlayer || m_mediaPlayer->playbackState() == QMediaPlayer::StoppedState)
    {
        LOG_ERROR("Cannot capture frame: no video loaded or player stopped");
        return;
    }

    LOG_INFO("Attempting to capture current frame using method: {}",
             m_frameCaptureMethod == CAPTURE_FFMPEG ? "FFmpeg" : "Qt Sink");

    switch (m_frameCaptureMethod)
    {
    case CAPTURE_FFMPEG:
        captureCurrentFrameFFmpeg();
        break;
    case CAPTURE_QT_SINK:
    default:
        captureCurrentFrameQt();
        break;
    }
}

// NOTE: Commenting out unused slot that was causing UI hangups
// This was being called 30-60 times per second during playback with no benefit
/*
void MainWindow::onFrameAvailable()
{
    // This slot is called when a new frame is available from the FrameCaptureSink
    // It's currently just a notification - the actual frame access happens in captureCurrentFrame()
    // We could use this for real-time frame processing if needed
}
*/

bool MainWindow::checkFFmpegAvailable()
{
    QProcess process;
    process.start("ffmpeg", QStringList() << "-version");
    process.waitForFinished(3000); // 3 second timeout

    bool available = (process.exitCode() == 0);
    LOG_INFO("FFmpeg availability check: {}", available ? "found" : "not found");

    return available;
}

void MainWindow::captureCurrentFrameQt()
{
    LOG_INFO("Using Qt sink capture method");

    // Generate filename with current prefix
    QString filename = generateFrameFilename();
    QString fullPath = QDir(m_outputDirectory).absoluteFilePath(filename);

    QPixmap framePixmap;

    // Try to capture from frame capture sink
    if (m_frameCaptureSink)
    {
        QVideoFrame currentFrame = m_frameCaptureSink->getCurrentFrame();
        LOG_INFO("Frame capture sink exists, current frame valid: {}", currentFrame.isValid());

        if (currentFrame.isValid())
        {
            LOG_INFO("Capturing frame from video sink, size: {}x{}, format: {}",
                     currentFrame.size().width(), currentFrame.size().height(),
                     (int)currentFrame.pixelFormat());

            // Convert QVideoFrame to QImage
            QVideoFrame mappedFrame = currentFrame;
            mappedFrame.map(QVideoFrame::ReadOnly);

            QImage frameImage = mappedFrame.toImage();

            if (!frameImage.isNull())
            {
                // Ensure proper color format - convert to RGB32 if needed for consistent output
                if (frameImage.format() != QImage::Format_RGB32 && frameImage.format() != QImage::Format_ARGB32)
                {
                    LOG_INFO("Converting image format from {} to RGB32", (int)frameImage.format());
                    frameImage = frameImage.convertToFormat(QImage::Format_RGB32);
                }

                framePixmap = QPixmap::fromImage(frameImage);
                LOG_INFO("Successfully converted video frame to pixmap, size: {}x{}, image format: {}",
                         framePixmap.width(), framePixmap.height(), (int)frameImage.format());
            }
            else
            {
                LOG_ERROR("Failed to convert video frame to image");
            }

            mappedFrame.unmap();
        }
        else
        {
            LOG_ERROR("No valid frame available from capture sink");
        }
    }

    // Fallback to placeholder if frame capture failed
    if (framePixmap.isNull())
    {
        LOG_INFO("Using placeholder image - Qt frame capture failed");
        framePixmap = QPixmap(800, 600);
        framePixmap.fill(Qt::darkGray);

        QPainter painter(&framePixmap);
        painter.setPen(Qt::white);
        painter.setFont(QFont("Arial", 16));
        painter.drawText(framePixmap.rect(), Qt::AlignCenter,
                         QString("Qt Frame capture failed\nPosition: %1ms\nTry playing the video first")
                             .arg(m_mediaPlayer->position()));
    }

    if (framePixmap.save(fullPath))
    {
        LOG_INFO("Frame saved to: {}", fullPath.toStdString());

        // Add to frame list using just the filename (without extension for display)
        QFileInfo fileInfo(filename);
        addFrameToList(fileInfo.baseName(), m_mediaPlayer->position());

        statusBar()->showMessage(QString("Frame saved: %1").arg(filename), 3000);
    }
    else
    {
        LOG_ERROR("Failed to save frame to: {}", fullPath.toStdString());
        statusBar()->showMessage("Failed to save frame", 3000);
    }
}

void MainWindow::captureCurrentFrameFFmpeg()
{
    LOG_INFO("Using FFmpeg capture method");

    if (m_currentVideoPath.isEmpty())
    {
        LOG_ERROR("No video path available for FFmpeg capture");
        return;
    }

    // Generate filename with current prefix
    QString filename = generateFrameFilename();
    QString fullPath = QDir(m_outputDirectory).absoluteFilePath(filename);

    // Get current position in seconds for ffmpeg
    double currentSeconds = m_mediaPlayer->position() / 1000.0;

    // Create ffmpeg command to extract frame at current position
    QStringList arguments;
    arguments << "-ss" << QString::number(currentSeconds, 'f', 3) // Seek to position
              << "-i" << m_currentVideoPath                       // Input file
              << "-frames:v" << "1"                               // Extract 1 frame
              << "-q:v" << "2"                                    // High quality
              << "-y"                                             // Overwrite output
              << fullPath;                                        // Output file

    LOG_INFO("FFmpeg command: ffmpeg {}", arguments.join(" ").toStdString());

    // Run ffmpeg in background thread
    QProcess *ffmpegProcess = new QProcess(this);

    // Handle completion
    connect(ffmpegProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            [this, ffmpegProcess, filename, fullPath](int exitCode, QProcess::ExitStatus exitStatus)
            {
                ffmpegProcess->deleteLater();

                if (exitCode == 0 && exitStatus == QProcess::NormalExit)
                {
                    LOG_INFO("FFmpeg frame saved to: {}", fullPath.toStdString());

                    // Add to frame list using just the filename (without extension for display)
                    QFileInfo fileInfo(filename);
                    addFrameToList(fileInfo.baseName(), m_mediaPlayer->position());

                    statusBar()->showMessage(QString("Frame saved: %1").arg(filename), 3000);
                }
                else
                {
                    QString errorOutput = ffmpegProcess->readAllStandardError();
                    LOG_ERROR("FFmpeg failed with exit code {}: {}", exitCode, errorOutput.toStdString());
                    statusBar()->showMessage("FFmpeg frame capture failed", 3000);
                }
            });

    // Handle errors
    connect(ffmpegProcess, &QProcess::errorOccurred,
            [this, ffmpegProcess](QProcess::ProcessError error)
            {
                ffmpegProcess->deleteLater();
                LOG_ERROR("FFmpeg process error: {}", (int)error);
                statusBar()->showMessage("FFmpeg process error", 3000);
            });

    statusBar()->showMessage("Capturing frame with FFmpeg...", 1000);
    ffmpegProcess->start("ffmpeg", arguments);
}

void MainWindow::scanForExistingFrames()
{
    if (m_currentVideoPath.isEmpty() || m_outputDirectory.isEmpty())
    {
        LOG_DEBUG("Cannot scan for existing frames: no video or output directory");
        return;
    }

    LOG_INFO("Scanning for existing frames in: {}", m_outputDirectory.toStdString());

    m_existingFrameTimestamps = parseExistingFrameTimestamps();

    LOG_INFO("Found {} existing frame(s)", m_existingFrameTimestamps.size());

    // Update timeline markers
    updateTimelineMarkers();
}

QList<qint64> MainWindow::parseExistingFrameTimestamps()
{
    QList<qint64> timestamps;
    QDir outputDir(m_outputDirectory);

    if (!outputDir.exists())
    {
        LOG_DEBUG("Output directory doesn't exist: {}", m_outputDirectory.toStdString());
        return timestamps;
    }

    // Get current filename prefix
    QString currentPrefix = "frame";
    if (m_filenamePrefixEdit)
    {
        QString userPrefix = m_filenamePrefixEdit->text().trimmed();
        if (!userPrefix.isEmpty())
        {
            currentPrefix = userPrefix;
        }
    }

    // Create regex pattern to match our NEW filename format:
    // prefix_YYYYMMDD_hhmmss_zzz_XXXXms_width_height.ext
    QString pattern = QString("^%1_(\\d{8})_(\\d{6})_(\\d{3})_(\\d+)ms_(\\d+)_(\\d+)\\.(png|jpg|jpeg|bmp|tiff)$")
                          .arg(QRegularExpression::escape(currentPrefix));
    QRegularExpression newFormatRegex(pattern, QRegularExpression::CaseInsensitiveOption);

    // Also support OLD filename format for backward compatibility:
    // prefix_YYYYMMDD_hhmmss_zzz_width_height.ext
    QString oldPattern = QString("^%1_(\\d{8})_(\\d{6})_(\\d{3})_(\\d+)_(\\d+)\\.(png|jpg|jpeg|bmp|tiff)$")
                             .arg(QRegularExpression::escape(currentPrefix));
    QRegularExpression oldFormatRegex(oldPattern, QRegularExpression::CaseInsensitiveOption);

    // Get all image files in directory
    QStringList nameFilters;
    nameFilters << "*.png" << "*.jpg" << "*.jpeg" << "*.bmp" << "*.tiff";
    QFileInfoList files = outputDir.entryInfoList(nameFilters, QDir::Files);

    LOG_INFO("Scanning directory: {}", m_outputDirectory.toStdString());
    LOG_INFO("Using prefix: '{}'", currentPrefix.toStdString());
    LOG_INFO("Found {} image files total", files.size());
    LOG_DEBUG("New pattern: {}", pattern.toStdString());
    LOG_DEBUG("Old pattern: {}", oldPattern.toStdString());

    // List all files for debugging
    for (const QFileInfo &fileInfo : files)
    {
        LOG_DEBUG("File found: {}", fileInfo.fileName().toStdString());
    }

    for (const QFileInfo &fileInfo : files)
    {
        QString filename = fileInfo.fileName();
        LOG_DEBUG("Processing file: {}", filename.toStdString());

        qint64 timestamp = extractTimestampFromFilename(filename);

        if (timestamp >= 0)
        {
            timestamps.append(timestamp);
            LOG_INFO("✓ Found existing frame: {} -> {}ms", filename.toStdString(), timestamp);
        }
        else
        {
            LOG_DEBUG("✗ Skipped file (no timestamp): {}", filename.toStdString());
        }
    }

    // Sort timestamps
    std::sort(timestamps.begin(), timestamps.end());

    return timestamps;
}

qint64 MainWindow::extractTimestampFromFilename(const QString &filename)
{
    // Get current filename prefix
    QString currentPrefix = "frame";
    if (m_filenamePrefixEdit)
    {
        QString userPrefix = m_filenamePrefixEdit->text().trimmed();
        if (!userPrefix.isEmpty())
        {
            currentPrefix = userPrefix;
        }
    }

    LOG_DEBUG("Extracting timestamp from: '{}' with prefix: '{}'", filename.toStdString(), currentPrefix.toStdString());

    // Try NEW simplified format: prefix_milliseconds.ext
    QString newPattern = QString("^%1_(\\d+)\\.(png|jpg|jpeg|bmp|tiff)$")
                             .arg(QRegularExpression::escape(currentPrefix));
    QRegularExpression newFormatRegex(newPattern, QRegularExpression::CaseInsensitiveOption);
    QRegularExpressionMatch newMatch = newFormatRegex.match(filename);

    LOG_DEBUG("Testing new simplified format pattern: {}", newPattern.toStdString());

    if (newMatch.hasMatch())
    {
        // Extract video position in milliseconds directly
        qint64 videoPosition = newMatch.captured(1).toLongLong();
        LOG_INFO("✓ Extracted video position from new format: {}ms", videoPosition);
        return videoPosition;
    }
    else
    {
        LOG_DEBUG("✗ New simplified format pattern didn't match");
    }

    // Try OLD detailed format for backward compatibility: prefix_YYYYMMDD_hhmmss_zzz_XXXXms_width_height.ext
    QString oldDetailedPattern = QString("^%1_(\\d{8})_(\\d{6})_(\\d{3})_(\\d+)ms_(\\d+)_(\\d+)\\.(png|jpg|jpeg|bmp|tiff)$")
                                     .arg(QRegularExpression::escape(currentPrefix));
    QRegularExpression oldDetailedRegex(oldDetailedPattern, QRegularExpression::CaseInsensitiveOption);
    QRegularExpressionMatch oldDetailedMatch = oldDetailedRegex.match(filename);

    LOG_DEBUG("Testing old detailed format pattern: {}", oldDetailedPattern.toStdString());

    if (oldDetailedMatch.hasMatch())
    {
        // Extract video position from the old detailed format
        qint64 videoPosition = oldDetailedMatch.captured(4).toLongLong();
        LOG_INFO("✓ Extracted video position from old detailed format: {}ms", videoPosition);
        return videoPosition;
    }
    else
    {
        LOG_DEBUG("✗ Old detailed format pattern didn't match");
    }

    // Try OLD filename format for backward compatibility: prefix_YYYYMMDD_hhmmss_zzz_width_height.ext
    QString oldPattern = QString("^%1_(\\d{8})_(\\d{6})_(\\d{3})_(\\d+)_(\\d+)\\.(png|jpg|jpeg|bmp|tiff)$")
                             .arg(QRegularExpression::escape(currentPrefix));
    QRegularExpression oldFormatRegex(oldPattern, QRegularExpression::CaseInsensitiveOption);
    QRegularExpressionMatch oldMatch = oldFormatRegex.match(filename);

    LOG_DEBUG("Testing old format pattern: {}", oldPattern.toStdString());

    if (oldMatch.hasMatch())
    {
        LOG_INFO("✓ Found old format file (no video position): {}", filename.toStdString());
        // For old format files, we can't extract video position, so skip them
        return -1;
    }
    else
    {
        LOG_DEBUG("✗ Old format pattern didn't match either");
    }

    LOG_DEBUG("✗ Filename doesn't match any expected pattern: {}", filename.toStdString());
    return -1;
}

void MainWindow::updateTimelineMarkers()
{
    if (!m_positionSlider || m_videoDuration <= 0)
    {
        LOG_DEBUG("Cannot update timeline markers: invalid slider or duration");
        return;
    }

    if (m_existingFrameTimestamps.isEmpty())
    {
        LOG_DEBUG("No existing frame timestamps to mark on timeline");
        // Reset to default slider style
        m_positionSlider->setStyleSheet("");
        return;
    }

    LOG_INFO("Marking {} existing frames on timeline", m_existingFrameTimestamps.size());

    // Create a simple stylesheet that adds visual indicators
    // We'll use a custom background with gradient stops at frame positions
    QString styleSheet = "QSlider::groove:horizontal {"
                         "border: 1px solid #999999;"
                         "height: 8px;"
                         "background: qlineargradient(x1:0, y1:0, x2:1, y2:0";

    // Add gradient stops for existing frames
    for (int i = 0; i < m_existingFrameTimestamps.size(); ++i)
    {
        qint64 timestamp = m_existingFrameTimestamps[i];
        double position = static_cast<double>(timestamp) / static_cast<double>(m_videoDuration);

        // Clamp position to valid range
        position = qMax(0.0, qMin(1.0, position));

        // Add a red marker at this position
        styleSheet += QString(", stop:%1 #ff4444").arg(position, 0, 'f', 4);

        // Add a small range around the marker
        if (position > 0.002)
        {
            styleSheet += QString(", stop:%1 #cccccc").arg(position - 0.002, 0, 'f', 4);
        }
        if (position < 0.998)
        {
            styleSheet += QString(", stop:%1 #cccccc").arg(position + 0.002, 0, 'f', 4);
        }
    }

    styleSheet += ");"
                  "border-radius: 4px;"
                  "}"
                  "QSlider::handle:horizontal {"
                  "background: #0078d4;"
                  "border: 1px solid #0078d4;"
                  "width: 14px;"
                  "margin: -3px 0;"
                  "border-radius: 7px;"
                  "}";

    m_positionSlider->setStyleSheet(styleSheet);

    // Update status bar to show frame count
    QString message = QString("Found %1 existing frame(s) at various positions").arg(m_existingFrameTimestamps.size());
    statusBar()->showMessage(message, 3000);
}
