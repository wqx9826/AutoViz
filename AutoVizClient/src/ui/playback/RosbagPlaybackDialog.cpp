#include "ui/playback/RosbagPlaybackDialog.h"

#include <QFileDialog>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QSlider>
#include <QVBoxLayout>

#include "core/playback/LocalRosbagPlaybackSource.h"
#include "ui/theme/UiScaleManager.h"

namespace autoviz::ui::playback {
RosbagPlaybackDialog::RosbagPlaybackDialog(autoviz::playback::LocalRosbagPlaybackSource*source,QWidget*parent):QDialog(parent),m_source(source)
{
    setupUi();connect(m_source,&autoviz::playback::LocalRosbagPlaybackSource::validationProgress,this,[this](int p,const QString&t){m_progress->setValue(p);m_status->setText(t);});connect(m_source,&autoviz::playback::LocalRosbagPlaybackSource::bagLoaded,this,[this](const auto&i){updateInfo(i);});connect(m_source,&autoviz::playback::LocalRosbagPlaybackSource::playbackStateChanged,this,&RosbagPlaybackDialog::updateState);connect(m_source,&autoviz::playback::LocalRosbagPlaybackSource::positionChanged,this,[this](qint64 p,qint64 d){if(!m_dragging&&d>0)m_position->setValue(static_cast<int>(100000.0*p/d));m_time->setText(QStringLiteral("%1 / %2").arg(timeText(p),timeText(d)));});connect(m_source,&autoviz::playback::LocalRosbagPlaybackSource::errorOccurred,this,[this](const QString&e){m_status->setText(e);QMessageBox::critical(this,tr("ROS2 Bag 无法播放"),e);});
}
void RosbagPlaybackDialog::setupUi()
{
    const auto&s=theme::UiScaleManager::instance();setWindowTitle(tr("回放数据"));setModal(false);resize(s.scaled(660),s.scaled(570));auto*root=new QVBoxLayout(this);root->setContentsMargins(s.scaled(18),s.scaled(18),s.scaled(18),s.scaled(18));root->setSpacing(s.spacingNormal());auto*title=new QLabel(tr("ROS2 Bag 本地回放"),this);title->setFont(s.font(s.fontSizeTitle(),QFont::Bold));root->addWidget(title);auto*hint=new QLabel(tr("直接读取 robot_ws rosbag2 SQLite 数据，无需启动 ROS2 或 AutoViz Server。"),this);hint->setProperty("class","status-key");root->addWidget(hint);
    auto*loadFrame=new QFrame(this);loadFrame->setObjectName("dialogRow");auto*load=new QHBoxLayout(loadFrame);m_path=new QLineEdit(loadFrame);m_path->setReadOnly(true);m_path->setPlaceholderText(tr("请选择包含 metadata.yaml 的 ROS2 bag 目录"));auto*browse=new QPushButton(tr("加载 ROS2 bag"),loadFrame);load->addWidget(m_path,1);load->addWidget(browse);root->addWidget(loadFrame);connect(browse,&QPushButton::clicked,this,&RosbagPlaybackDialog::chooseBag);
    m_summary=new QLabel(tr("尚未加载 bag"),this);m_summary->setWordWrap(true);root->addWidget(m_summary);m_channels=new QPlainTextEdit(this);m_channels->setReadOnly(true);m_channels->setMaximumHeight(s.scaled(190));m_channels->setPlaceholderText(tr("验证后显示通道和消息数量"));root->addWidget(m_channels,1);m_progress=new QProgressBar(this);m_progress->setRange(0,100);m_progress->setValue(0);root->addWidget(m_progress);m_status=new QLabel(tr("请选择 ROS2 bag"),this);m_status->setWordWrap(true);m_status->setProperty("class","status-key");root->addWidget(m_status);
    auto*progressRow=new QHBoxLayout;m_position=new QSlider(Qt::Horizontal,this);m_position->setRange(0,100000);m_position->setEnabled(false);m_time=new QLabel(QStringLiteral("00:00 / 00:00"),this);progressRow->addWidget(m_position,1);progressRow->addWidget(m_time);root->addLayout(progressRow);connect(m_position,&QSlider::sliderPressed,this,[this]{m_dragging=true;});connect(m_position,&QSlider::sliderReleased,this,[this]{m_dragging=false;const auto i=m_source->bagInfo();m_source->seek(static_cast<qint64>(i.durationNs()*m_position->value()/100000.0));});
    auto*buttons=new QHBoxLayout;m_pause=new QPushButton(tr("暂停"),this);m_stop=new QPushButton(tr("停止"),this);m_start=new QPushButton(tr("开始播放"),this);m_pause->setEnabled(false);m_stop->setEnabled(false);m_start->setEnabled(false);buttons->addWidget(m_pause);buttons->addWidget(m_stop);buttons->addStretch(1);buttons->addWidget(m_start);root->addLayout(buttons);connect(m_start,&QPushButton::clicked,this,[this]{emit playbackStarting();m_source->play();});connect(m_pause,&QPushButton::clicked,this,[this]{m_source->state()==autoviz::playback::PlaybackState::Playing?m_source->pause():m_source->play();});connect(m_stop,&QPushButton::clicked,m_source,&autoviz::playback::LocalRosbagPlaybackSource::stop);
}
void RosbagPlaybackDialog::chooseBag(){const QString dir=QFileDialog::getExistingDirectory(this,tr("选择 ROS2 bag 目录"),m_path->text());if(dir.isEmpty())return;m_path->setText(dir);m_channels->clear();m_progress->setValue(0);m_source->loadAndValidate(dir);}
QString RosbagPlaybackDialog::timeText(qint64 ns){const qint64 sec=qMax<qint64>(0,ns/1000000000LL);return QStringLiteral("%1:%2").arg(sec/60,2,10,QLatin1Char('0')).arg(sec%60,2,10,QLatin1Char('0'));}
void RosbagPlaybackDialog::updateInfo(const autoviz::playback::RosbagInfo&i){m_path->setText(i.directory);m_summary->setText(tr("%1 · sqlite3 · %2 个分片 · %3 MB · 时长 %4").arg(i.name).arg(i.splitCount).arg(i.totalBytes/1048576.0,0,'f',1).arg(timeText(i.durationNs())));QStringList lines;for(const auto&c:i.channels)lines<<QStringLiteral("%1  %2  %3 条").arg(c.present?QStringLiteral("✓"):QStringLiteral("—"),c.topic,QString::number(c.messageCount));if(!i.warnings.isEmpty())lines<<QStringLiteral("\n警告：")+i.warnings.join(QStringLiteral("；"));m_channels->setPlainText(lines.join('\n'));m_position->setEnabled(true);m_start->setEnabled(true);}
void RosbagPlaybackDialog::updateState(autoviz::playback::PlaybackState state,const QString&text){m_status->setText(text);const bool active=state==autoviz::playback::PlaybackState::Playing||state==autoviz::playback::PlaybackState::Paused||state==autoviz::playback::PlaybackState::Completed;m_pause->setEnabled(state==autoviz::playback::PlaybackState::Playing||state==autoviz::playback::PlaybackState::Paused);m_pause->setText(state==autoviz::playback::PlaybackState::Paused?tr("继续"):tr("暂停"));m_stop->setEnabled(active);m_start->setEnabled(state==autoviz::playback::PlaybackState::Ready||state==autoviz::playback::PlaybackState::Stopped||state==autoviz::playback::PlaybackState::Completed);if(state==autoviz::playback::PlaybackState::Completed)m_start->setText(tr("重新播放"));else if(state!=autoviz::playback::PlaybackState::Playing)m_start->setText(tr("开始播放"));}
}  // namespace autoviz::ui::playback
