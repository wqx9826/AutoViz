#include "core/playback/LocalRosbagPlaybackSource.h"

#include <algorithm>
#include <limits>

#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QThread>
#include <QTimer>
#include <QVariant>

#include "core/datacenter/DataManager.h"
#include "core/network/ProtocolModelConverter.h"
#include "core/playback/RobotWsCdrDecoder.h"

namespace autoviz::playback {
namespace wire = ::autoviz;

namespace {
constexpr qint64 kTopicTimeoutNs = 5000000000LL;

QString scalar(const QString& yaml, const QString& key)
{
    QRegularExpression re(QStringLiteral("(?m)^\\s*%1:\\s*([^\\r\\n]+)\\s*$").arg(QRegularExpression::escape(key)));
    const auto match=re.match(yaml); return match.hasMatch()?match.captured(1).trimmed():QString{};
}
QStringList relativeFiles(const QString& yaml)
{
    QStringList out; const int start=yaml.indexOf(QStringLiteral("  relative_file_paths:")); if(start<0)return out;
    const int end=yaml.indexOf(QRegularExpression(QStringLiteral("(?m)^  [a-zA-Z_]")),start+24);
    const QString block=yaml.mid(start,end<0?-1:end-start); QRegularExpression re(QStringLiteral("(?m)^    -\\s+(.+?)\\s*$")); auto it=re.globalMatch(block);while(it.hasNext())out<<it.next().captured(1).trimmed();return out;
}
QString connectionName(const void* p){return QStringLiteral("autoviz_bag_%1").arg(reinterpret_cast<quintptr>(p),0,16);}
}

class LocalRosbagPlaybackSource::Worker final : public QObject {
    Q_OBJECT
public:
    Worker(datacenter::DataManager* dm):m_dataManager(dm){m_timer=new QTimer(this);m_timer->setInterval(20);connect(m_timer,&QTimer::timeout,this,&Worker::tick);}
    ~Worker() override { closeDb(); }

public slots:
    void load(const QString& directory)
    {
        m_timer->stop();closeDb();m_state=PlaybackState::Validating;emit stateChanged(m_state,tr("正在验证 ROS2 bag"));emit progress(0,tr("正在读取 metadata.yaml"));
        RosbagInfo info;QString error;if(!inspectMetadata(directory,info,error)||!openDb(info,error)||!validate(info,error)){m_state=PlaybackState::Error;emit stateChanged(m_state,error);emit failed(error);closeDb();return;}
        m_info=info;m_positionNs=0;m_rate=1.0;m_state=PlaybackState::Ready;resetSnapshot();emit progress(100,tr("验证完成"));emit loaded(m_info);emit position(0,m_info.durationNs());emit rateChanged(m_rate);emit stateChanged(m_state,tr("ROS2 bag 已就绪"));
    }
    void play()
    {
        if(!m_info.isValid()||m_state==PlaybackState::Validating||m_state==PlaybackState::Error)return;
        if((m_state==PlaybackState::Paused||m_state==PlaybackState::Ready)&&m_cursorReady){m_clock.restart();m_clockBaseNs=m_positionNs;m_state=PlaybackState::Playing;m_timer->start();emit stateChanged(m_state,tr("正在播放"));return;}
        if(m_state==PlaybackState::Completed){m_positionNs=0;resetSnapshot();}
        else if(m_state==PlaybackState::Ready||m_state==PlaybackState::Stopped)resetSnapshot();
        if(!prepareCursor(m_info.startTimeNs+m_positionNs)){emit failed(m_query.lastError().text());return;}
        m_clock.restart();m_clockBaseNs=m_positionNs;m_state=PlaybackState::Playing;m_timer->start();emit stateChanged(m_state,tr("正在播放"));
    }
    void pause(){if(m_state!=PlaybackState::Playing)return;advanceClock();synchronizeToPosition();m_timer->stop();m_state=PlaybackState::Paused;emit stateChanged(m_state,tr("已暂停"));emit position(m_positionNs,m_info.durationNs());}
    void stop(){m_timer->stop();m_positionNs=0;resetSnapshot();m_state=m_info.isValid()?PlaybackState::Stopped:PlaybackState::Empty;emit position(0,m_info.durationNs());emit stateChanged(m_state,tr("已停止"));}
    void seek(qint64 relative)
    {
        if(!m_info.isValid())return;const bool resume=m_state==PlaybackState::Playing;m_timer->stop();m_positionNs=qBound<qint64>(0,relative,m_info.durationNs());rebuildAt(m_info.startTimeNs+m_positionNs);emit position(m_positionNs,m_info.durationNs());if(resume){m_clock.restart();m_clockBaseNs=m_positionNs;m_timer->start();}
    }
    void setRate(double value){if(m_state==PlaybackState::Playing){advanceClock();synchronizeToPosition();}m_rate=qBound(0.1,value,8.0);m_clock.restart();m_clockBaseNs=m_positionNs;emit rateChanged(m_rate);}

signals:
    void progress(int,const QString&);void loaded(const RosbagInfo&);void stateChanged(PlaybackState,const QString&);void position(qint64,qint64);void rateChanged(double);void failed(const QString&);

private:
    struct PendingMessage {
        qint64 timestampNs = 0;
        QString topic;
        QString type;
        QByteArray payload;
        int splitIndex = 0;
        qint64 messageId = 0;
    };

