#pragma once

#include <QDialog>
#include "core/playback/RosbagPlaybackTypes.h"

class QLabel;class QLineEdit;class QPlainTextEdit;class QProgressBar;class QPushButton;class QSlider;
namespace autoviz::playback { class LocalRosbagPlaybackSource; }

namespace autoviz::ui::playback {
class RosbagPlaybackDialog final : public QDialog {
    Q_OBJECT
public:
    explicit RosbagPlaybackDialog(autoviz::playback::LocalRosbagPlaybackSource* source,QWidget*parent=nullptr);
signals:void playbackLoading();void playbackStarting();
private:
    void setupUi();void chooseBag();void updateInfo(const autoviz::playback::RosbagInfo& info);void updateState(autoviz::playback::PlaybackState state,const QString&text);static QString timeText(qint64 ns);
    autoviz::playback::LocalRosbagPlaybackSource*m_source=nullptr;QLineEdit*m_path=nullptr;QLabel*m_summary=nullptr;QPlainTextEdit*m_channels=nullptr;QProgressBar*m_progress=nullptr;QLabel*m_status=nullptr;QSlider*m_position=nullptr;QLabel*m_time=nullptr;QPushButton*m_pause=nullptr;QPushButton*m_stop=nullptr;QPushButton*m_start=nullptr;bool m_dragging=false;
};
}  // namespace autoviz::ui::playback
