#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QPushButton>
#include <QLabel>
#include <QSlider>
#include <QSpinBox>
#include <QFileDialog>
#include <QMessageBox>
#include <QStatusBar>
#include <QMenuBar>
#include <QAction>
#include <QKeySequence>
#include <QVideoWidget>
#include <QMediaPlayer>
#include <QListWidget>
#include <QSplitter>
#include <QGroupBox>
#include <QProgressBar>
#include <QLineEdit>
#include <QComboBox>
#include <QTimer>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QInputDialog>
#include "Logger.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void openVideo();
    void saveCurrentFrame();
    void playPause();
    void nextFrame();
    void previousFrame();
    void seekToPosition(int position);
    void onPositionChanged(qint64 position);
    void onDurationChanged(qint64 duration);
    void onMediaStatusChanged(QMediaPlayer::MediaStatus status);
    void removeSelectedFrame();
    void exportSelectedFrames();
    void clearSelectedFrames();
    void onFrameStepTimer();

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;

private:
    void setupUI();
    void setupMenuBar();
    void connectSignals();
    void updateControls();
    void addFrameToList(const QString &framePath, qint64 timestamp);
    QString formatTime(qint64 milliseconds);

    // UI Components
    QWidget *m_centralWidget;
    QSplitter *m_mainSplitter;

    // Video section
    QWidget *m_videoWidget;
    QVideoWidget *m_videoDisplay;
    QMediaPlayer *m_mediaPlayer;

    // Controls section
    QWidget *m_controlsWidget;
    QPushButton *m_playPauseBtn;
    QPushButton *m_previousFrameBtn;
    QPushButton *m_nextFrameBtn;
    QPushButton *m_saveFrameBtn;
    QSlider *m_positionSlider;
    QLabel *m_timeLabel;
    QLabel *m_durationLabel;

    // Frame list section
    QWidget *m_frameListWidget;
    QListWidget *m_frameList;
    QPushButton *m_removeFrameBtn;
    QPushButton *m_exportFramesBtn;
    QPushButton *m_clearFramesBtn;
    QLabel *m_frameCountLabel;

    // Settings section
    QGroupBox *m_settingsGroup;
    QLineEdit *m_outputDirEdit;
    QPushButton *m_browseDirBtn;
    QComboBox *m_imageFormatCombo;

    // Menu and actions
    QAction *m_openVideoAction;
    QAction *m_exitAction;
    QAction *m_aboutAction;
    QAction *m_keyboardShortcutsAction;
    QAction *m_logLevelAction;

    // Status
    QProgressBar *m_progressBar;

    // Frame stepping
    QTimer *m_frameStepTimer;
    bool m_isSteppingForward;
    bool m_isSteppingBackward;
    int m_stepInterval;

    // Data
    QString m_currentVideoPath;
    QString m_outputDirectory;
    QStringList m_selectedFrames;
    qint64 m_videoDuration;
    bool m_isPlaying;
};

#endif // MAINWINDOW_H