    struct OrderingKey {
        qint64 timestampNs = 0;
        int splitIndex = 0;
        qint64 messageId = 0;
    };

    struct CenterTurnTransition {
        OrderingKey key;
        bool active = false;
    };

    bool inspectMetadata(const QString& directory,RosbagInfo& info,QString& error)
    {
        const QDir dir(directory);QFile file(dir.filePath("metadata.yaml"));if(!dir.exists()||!file.open(QIODevice::ReadOnly|QIODevice::Text)){error=tr("目录中缺少可读的 metadata.yaml");return false;}const QString yaml=QString::fromUtf8(file.readAll());
        bool ok=false;info.metadataVersion=scalar(yaml,"version").toInt(&ok);if(!ok||info.metadataVersion!=5){error=tr("仅支持 rosbag2 metadata version 5");return false;}info.storageId=scalar(yaml,"storage_identifier");if(info.storageId!="sqlite3"){error=tr("仅支持 sqlite3 存储，当前为 %1").arg(info.storageId);return false;}
        m_files=relativeFiles(yaml);if(m_files.isEmpty())m_files=dir.entryList({"*.db3"},QDir::Files,QDir::Name);if(m_files.isEmpty()){error=tr("metadata 未列出任何 DB3 分片");return false;}
        info.directory=dir.absolutePath();info.name=dir.dirName();info.splitCount=m_files.size();for(const auto& f:m_files){QFileInfo fi(dir.filePath(f));if(!fi.exists()||!fi.isReadable()){error=tr("DB3 分片缺失或不可读：%1").arg(f);return false;}info.totalBytes+=fi.size();}return true;
    }
    bool openDb(const RosbagInfo& info,QString& error)
    {
        m_connection=connectionName(this);m_db=QSqlDatabase::addDatabase("QSQLITE",m_connection);m_db.setDatabaseName(QDir(info.directory).filePath(m_files.first()));if(!m_db.open()){error=tr("无法打开 DB3：%1").arg(m_db.lastError().text());return false;}
        QSqlQuery q(m_db);for(int i=1;i<m_files.size();++i){const QString alias=QStringLiteral("bag%1").arg(i);const QString path=QDir(info.directory).filePath(m_files[i]);q.prepare(QStringLiteral("ATTACH DATABASE ? AS %1").arg(alias));q.addBindValue(path);if(!q.exec()){error=tr("无法附加分片 %1：%2").arg(m_files[i],q.lastError().text());return false;}}return true;
    }
    QString unionSql(const QString& columns,const QString& where={}) const
    {
        QStringList parts;for(int i=0;i<m_files.size();++i){const QString schema=i?QStringLiteral("bag%1.").arg(i):QString{};parts<<QStringLiteral("SELECT %1 FROM %2messages m JOIN %2topics t ON t.id=m.topic_id %3").arg(columns,schema,where);}return parts.join(" UNION ALL ");
    }
    QString messageUnionSql(const QString& where={}) const
    {
        QStringList parts;
        for(int i=0;i<m_files.size();++i){
            const QString schema=i?QStringLiteral("bag%1.").arg(i):QString{};
            parts<<QStringLiteral("SELECT m.timestamp timestamp,t.name name,t.type type,m.data data,%1 split_index,m.id message_id FROM %2messages m JOIN %2topics t ON t.id=m.topic_id %3").arg(i).arg(schema,where);
        }
        return parts.join(" UNION ALL ");
    }
    QString supportedTopicListSql() const
    {
        QStringList quoted;
        for (const auto& topic : RobotWsCdrDecoder::supportedTopics()) {
            quoted << QStringLiteral("'%1'").arg(topic);
        }
        return QStringLiteral("(%1)").arg(quoted.join(','));
    }
    bool validate(RosbagInfo& info,QString& error)
    {
        QSqlQuery q(m_db);for(int i=0;i<m_files.size();++i){const QString schema=i?QStringLiteral("bag%1.").arg(i):QString{};if(!q.exec(QStringLiteral("PRAGMA %1quick_check").arg(schema))||!q.next()||q.value(0).toString()!="ok"){error=tr("SQLite 完整性检查失败：分片 %1").arg(i+1);return false;}}
        QMap<QString,RosbagChannelInfo> channels;for(const auto&t:RobotWsCdrDecoder::supportedTopics()){RosbagChannelInfo c;c.topic=t;c.type=RobotWsCdrDecoder::expectedType(t);c.supported=true;channels.insert(t,c);}
        const QString sql=QStringLiteral("SELECT name,type,serialization_format,COUNT(*) FROM (%1) GROUP BY name,type,serialization_format").arg(unionSql("t.name name,t.type type,t.serialization_format serialization_format,m.id mid"));if(!q.exec(sql)){error=q.lastError().text();return false;}
        qint64 supportedCount=0;while(q.next()){const QString topic=q.value(0).toString(),type=q.value(1).toString(),format=q.value(2).toString();const qint64 count=q.value(3).toLongLong();if(!RobotWsCdrDecoder::expectedType(topic).isEmpty()&&type!=RobotWsCdrDecoder::expectedType(topic)){error=tr("%1 的消息类型不匹配：%2").arg(topic,type);return false;}if(RobotWsCdrDecoder::isSupported(topic,type)){if(format!="cdr"){error=tr("%1 不是 CDR 序列化").arg(topic);return false;}auto c=channels.value(topic);c.present=count>0;c.messageCount+=count;channels[topic]=c;supportedCount+=count;}}
        if(supportedCount==0){error=tr("bag 中没有 AutoViz 支持的数据通道");return false;}
        const QString timeRangeSql = QStringLiteral("SELECT MIN(timestamp),MAX(timestamp) FROM (%1)")
            .arg(unionSql("m.timestamp timestamp", QStringLiteral("WHERE t.name IN %1").arg(supportedTopicListSql())));
        if (!q.exec(timeRangeSql) || !q.next()) { error=tr("无法读取 bag 时间范围");return false; }
        info.startTimeNs=q.value(0).toLongLong();info.endTimeNs=q.value(1).toLongLong();
        info.channels=channels.values().toVector();for(const auto&c:info.channels)if(!c.present&&c.topic!="/targets/final_objects")info.warnings<<tr("缺少通道 %1").arg(c.topic);
        emit progress(5,tr("正在完整解码 %1 条受支持消息").arg(supportedCount));
        m_centerTurnTransitions.clear();
        bool hasCenterTurnState=false;
        bool centerTurnState=false;
        const QString decodeSql=messageUnionSql(QStringLiteral("WHERE t.name IN %1").arg(supportedTopicListSql()))+" ORDER BY timestamp,split_index,message_id";q.setForwardOnly(true);if(!q.exec(decodeSql)){error=q.lastError().text();return false;}qint64 n=0;while(q.next()){wire::VisualizationSnapshot scratch;QString detail;if(!RobotWsCdrDecoder::decode(q.value(1).toString(),q.value(2).toString(),q.value(3).toByteArray(),q.value(0).toULongLong(),&scratch,&detail)){error=tr("消息解码失败：%1，第 %2 条：%3").arg(q.value(1).toString()).arg(n+1).arg(detail);return false;}if(q.value(1).toString()==QStringLiteral("/chassis_command")&&scratch.has_control_command()){const bool active=scratch.control_command().maneuver()==wire::ControlCommand::MANEUVER_YAW_IN_PLACE;if(!hasCenterTurnState||active!=centerTurnState){m_centerTurnTransitions.push_back({{q.value(0).toLongLong(),q.value(4).toInt(),q.value(5).toLongLong()},active});hasCenterTurnState=true;centerTurnState=active;}}++n;if(n%10000==0)emit progress(5+static_cast<int>(94.0*n/supportedCount),tr("已验证 %1 / %2 条消息").arg(n).arg(supportedCount));}return true;
    }
    static bool keyLess(const OrderingKey& left, const OrderingKey& right)
    {
        if (left.timestampNs != right.timestampNs) return left.timestampNs < right.timestampNs;
        if (left.splitIndex != right.splitIndex) return left.splitIndex < right.splitIndex;
        return left.messageId < right.messageId;
    }
    static OrderingKey keyFor(const PendingMessage& message)
    {
        return {message.timestampNs, message.splitIndex, message.messageId};
    }
    static bool isSequentialTopic(const QString& topic)
    {
        return topic == QStringLiteral("/chassis_command")
               || topic == QStringLiteral("/chassis_states")
               || topic == QStringLiteral("/system_run_states")
               || topic == QStringLiteral("/global_path")
               || topic == QStringLiteral("/local_path");
    }
    void resetSnapshot()
    {
        m_snapshot.Clear();m_actionDiagnostics.clear();m_snapshotSequence=0;m_snapshot.set_session_id(QStringLiteral("local:%1").arg(m_info.name).toStdString());auto*src=m_snapshot.mutable_source();src->set_source_id(m_info.name.toStdString());src->set_communication_type("ROS2 Bag");src->set_communication_version("Humble");src->set_description("Local robot_ws rosbag2 sqlite3 playback");src->add_capability(wire::CAPABILITY_COMMON_PLANNING_CONTROL);src->add_capability(wire::CAPABILITY_VERTICAL_MOTION);src->add_capability(wire::CAPABILITY_UNDERWATER_SYSTEM);src->add_capability(wire::CAPABILITY_PLATFORM_DIAGNOSTICS);m_counts.clear();m_lastTimes.clear();m_centerTurnActive=false;m_hasPending=false;m_cursorReady=false;m_cursorExhausted=false;if(m_dataManager)m_dataManager->resetVisualizationData(datacenter::VisualizationInputSource::Ros2Bag);
    }
    void clearTopicSnapshot(const QString& topic)
    {
        if(topic==QStringLiteral("/location"))m_snapshot.clear_vehicle_state();
        else if(topic==QStringLiteral("/targets/final_objects"))m_snapshot.clear_obstacles();
        else if(topic==QStringLiteral("/chassis_command")){m_snapshot.clear_control_command();m_centerTurnActive=false;}
        else if(topic==QStringLiteral("/chassis_states"))m_snapshot.clear_chassis_state();
        else if(topic==QStringLiteral("/system_run_states"))m_snapshot.clear_action_state();
        else if(topic==QStringLiteral("/task_params"))m_snapshot.clear_task_state();
        else if(topic==QStringLiteral("/local_path"))m_snapshot.clear_local_trajectory();
        else if(topic==QStringLiteral("/global_path"))m_snapshot.clear_global_trajectory();
    }
    void updateRuntime(qint64 now)
    {
        auto*r=m_snapshot.mutable_runtime_state();r->clear_topic();
        for(const auto&c:m_info.channels){
            if(!c.present)continue;
            const qint64 last=m_lastTimes.value(c.topic);
            const bool timedOut=last<=0||now-last>kTopicTimeoutNs;
            if(timedOut)clearTopicSnapshot(c.topic);
            auto*t=r->add_topic();t->set_name(c.topic.toStdString());t->set_type(c.type.toStdString());t->set_data_kind(RobotWsCdrDecoder::dataKind(c.topic));t->set_message_count(m_counts.value(c.topic));t->set_last_update_time_ns(last);t->set_timeout_ns(kTopicTimeoutNs);t->set_timed_out(timedOut);
        }
    }
    bool prepareCursor(qint64 absolute)
    {
        const QString sql=messageUnionSql(QStringLiteral("WHERE m.timestamp >= ? AND t.name IN %1").arg(supportedTopicListSql()))+" ORDER BY timestamp,split_index,message_id";m_query=QSqlQuery(m_db);m_query.setForwardOnly(true);m_query.prepare(sql);for(int i=0;i<m_files.size();++i)m_query.addBindValue(absolute);m_hasPending=false;m_cursorExhausted=false;m_cursorReady=m_query.exec();return m_cursorReady;
    }
    void rebuildAt(qint64 absolute)
    {
        resetSnapshot();
        const QString countSql=QStringLiteral("SELECT name,COUNT(*),MAX(timestamp) FROM (%1) GROUP BY name").arg(unionSql("m.timestamp timestamp,t.name name",QStringLiteral("WHERE m.timestamp <= ? AND t.name IN %1").arg(supportedTopicListSql())));
        QSqlQuery counts(m_db);counts.prepare(countSql);for(int i=0;i<m_files.size();++i)counts.addBindValue(absolute);if(counts.exec())while(counts.next()){m_counts[counts.value(0).toString()]=counts.value(1).toULongLong();m_lastTimes[counts.value(0).toString()]=counts.value(2).toLongLong();}

        const OrderingKey targetKey{absolute,std::numeric_limits<int>::max(),std::numeric_limits<qint64>::max()};
        bool centerAtTarget=false;
        bool hasTransition=false;
        bool hasExitBoundary=false;
        OrderingKey exitBoundary;
        for(const auto&transition:m_centerTurnTransitions){
            if(keyLess(targetKey,transition.key))break;
            if(hasTransition&&centerAtTarget&&!transition.active){exitBoundary=transition.key;hasExitBoundary=true;}
            centerAtTarget=transition.active;
            hasTransition=true;
        }

        QVector<PendingMessage> latest;
        for(const auto& topic:RobotWsCdrDecoder::supportedTopics()){
            const QString sql=messageUnionSql("WHERE m.timestamp <= ? AND t.name = ?")+" ORDER BY timestamp DESC,split_index DESC,message_id DESC LIMIT 1";
            QSqlQuery q(m_db);q.prepare(sql);for(int i=0;i<m_files.size();++i){q.addBindValue(absolute);q.addBindValue(topic);}if(!q.exec()||!q.next())continue;
            const PendingMessage message=messageFromRow(q);
            latest.push_back(message);
        }

        bool commandTimedOut=false;
        for(const auto&message:latest){
            if(message.topic!=QStringLiteral("/chassis_command"))continue;
            commandTimedOut=absolute-message.timestampNs>kTopicTimeoutNs;
            if(commandTimedOut&&centerAtTarget){
                centerAtTarget=false;
                exitBoundary={message.timestampNs+kTopicTimeoutNs,
                              std::numeric_limits<int>::max(),
                              std::numeric_limits<qint64>::max()};
                hasExitBoundary=true;
            }
            break;
        }
        latest.erase(std::remove_if(latest.begin(),latest.end(),[&](const PendingMessage&message){
            if(commandTimedOut&&message.topic==QStringLiteral("/chassis_command"))return true;
            const bool path=message.topic==QStringLiteral("/global_path")
                            ||message.topic==QStringLiteral("/local_path");
            return path&&(centerAtTarget
                          ||(hasExitBoundary&&!keyLess(exitBoundary,keyFor(message))));
        }),latest.end());
        std::sort(latest.begin(),latest.end(),[](const PendingMessage&left,const PendingMessage&right){return keyLess(keyFor(left),keyFor(right));});
        for(const auto&message:latest)applyMessage(message,m_counts.value(message.topic));
        publish(absolute);
        prepareCursor(absolute+1);
    }
    PendingMessage messageFromRow(const QSqlQuery& q) const
    {
        PendingMessage message;
        message.timestampNs = q.value(0).toLongLong();
        message.topic = q.value(1).toString();
        message.type = q.value(2).toString();
        message.payload = q.value(3).toByteArray();
        message.splitIndex = q.value(4).toInt();
        message.messageId = q.value(5).toLongLong();
        return message;
    }
    bool decodeMessage(const PendingMessage& message)
    {
        QString error;
        return RobotWsCdrDecoder::decode(message.topic,
                                         message.type,
                                         message.payload,
                                         message.timestampNs,
                                         &m_snapshot,
                                         &error,
                                         &m_actionDiagnostics);
    }
    void stampSequence(const QString& topic, quint64 sequence, int firstEvent)
    {
        if(topic=="/chassis_command"&&m_snapshot.has_control_command())m_snapshot.mutable_control_command()->mutable_header()->set_sequence(sequence);
        else if(topic=="/chassis_states"&&m_snapshot.has_chassis_state())m_snapshot.mutable_chassis_state()->mutable_header()->set_sequence(sequence);
        else if(topic=="/system_run_states"&&m_snapshot.has_action_state())m_snapshot.mutable_action_state()->mutable_header()->set_sequence(sequence);
        for(int index=firstEvent;index<m_snapshot.control_state_event_size();++index)m_snapshot.mutable_control_state_event(index)->mutable_header()->set_sequence(sequence);
    }
    bool applyMessage(const PendingMessage& message, quint64 sequence)
    {
        const bool globalPath=message.topic==QStringLiteral("/global_path");
        const bool localPath=message.topic==QStringLiteral("/local_path");
        if(m_centerTurnActive&&(globalPath||localPath)){
            if(globalPath)m_snapshot.clear_global_trajectory();else m_snapshot.clear_local_trajectory();
            return true;
        }
        const int firstEvent=m_snapshot.control_state_event_size();
        if(!decodeMessage(message))return false;
        stampSequence(message.topic,sequence,firstEvent);
        if(message.topic==QStringLiteral("/chassis_command")&&m_snapshot.has_control_command()){
            const bool center=m_snapshot.control_command().maneuver()==wire::ControlCommand::MANEUVER_YAW_IN_PLACE;
            if(center&&!m_centerTurnActive){m_snapshot.clear_global_trajectory();m_snapshot.clear_local_trajectory();}
            m_centerTurnActive=center;
        }
        return true;
    }
    void publish(qint64 now){updateRuntime(now);m_snapshot.set_sequence(++m_snapshotSequence);m_snapshot.set_server_time_ns(now);if(m_dataManager)m_dataManager->replaceVisualizationSnapshot(network::ProtocolModelConverter::toModelSnapshot(m_snapshot),datacenter::VisualizationInputSource::Ros2Bag);}
    void advanceClock(){if(m_state==PlaybackState::Playing&&m_clock.isValid())m_positionNs=qMin(m_info.durationNs(),m_clockBaseNs+static_cast<qint64>(m_clock.elapsed()*1000000.0*m_rate));}
    struct ProcessResult {
        bool reachedTarget = true;
        qint64 effectiveTimeNs = 0;
    };
    ProcessResult processUntil(qint64 target)
    {
        QElapsedTimer budget;
        budget.start();
        QMap<QString, PendingMessage> latestByTopic;
        qint64 lastProcessedNs = target;
        int processedRows = 0;
        bool reachedTarget = true;

        while (true) {
            if (!m_hasPending) {
                if (!m_query.next()) {m_cursorExhausted=true;break;}
                m_pendingTs = m_query.value(0).toLongLong();
                m_hasPending = true;
            }
            if (m_pendingTs > target) break;

            const PendingMessage message = messageFromRow(m_query);
            m_counts[message.topic]++;
            m_lastTimes[message.topic] = message.timestampNs;
            if(isSequentialTopic(message.topic))applyMessage(message,m_counts.value(message.topic));
            else latestByTopic[message.topic] = message;
            lastProcessedNs = message.timestampNs;
            m_hasPending = false;
            ++processedRows;

            // 单次工作必须可抢占，保证暂停、停止和调速请求不会饿死在 worker 队列中。
            if (processedRows >= 2000 || budget.elapsed() >= 4) {
                reachedTarget = false;
                break;
            }
        }

        QVector<PendingMessage> latestMessages;
        latestMessages.reserve(latestByTopic.size());
        for (auto it = latestByTopic.cbegin(); it != latestByTopic.cend(); ++it) {
            latestMessages.push_back(it.value());
        }
        std::sort(latestMessages.begin(), latestMessages.end(), [](const PendingMessage& left, const PendingMessage& right) {
            return left.timestampNs < right.timestampNs;
        });
        for (const auto& message : latestMessages) {
            applyMessage(message,m_counts.value(message.topic));
        }
        const qint64 effectiveTimeNs=reachedTarget?target:lastProcessedNs;
        publish(effectiveTimeNs);
        return {reachedTarget,effectiveTimeNs};
    }
    void synchronizeToPosition()
    {
        if(!m_cursorReady)return;
        const auto result=processUntil(m_info.startTimeNs+m_positionNs);
        if(!result.reachedTarget){
            m_positionNs=qBound<qint64>(0,result.effectiveTimeNs-m_info.startTimeNs,m_info.durationNs());
            m_clockBaseNs=m_positionNs;
            m_clock.restart();
        }
    }
    void tick()
    {
        advanceClock();
        synchronizeToPosition();

        emit position(m_positionNs,m_info.durationNs());
        if(m_positionNs>=m_info.durationNs()&&!m_hasPending&&m_cursorExhausted){m_timer->stop();m_state=PlaybackState::Completed;emit stateChanged(m_state,tr("播放完成"));}
    }
    void closeDb(){m_query=QSqlQuery();if(m_db.isValid()){m_db.close();m_db=QSqlDatabase();}if(!m_connection.isEmpty()){QSqlDatabase::removeDatabase(m_connection);m_connection.clear();}}

