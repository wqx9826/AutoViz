#include "core/playback/RosbagPlaybackTypes.h"

namespace autoviz::playback {

QString playbackStateText(PlaybackState state)
{
    switch (state) {
    case PlaybackState::Validating: return QStringLiteral("正在验证");
    case PlaybackState::Ready: return QStringLiteral("已就绪");
    case PlaybackState::Playing: return QStringLiteral("正在播放");
    case PlaybackState::Paused: return QStringLiteral("已暂停");
    case PlaybackState::Stopped: return QStringLiteral("已停止");
    case PlaybackState::Completed: return QStringLiteral("播放完成");
    case PlaybackState::Error: return QStringLiteral("错误");
    case PlaybackState::Empty:
    default: return QStringLiteral("未加载");
    }
}

}  // namespace autoviz::playback