    datacenter::DataManager*m_dataManager=nullptr;QTimer* m_timer=nullptr;QElapsedTimer m_clock;QSqlDatabase m_db;QSqlQuery m_query;QString m_connection;QStringList m_files;RosbagInfo m_info;PlaybackState m_state=PlaybackState::Empty;wire::VisualizationSnapshot m_snapshot;RobotWsCdrDecoder::ActionDiagnosticCache m_actionDiagnostics;QMap<QString,quint64>m_counts;QMap<QString,qint64>m_lastTimes;QVector<CenterTurnTransition>m_centerTurnTransitions;quint64 m_snapshotSequence=0;qint64 m_positionNs=0,m_clockBaseNs=0,m_pendingTs=0;double m_rate=1.0;bool m_hasPending=false,m_cursorReady=false,m_cursorExhausted=false,m_centerTurnActive=false;
};

LocalRosbagPlaybackSource::LocalRosbagPlaybackSource(datacenter::DataManager*dm,QObject*parent):QObject(parent)
{
    qRegisterMetaType<PlaybackState>("PlaybackState");qRegisterMetaType<RosbagInfo>("RosbagInfo");m_thread=new QThread(this);m_worker=new Worker(dm);m_worker->moveToThread(m_thread);connect(m_thread,&QThread::finished,m_worker,&QObject::deleteLater);connect(this,&LocalRosbagPlaybackSource::requestLoad,m_worker,&Worker::load);connect(this,&LocalRosbagPlaybackSource::requestPlay,m_worker,&Worker::play);connect(this,&LocalRosbagPlaybackSource::requestPause,m_worker,&Worker::pause);connect(this,&LocalRosbagPlaybackSource::requestStop,m_worker,&Worker::stop);connect(this,&LocalRosbagPlaybackSource::requestSeek,m_worker,&Worker::seek);connect(this,&LocalRosbagPlaybackSource::requestRate,m_worker,&Worker::setRate);
    connect(m_worker,&Worker::progress,this,&LocalRosbagPlaybackSource::validationProgress);connect(m_worker,&Worker::loaded,this,[this](const RosbagInfo&i){m_info=i;emit bagLoaded(i);});connect(m_worker,&Worker::stateChanged,this,[this](PlaybackState s,const QString&t){m_state=s;emit playbackStateChanged(s,t);});connect(m_worker,&Worker::position,this,&LocalRosbagPlaybackSource::positionChanged);connect(m_worker,&Worker::rateChanged,this,[this](double r){m_rate=r;emit playbackRateChanged(r);});connect(m_worker,&Worker::failed,this,&LocalRosbagPlaybackSource::errorOccurred);m_thread->start();
}
LocalRosbagPlaybackSource::~LocalRosbagPlaybackSource(){m_thread->quit();m_thread->wait();}
void LocalRosbagPlaybackSource::loadAndValidate(const QString&p){emit requestLoad(p);}void LocalRosbagPlaybackSource::play(){emit requestPlay();}void LocalRosbagPlaybackSource::pause(){emit requestPause();}void LocalRosbagPlaybackSource::stop(){emit requestStop();}void LocalRosbagPlaybackSource::seek(qint64 n){emit requestSeek(n);}void LocalRosbagPlaybackSource::setPlaybackRate(double r){emit requestRate(r);}

}  // namespace autoviz::playback

#include "LocalRosbagPlaybackSource.moc"
