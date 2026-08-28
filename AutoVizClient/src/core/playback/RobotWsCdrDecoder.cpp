#include "core/playback/RobotWsCdrDecoder.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <QMap>
#include <QSet>
#include <QStringList>

namespace autoviz::playback {
namespace wire = ::autoviz;

namespace {
constexpr quint32 kMaxSequenceElements = 1000000U;
constexpr quint32 kMaxStringBytes = 16U * 1024U * 1024U;
constexpr double kDegreesToRadians = 0.017453292519943295769;

class CdrReader {
public:
    explicit CdrReader(const QByteArray& data) : m_data(data) {}

    bool begin(QString* error)
    {
        if (m_data.size() < 4) return fail(QStringLiteral("CDR 封装头不足 4 字节"), error);
        const auto* b = reinterpret_cast<const unsigned char*>(m_data.constData());
        const quint16 kind = static_cast<quint16>((b[0] << 8U) | b[1]);
        if (kind == 0x0001U || kind == 0x0003U) m_littleEndian = true;
        else if (kind == 0x0000U || kind == 0x0002U) m_littleEndian = false;
        else return fail(QStringLiteral("不支持的 CDR encapsulation: 0x%1").arg(kind, 4, 16, QLatin1Char('0')), error);
        m_pos = 4;
        return true;
    }

    bool u8(quint8& v, QString* e) { return scalar(v, 1, e); }
    bool i8(qint8& v, QString* e) { return scalar(v, 1, e); }
    bool boolean(bool& v, QString* e) { quint8 x=0; if (!u8(x,e)) return false; if (x>1) return fail(QStringLiteral("非法 bool 值 %1").arg(x),e); v=x!=0; return true; }
    bool u16(quint16& v, QString* e) { return scalar(v, 2, e); }
    bool i16(qint16& v, QString* e) { return scalar(v, 2, e); }
    bool u32(quint32& v, QString* e) { return scalar(v, 4, e); }
    bool i32(qint32& v, QString* e) { return scalar(v, 4, e); }
    bool u64(quint64& v, QString* e) { return scalar(v, 8, e); }
    bool i64(qint64& v, QString* e) { return scalar(v, 8, e); }
    bool f32(float& v, QString* e) { if (!scalar(v,4,e)) return false; return std::isfinite(v) || fail(QStringLiteral("浮点字段不是有限值"),e); }
    bool f64(double& v, QString* e) { if (!scalar(v,8,e)) return false; return std::isfinite(v) || fail(QStringLiteral("浮点字段不是有限值"),e); }
    bool f32Raw(float& v, QString* e) { return scalar(v, 4, e); }
    bool f64Raw(double& v, QString* e) { return scalar(v, 8, e); }

    bool string(QString& value, QString* error)
    {
        quint32 size = 0;
        if (!u32(size, error)) return false;
        if (size == 0 || size > kMaxStringBytes || size > static_cast<quint32>(m_data.size() - m_pos))
            return fail(QStringLiteral("非法 CDR 字符串长度 %1").arg(size), error);
        const char* bytes = m_data.constData() + m_pos;
        if (bytes[size - 1] != '\0') return fail(QStringLiteral("CDR 字符串缺少结尾 NUL"), error);
        value = QString::fromUtf8(bytes, static_cast<int>(size - 1));
        m_pos += static_cast<int>(size);
        return true;
    }

    bool sequenceSize(quint32& size, QString* error)
    {
        if (!u32(size, error)) return false;
        return size <= kMaxSequenceElements || fail(QStringLiteral("序列元素过多: %1").arg(size), error);
    }
    bool finished(QString* error) const
    {
        if (m_pos == m_data.size()) return true;
        for (int i=m_pos; i<m_data.size(); ++i) if (m_data.at(i) != 0)
            return failConst(QStringLiteral("CDR 尾部仍有 %1 字节未解析").arg(m_data.size()-m_pos),error);
        return (m_data.size()-m_pos) <= 7 || failConst(QStringLiteral("CDR 尾部填充异常"),error);
    }

private:
    template <typename T> bool scalar(T& value, int alignment, QString* error)
    {
        // ROS 2 CDR 的对齐原点位于 4 字节 encapsulation 之后，而不是整个
        // SQLite BLOB 的起点。
        const int relative = m_pos - 4;
        const int aligned = 4 + ((relative + alignment - 1) & ~(alignment - 1));
        if (aligned < m_pos || aligned + static_cast<int>(sizeof(T)) > m_data.size())
            return fail(QStringLiteral("CDR 数据在偏移 %1 处被截断").arg(m_pos), error);
        unsigned char bytes[sizeof(T)];
        std::memcpy(bytes, m_data.constData()+aligned, sizeof(T));
#if Q_BYTE_ORDER == Q_LITTLE_ENDIAN
        const bool nativeLittle = true;
#else
        const bool nativeLittle = false;
#endif
        if (nativeLittle != m_littleEndian) std::reverse(bytes, bytes+sizeof(T));
        std::memcpy(&value, bytes, sizeof(T));
        m_pos = aligned + static_cast<int>(sizeof(T));
        return true;
    }
    bool fail(const QString& text, QString* error) { if (error) *error=text; return false; }
    static bool failConst(const QString& text, QString* error) { if (error) *error=text; return false; }
    const QByteArray& m_data;
    int m_pos = 0;
    bool m_littleEndian = true;
};

bool readHeader(CdrReader& r, qint32& sec, quint32& nsec, QString& frame, QString* e)
{ return r.i32(sec,e) && r.u32(nsec,e) && r.string(frame,e); }

quint64 headerNs(qint32 sec, quint32 nsec, quint64 fallback)
{
    if (sec <= 0 && nsec == 0) return fallback;
    return static_cast<quint64>(std::max<qint64>(0,sec))*1000000000ULL+nsec;
}

void fillHeader(wire::Header* h, quint64 source, quint64 receive, const char* module, const QString& frame={})
{
    if (source > 0) h->set_source_time_ns(source); if (receive > 0) h->set_server_receive_time_ns(receive); h->set_module_name(module);
    if (!frame.isEmpty()) h->set_frame_id(frame.toStdString());
}

wire::BuoyancyCommand buoyancy(quint8 v)
{
    return v==0?wire::BUOYANCY_COMMAND_STOP:v==1?wire::BUOYANCY_COMMAND_FILL:v==2?wire::BUOYANCY_COMMAND_DRAIN:wire::BUOYANCY_COMMAND_UNKNOWN;
}

wire::WaterTankState tankState(quint8 v)
{
    switch(v) { case 0:return wire::WATER_TANK_STATE_IDLE; case 1:return wire::WATER_TANK_STATE_FILLING;
    case 2:return wire::WATER_TANK_STATE_DRAINING; case 3:return wire::WATER_TANK_STATE_MANUAL_OVERRIDE;
    case 4:return wire::WATER_TANK_STATE_FAULT; case 5:return wire::WATER_TANK_STATE_FILL_DONE;
    case 6:return wire::WATER_TANK_STATE_DRAIN_DONE; default:return wire::WATER_TANK_STATE_UNKNOWN; }
}

double yaw(double x,double y,double z,double w)
{ return std::atan2(2.0*(w*z+x*y),1.0-2.0*(y*y+z*z)); }

double trajectoryLength(const wire::Trajectory& t)
{
    double sum=0; for(int i=1;i<t.point_size();++i){ const auto&a=t.point(i-1).path_point().position(); const auto&b=t.point(i).path_point().position(); sum+=std::hypot(b.x_m()-a.x_m(),b.y_m()-a.y_m()); } return sum;
}

wire::VerticalControlMode verticalMode(quint8 navigationMode)
{ if(navigationMode==1)return wire::VERTICAL_CONTROL_MODE_DEPTH_HOLD; if(navigationMode==2)return wire::VERTICAL_CONTROL_MODE_HEIGHT_HOLD; return wire::VERTICAL_CONTROL_MODE_NONE; }

wire::VerticalControlMode actionVerticalMode(quint8 owner, quint8 chassisMode, quint8 navigationMode)
{ Q_UNUSED(navigationMode); if(owner==2&&chassisMode==1)return wire::VERTICAL_CONTROL_MODE_DEPTH_HOLD; if(owner==2&&chassisMode==2)return wire::VERTICAL_CONTROL_MODE_LANDING; return wire::VERTICAL_CONTROL_MODE_NONE; }

QString actionName(quint8 owner)
{ return owner==1?QStringLiteral("custom_msgs/action/Move"):owner==2?QStringLiteral("custom_msgs/action/DepthCommand"):QString{}; }

bool readUuid(CdrReader& r, QString& value, QString* error)
{
    QString result;
    result.reserve(32);
    for (int index=0; index<16; ++index) {
        quint8 byte=0;
        if (!r.u8(byte,error)) return false;
        result += QStringLiteral("%1").arg(byte,2,16,QLatin1Char('0'));
    }
    value=result;
    return true;
}

// 把 canonical 32 位 hex 转成 robot_ws 用 %x 逐字节格式化会得到的 lossy 形式：
// 每个字节若有前导零则丢弃。只应传入 canonical（32 位）字符串。
QString lossyUuidHex(const QString& canonical)
{
    QString result;
    result.reserve(32);
    for (int index = 0; index + 1 < canonical.size(); index += 2) {
        if (canonical.at(index) != QLatin1Char('0')) {
            result.append(canonical.at(index));
        }
        result.append(canonical.at(index + 1));
    }
    return result;
}

void mergeActionDiagnostic(const QString& goal, wire::ActionState* action,
                           const RobotWsCdrDecoder::ActionDiagnosticCache* diagnostics)
{
    if (action == nullptr || diagnostics == nullptr || goal.isEmpty()) return;
    // goal 可能是丢前导零的 lossy hex，逐项比对以命中 canonical 缓存键。
    for (auto it = diagnostics->cbegin(); it != diagnostics->cend(); ++it) {
        if (!RobotWsCdrDecoder::sameGoalUuid(goal, it.key())) continue;
        if (it->hasNativeStatus) {
            action->set_native_status(it->nativeStatus);
            action->set_native_status_time_ns(it->nativeStatusTimeNs);
        }
        if (it->hasFeedbackProgress) {
            action->set_feedback_progress(it->feedbackProgress);
            action->set_feedback_time_ns(it->feedbackTimeNs);
        }
        return;
    }
}

bool updateActionFeedback(CdrReader& r, quint64 ts, wire::VisualizationSnapshot* snapshot, QString* error,
                          RobotWsCdrDecoder::ActionDiagnosticCache* diagnostics)
{
    QString goal; double progress=0.0;
    if(!readUuid(r,goal,error)||!r.f64(progress,error))return false;
    if (diagnostics != nullptr) {
        auto& diagnostic = (*diagnostics)[goal];
        diagnostic.hasFeedbackProgress = true;
        diagnostic.feedbackProgress = progress;
        diagnostic.feedbackTimeNs = ts;
    }
    if(snapshot->has_action_state()
       && RobotWsCdrDecoder::sameGoalUuid(QString::fromStdString(snapshot->action_state().goal_id()), goal)) {
        auto* action=snapshot->mutable_action_state(); action->set_feedback_progress(progress); action->set_feedback_time_ns(ts);
    }
    return true;
}

bool updateActionStatus(CdrReader& r, quint64 ts, wire::VisualizationSnapshot* snapshot, QString* error,
                        RobotWsCdrDecoder::ActionDiagnosticCache* diagnostics)
{
    // Humble 的 action_msgs/GoalStatusArray 只有 status_list；其每个元素
    // 自己携带 GoalInfo（UUID + stamp），没有 std_msgs/Header。
    quint32 count=0;
    if(!r.sequenceSize(count,error))return false;
    const QString current=snapshot->has_action_state()?QString::fromStdString(snapshot->action_state().goal_id()):QString{};
    for(quint32 index=0;index<count;++index) {
        QString goal; qint32 stampSec=0; quint32 stampNs=0; qint8 status=0;
        if(!readUuid(r,goal,error)||!r.i32(stampSec,error)||!r.u32(stampNs,error)||!r.i8(status,error))return false;
        if (diagnostics != nullptr) {
            auto& diagnostic = (*diagnostics)[goal];
            diagnostic.hasNativeStatus = true;
            diagnostic.nativeStatus = status;
            diagnostic.nativeStatusTimeNs = ts;
        }
        if(!current.isEmpty() && RobotWsCdrDecoder::sameGoalUuid(current, goal)) { auto* action=snapshot->mutable_action_state(); action->set_native_status(status); action->set_native_status_time_ns(ts); }
    }
    return true;
}

bool readPose(CdrReader& r, double& px,double& py,double& pz,double& qx,double& qy,double& qz,double& qw,QString*e)
{ return r.f64(px,e)&&r.f64(py,e)&&r.f64(pz,e)&&r.f64(qx,e)&&r.f64(qy,e)&&r.f64(qz,e)&&r.f64(qw,e); }

bool readTwist(CdrReader&r,double&lx,double&ly,double&lz,double&ax,double&ay,double&az,QString*e)
{ return r.f64(lx,e)&&r.f64(ly,e)&&r.f64(lz,e)&&r.f64(ax,e)&&r.f64(ay,e)&&r.f64(az,e); }

bool decodeLocation(CdrReader&r,quint64 ts,wire::VisualizationSnapshot*s,QString*e)
{
    qint64 start,gps,status,error,usbl[4]; double d[30],ux,uy,uz;
    if(!r.i64(start,e)||!r.i64(gps,e)||!r.i64(status,e)||!r.i64(error,e)) return false;
    for(double&v:d) if(!r.f64(v,e)) return false;
    for(int i=0;i<4;++i) if(!r.i64(usbl[i],e)) return false;
    if(!r.f64(ux,e)||!r.f64(uy,e)||!r.f64(uz,e)) return false;
    auto* v=s->mutable_vehicle_state(); fillHeader(v->mutable_header(),0,ts,"robot_ws.location","odom");
    // d: lon,lat,height,depth,gauss3,origin5,odom3,odom_heading,velocity4,pitch,roll,heading,omega3,acc4
    v->mutable_position()->set_x_m(d[12]); v->mutable_position()->set_y_m(d[13]); v->mutable_position()->set_z_m(d[14]);
    v->set_heading_rad(d[22]); v->set_pitch_rad(d[20]); v->set_roll_rad(d[21]);
    v->mutable_linear_velocity_mps()->set_x(d[16]); v->mutable_linear_velocity_mps()->set_y(d[17]); v->mutable_linear_velocity_mps()->set_z(d[18]); v->set_speed_mps(d[19]);
    v->set_yaw_rate_radps(d[25]); v->mutable_linear_acceleration_mps2()->set_x(d[26]); v->mutable_linear_acceleration_mps2()->set_y(d[27]); v->mutable_linear_acceleration_mps2()->set_z(d[28]); v->set_longitudinal_acceleration_mps2(d[29]);
    auto*u=v->mutable_underwater(); u->set_odom_z_m(d[14]); u->set_depth_m(d[3]); u->set_height_above_bottom_m(d[2]); u->set_vertical_velocity_mps(d[18]); u->set_usbl_x_m(ux); u->set_usbl_y_m(uy); u->set_usbl_z_m(uz);
    v->set_longitude_deg(d[0]); v->set_latitude_deg(d[1]);
    v->set_localization_status(status); v->set_localization_error(error); if(gps>0)v->set_gps_time(static_cast<quint64>(gps));
    v->set_start_time_s(start); v->set_gauss_x_m(d[4]); v->set_gauss_y_m(d[5]); v->set_gauss_z_m(d[6]);
    v->set_origin_longitude_deg(d[7]); v->set_origin_latitude_deg(d[8]); v->set_origin_x_m(d[9]); v->set_origin_y_m(d[10]); v->set_origin_z_m(d[11]);
    v->set_odom_heading_rad(d[15]); v->set_angular_velocity_x_radps(d[23]); v->set_angular_velocity_y_radps(d[24]);
    v->set_usbl_message_word_1(usbl[0]); v->set_usbl_message_word_2(usbl[1]);
    v->set_usbl_message_word_3(usbl[2]); v->set_usbl_message_word_4(usbl[3]);
    return true;
}

enum class ChassisCommandCdrLayout { Invalid, LegacyWithDiveSpeed, Current };

bool decodeCommand(CdrReader&r,quint64 ts,wire::VisualizationSnapshot*s,QString*e,
                   ChassisCommandCdrLayout layout)
{
    quint8 mode,gear,navi,buoy; bool enabled,emergency,useWater,sonar; double speed,rate,depth,height,heading,diveSpeed=0; qint8 left,right;
    if(!r.u8(mode,e)||!r.boolean(enabled,e)||!r.boolean(emergency,e)||!r.f64(speed,e)||!r.f64(rate,e)||!r.u8(gear,e)||!r.boolean(useWater,e)||!r.u8(navi,e)||!r.f64(depth,e)||!r.f64(height,e)||!r.f64(heading,e))return false;
    // The old bag layout placed the now-removed dive_speed between heading
    // and actuator commands. It has no current protocol counterpart, but
    // must be consumed to preserve the following command fields.
    if(layout==ChassisCommandCdrLayout::LegacyWithDiveSpeed&&!r.f64(diveSpeed,e))return false;
    if(!r.i8(left,e)||!r.i8(right,e)||!r.u8(buoy,e)||!r.boolean(sonar,e))return false;
    auto*c=s->mutable_control_command(); fillHeader(c->mutable_header(),0,ts,"robot_ws.chassis_command");
    const bool crawl=mode==6||mode==8||mode==11, sailing=(mode>=1&&mode<=5)||mode==7||mode==9||mode==10;
    c->set_mode(crawl?wire::ControlCommand::MODE_CRAWL:sailing?wire::ControlCommand::MODE_SAILING:wire::ControlCommand::MODE_UNKNOWN); c->set_maneuver((mode==10||mode==11)?wire::ControlCommand::MANEUVER_YAW_IN_PLACE:wire::ControlCommand::MANEUVER_NONE); c->set_source_mode(mode); c->set_enabled(enabled); c->set_target_speed_mps(speed); c->set_target_yaw_rate_radps(rate); c->set_target_heading_rad(heading); c->set_target_gear(gear);
    auto*u=c->mutable_underwater();u->set_water_actuator_enabled(useWater);u->set_navigation_mode(navi);u->set_target_depth_m(depth);u->set_target_height_above_bottom_m(height);u->set_left_thruster_command(left);u->set_right_thruster_command(right);u->set_buoyancy_command(buoyancy(buoy));u->set_vertical_control_mode(layout==ChassisCommandCdrLayout::Current&&mode==2?wire::VERTICAL_CONTROL_MODE_LANDING:verticalMode(navi));u->set_sonar_power_enabled(sonar);u->set_emergency_ascent(emergency); return true;
}

struct Motor { double speed=0,torque=0,bus=0,cmdSpeed=0,cmdTorque=0; qint16 temp=0,utemp=0,vtemp=0; bool ready=false,output=false,fault=false,cmdEnable=false,cmdSpeedMode=false,cmdReverse=false; quint8 faultCode=0; };
bool readMotor(CdrReader&r,Motor&m,QString*e){return r.f64(m.speed,e)&&r.f64(m.torque,e)&&r.i16(m.temp,e)&&r.f64(m.bus,e)&&r.boolean(m.ready,e)&&r.boolean(m.output,e)&&r.i16(m.utemp,e)&&r.i16(m.vtemp,e)&&r.boolean(m.fault,e)&&r.u8(m.faultCode,e);}
bool readMotorCommand(CdrReader&r,Motor&m,QString*e){return r.boolean(m.cmdEnable,e)&&r.boolean(m.cmdSpeedMode,e)&&r.boolean(m.cmdReverse,e)&&r.f64(m.cmdSpeed,e)&&r.f64(m.cmdTorque,e);}
void fillMotor(wire::CrawlMotorState*t,const Motor&m,quint8 actuator){t->set_valid(true);t->set_speed_rpm(m.speed);t->set_torque_or_q_axis_current(m.torque);t->set_temperature_c(m.temp);t->set_bus_voltage_v(m.bus);t->set_controller_ready(m.ready);t->set_output_enabled(m.output);t->set_controller_u_temperature_c(m.utemp);t->set_controller_v_temperature_c(m.vtemp);t->set_fault(m.fault);t->set_motor_fault_code(m.faultCode);t->set_actuator_fault_code(actuator);t->set_command_enable(m.cmdEnable);t->set_command_speed_mode(m.cmdSpeedMode);t->set_command_reverse(m.cmdReverse);t->set_command_speed_rpm(m.cmdSpeed);t->set_command_torque_or_q_axis_current(m.cmdTorque);}

enum class ChassisCdrLayout { Invalid, Legacy, LegacyWithTail, Current, CurrentWithTail, CurrentWithAllThrusterMotors };

struct ThrusterMotorFeedback {
    double busCurrent = 0;
    qint16 controllerTemperature = 0;
    double targetSpeed = 0;
    double actualSpeed = 0;
};

bool readThrusterMotorFeedback(CdrReader& reader, ThrusterMotorFeedback& motor, QString* error)
{
    return reader.f64(motor.busCurrent, error)
           && reader.i16(motor.controllerTemperature, error)
           && reader.f64(motor.targetSpeed, error)
           && reader.f64(motor.actualSpeed, error);
}

void addThrusterMotor(wire::ChassisState* chassis,
                      const char* id,
                      const ThrusterMotorFeedback& motor)
{
    auto* output = chassis->add_thruster_motor();
    output->set_id(id);
    output->set_bus_current_a(motor.busCurrent);
    output->set_controller_temperature_c(motor.controllerTemperature);
    output->set_target_speed_rpm(motor.targetSpeed);
    output->set_actual_speed_rpm(motor.actualSpeed);
}

bool decodeChassis(CdrReader&r,quint64 ts,wire::VisualizationSnapshot*s,QString*e,ChassisCdrLayout layout)
{
    const bool hasHeading = layout == ChassisCdrLayout::Current || layout == ChassisCdrLayout::CurrentWithTail || layout == ChassisCdrLayout::CurrentWithAllThrusterMotors;
    const bool hasTailMotorTelemetry = layout == ChassisCdrLayout::LegacyWithTail || layout == ChassisCdrLayout::CurrentWithTail || layout == ChassisCdrLayout::CurrentWithAllThrusterMotors;
    const bool hasAllThrusterMotorTelemetry = layout == ChassisCdrLayout::CurrentWithAllThrusterMotors;
    quint8 thr[5],waterHb,tank, tankStatus,gear,leftAct,rightAct,crawlHb; bool tankRaw; double speed,rate;
    double headingKp=0,headingTarget=0,headingActual=0,headingError=0,headingOutput=0;
    ThrusterMotorFeedback tailMotors[2];
    ThrusterMotorFeedback verticalMotors[3];
    for(auto&v:thr)if(!r.u8(v,e))return false;
    if(hasHeading&&!r.f64(headingKp,e))return false;
    if(!r.u8(waterHb,e))return false;
    if(hasHeading&&!r.f64(headingTarget,e))return false;
    if(hasHeading&&!r.f64(headingActual,e))return false;
    if(hasHeading&&!r.f64(headingError,e))return false;
    if(hasHeading&&!r.f64(headingOutput,e))return false;
    if(hasTailMotorTelemetry&&!readThrusterMotorFeedback(r, tailMotors[0], e))return false;
    if(hasTailMotorTelemetry&&!readThrusterMotorFeedback(r, tailMotors[1], e))return false;
    if(hasAllThrusterMotorTelemetry) {
        for (auto& motor : verticalMotors) if (!readThrusterMotorFeedback(r, motor, e)) return false;
    }
    if(!r.u8(tank,e)||!r.boolean(tankRaw,e)||!r.u8(tankStatus,e)||!r.u8(gear,e)||!r.f64(speed,e)||!r.f64(rate,e)||!r.u8(leftAct,e)||!r.u8(rightAct,e)||!r.u8(crawlHb,e))return false;
    Motor left,right; if(!readMotor(r,left,e)||!readMotor(r,right,e)||!readMotorCommand(r,left,e)||!readMotorCommand(r,right,e))return false;
    quint8 bmsStatus,bmsHb,alarm,current,maxVi,minVi,maxTi,minTi; double packV,packI,maxV,minV; qint8 maxT,minT;
    if(!r.u8(bmsStatus,e)||!r.u8(bmsHb,e)||!r.u8(alarm,e)||!r.u8(current,e)||!r.f64(packV,e)||!r.f64(packI,e)||!r.u8(maxVi,e)||!r.f64(maxV,e)||!r.u8(minVi,e)||!r.f64(minV,e)||!r.u8(maxTi,e)||!r.i8(maxT,e)||!r.u8(minTi,e)||!r.i8(minT,e))return false;
    quint8 warnings[12],power[16],soc; for(auto&v:warnings)if(!r.u8(v,e))return false; bool dcdc; if(!r.boolean(dcdc,e))return false; for(auto&v:power)if(!r.u8(v,e))return false; double inputV; bool emergency; if(!r.u8(soc,e)||!r.f64(inputV,e)||!r.boolean(emergency,e))return false;
    auto*c=s->mutable_chassis_state();fillHeader(c->mutable_header(),0,ts,"robot_ws.chassis_states");c->set_speed_mps(speed);c->set_yaw_rate_radps(rate);c->set_gear(gear);
    // Keep consuming legacy heading-controller diagnostics to preserve the
    // fixed CDR layout, but do not expose these retired Client-side values.
    (void)headingKp; (void)headingTarget; (void)headingActual; (void)headingError; (void)headingOutput;
    if(hasTailMotorTelemetry){
        auto*leftTail=c->add_tail_thruster_motor();leftTail->set_id("left_tail_thruster");leftTail->set_bus_current_a(tailMotors[0].busCurrent);leftTail->set_controller_temperature_c(tailMotors[0].controllerTemperature);leftTail->set_target_speed_rpm(tailMotors[0].targetSpeed);leftTail->set_actual_speed_rpm(tailMotors[0].actualSpeed);
        auto*rightTail=c->add_tail_thruster_motor();rightTail->set_id("right_tail_thruster");rightTail->set_bus_current_a(tailMotors[1].busCurrent);rightTail->set_controller_temperature_c(tailMotors[1].controllerTemperature);rightTail->set_target_speed_rpm(tailMotors[1].targetSpeed);rightTail->set_actual_speed_rpm(tailMotors[1].actualSpeed);
        if(hasAllThrusterMotorTelemetry){
            addThrusterMotor(c, "left_tail_thruster", tailMotors[0]);
            addThrusterMotor(c, "right_tail_thruster", tailMotors[1]);
            addThrusterMotor(c, "left_vertical_thruster", verticalMotors[0]);
            addThrusterMotor(c, "right_vertical_thruster", verticalMotors[1]);
            addThrusterMotor(c, "back_vertical_thruster", verticalMotors[2]);
        }
    }
    auto*u=c->mutable_underwater();u->set_water_tank_level(tank);u->set_water_tank_level_is_raw(tankRaw);u->set_water_tank_state(tankState(tankStatus));u->set_water_heartbeat(waterHb);u->set_emergency_ascent_active(emergency);const char*ids[]={"left_tail_thruster","right_tail_thruster","left_vertical_thruster","right_vertical_thruster","back_vertical_thruster"};for(int i=0;i<5;++i){auto*t=u->add_thruster();t->set_id(ids[i]);t->set_fault_code(thr[i]);}
    auto*p=c->mutable_platform();p->set_crawl_heartbeat(crawlHb);p->set_dcdc_enabled(dcdc);p->set_smart_power_input_voltage_v(inputV);fillMotor(p->mutable_left_crawl_motor(),left,leftAct);fillMotor(p->mutable_right_crawl_motor(),right,rightAct);
    auto*b=p->mutable_battery();b->set_valid(true);b->set_self_check_status(bmsStatus);b->set_heartbeat(bmsHb);b->set_alarm_level(alarm);b->set_current_status(current);b->set_pack_voltage_v(packV);b->set_pack_current_a(packI);b->set_max_cell_voltage_index(maxVi);b->set_max_cell_voltage_v(maxV);b->set_min_cell_voltage_index(minVi);b->set_min_cell_voltage_v(minV);b->set_max_temperature_index(maxTi);b->set_max_temperature_c(maxT);b->set_min_temperature_index(minTi);b->set_min_temperature_c(minT);b->set_state_of_charge_percent(soc);for(auto v:warnings)b->add_warning_code(v);for(int i=0;i<16;++i){auto*ch=p->add_power_channel();ch->set_index(i+1);ch->set_status(power[i]);} return true;
}

bool decodeAction(CdrReader&r,quint64 ts,wire::VisualizationSnapshot*s,QString*e,
                  const RobotWsCdrDecoder::ActionDiagnosticCache* diagnostics)
{
    quint8 owner,state,mode,navi,buoy;QString goal,message;bool enabled,emergency;double depth,height,speed,heading,rate;
    if(!r.u8(owner,e)||!r.string(goal,e)||!r.u8(state,e)||!r.string(message,e)||!r.u8(mode,e)||!r.boolean(enabled,e)||!r.boolean(emergency,e)||!r.u8(navi,e)||!r.f64(depth,e)||!r.f64(height,e)||!r.u8(buoy,e)||!r.f64(speed,e)||!r.f64(heading,e)||!r.f64(rate,e))return false;
    wire::ActionState next;fillHeader(next.mutable_header(),0,ts,"robot_ws.system_run_states");next.set_owner(owner);next.set_state(state);next.set_goal_id(goal.toStdString());next.set_message(message.toStdString());next.set_action_name(actionName(owner).toStdString());next.set_chassis_mode(mode);next.set_enabled(enabled);next.set_navigation_mode(navi);next.set_target_speed_mps(speed);next.set_target_heading_rad(heading);next.set_target_yaw_rate_radps(rate*kDegreesToRadians);auto*u=next.mutable_underwater();u->set_navigation_mode(navi);u->set_target_depth_m(depth);u->set_target_height_above_bottom_m(height);u->set_buoyancy_command(buoyancy(buoy));u->set_vertical_control_mode(actionVerticalMode(owner,mode,navi));u->set_emergency_ascent(emergency);
    if(s->has_action_state()&&s->action_state().goal_id()==goal.toStdString()){
        const auto& previous=s->action_state();
        if(previous.has_native_status()){next.set_native_status(previous.native_status());next.set_native_status_time_ns(previous.native_status_time_ns());}
        if(previous.has_feedback_progress()){next.set_feedback_progress(previous.feedback_progress());next.set_feedback_time_ns(previous.feedback_time_ns());}
    }
    if(state==2||state==3||state==4){wire::ActionState terminal=next;terminal.clear_recent_terminal();next.mutable_recent_terminal()->CopyFrom(terminal);}
    else if(s->has_action_state()&&s->action_state().has_recent_terminal())next.mutable_recent_terminal()->CopyFrom(s->action_state().recent_terminal());
    mergeActionDiagnostic(goal, &next, diagnostics);
    s->mutable_action_state()->Swap(&next);return true;
}

bool decodeTask(CdrReader&r,quint64 ts,wire::VisualizationSnapshot*s,QString*e)
{
    quint8 task,id,remote,power,buoy,gear;bool enabled,emergency,release,action;double crawlSpeed,crawlRate; qint8 remoteValues[8]{};bool ps;
    if(!r.u8(task,e)||!r.u8(id,e)||!r.boolean(enabled,e)||!r.boolean(emergency,e)||!r.boolean(release,e)||!r.u8(remote,e)||!r.u8(power,e)||!r.boolean(action,e)||!r.u8(buoy,e)||!r.u8(gear,e)||!r.f64(crawlSpeed,e)||!r.f64(crawlRate,e))return false;
    // TaskParams stores three sailing percentages followed by five actuator commands.
    // Keep the wire order aligned with the robot_ws schema so bag playback matches Server conversion.
    for(int index=0;index<8;++index)if(!r.i8(remoteValues[index],e))return false;
    auto*t=s->mutable_task_state();fillHeader(t->mutable_header(),0,ts,"robot_ws.task_params");t->set_task_type(task);t->set_task_id(id);t->set_enabled(enabled);t->set_action_enabled(action);t->set_emergency_stop(emergency);t->set_remote_mode(remote);t->set_power_enable(power);t->mutable_underwater()->set_release_emergency_ascent(release);t->mutable_underwater()->set_buoyancy_command(buoyancy(buoy));
    auto* remoteControl=t->mutable_remote_control();remoteControl->set_crawl_gear(gear);remoteControl->set_crawl_speed_mps(crawlSpeed);remoteControl->set_crawl_angular_velocity_radps(crawlRate);
    remoteControl->set_forward_percent(remoteValues[0]);remoteControl->set_turn_percent(remoteValues[1]);remoteControl->set_dive_percent(remoteValues[2]);remoteControl->set_left_tail_actuator_speed(remoteValues[3]);remoteControl->set_right_tail_actuator_speed(remoteValues[4]);remoteControl->set_left_vertical_actuator_speed(remoteValues[5]);remoteControl->set_right_vertical_actuator_speed(remoteValues[6]);remoteControl->set_back_vertical_actuator_speed(remoteValues[7]);
    for(int index=0;index<16;++index){if(!r.boolean(ps,e))return false;remoteControl->add_power_supply_enabled(ps);}return true;
}

bool readCustomPoint(CdrReader& r, double values[8], QString* e);

bool decodeRangeMotion(CdrReader& r, quint64 ts, wire::VisualizationSnapshot* s, QString* e)
{
    qint32 sec; quint32 ns, taskId, commandSequence; QString frame, reason; quint8 motion; float speedLimit;
    if (!readHeader(r, sec, ns, frame, e) || !r.u32(taskId, e) || !r.u32(commandSequence, e)
        || !r.u8(motion, e) || !r.f32Raw(speedLimit, e) || !r.string(reason, e)) {
        return false;
    }
    auto* target = s->mutable_perception_state()->mutable_range_motion_directive();
    fillHeader(target->mutable_header(), headerNs(sec, ns, ts), ts,
               "robot_ws.range_motion_request", frame);
    target->set_task_id(taskId);
    target->set_command_sequence(commandSequence);
    target->set_motion(static_cast<wire::RangeMotionDirective::Motion>(motion));
    target->set_speed_limit_mps(speedLimit);
    target->set_reason(reason.toStdString());
    return true;
}

bool decodeInspectionGoal(CdrReader& r, quint64 ts, wire::VisualizationSnapshot* s, QString* e)
{
    qint32 sec; quint32 ns, taskId, goalId; quint16 targetId; QString frame;
    double target[8], observation[8], heading; bool hold; quint8 mode; float speedLimit;
    if (!readHeader(r, sec, ns, frame, e) || !r.u32(taskId, e) || !r.u32(goalId, e)
        || !r.u16(targetId, e) || !readCustomPoint(r, target, e)
        || !readCustomPoint(r, observation, e) || !r.f64Raw(heading, e)
        || !r.boolean(hold, e) || !r.u8(mode, e) || !r.f32Raw(speedLimit, e)) {
        return false;
    }
    auto* result = s->mutable_perception_state()->mutable_inspection_goal();
    fillHeader(result->mutable_header(), headerNs(sec, ns, ts), ts,
               "robot_ws.inspection_request_goal", frame);
    result->set_task_id(taskId);
    result->set_goal_id(goalId);
    result->set_target_id(targetId);
    result->mutable_target_position()->set_x_m(target[4]);
    result->mutable_target_position()->set_y_m(target[5]);
    result->mutable_target_position()->set_z_m(target[6]);
    result->mutable_observation_position()->set_x_m(observation[4]);
    result->mutable_observation_position()->set_y_m(observation[5]);
    result->mutable_observation_position()->set_z_m(observation[6]);
    result->set_target_heading_rad(heading);
    result->set_hold_on_arrival(hold);
    result->set_mode(static_cast<wire::InspectionGoal::Mode>(mode));
    result->set_speed_limit_mps(speedLimit);
    return true;
}

bool decodeLocalPath(CdrReader&r,quint64 ts,wire::VisualizationSnapshot*s,QString*e)
{
    qint32 sec;quint32 ns,count,dns;QString frame,goal;if(!readHeader(r,sec,ns,frame,e)||!r.string(goal,e)||!r.sequenceSize(count,e))return false;const quint64 source=headerNs(sec,ns,ts);auto*t=s->mutable_local_trajectory();fillHeader(t->mutable_header(),source,ts,"robot_ws.local_path",frame);t->set_kind(wire::Trajectory::KIND_LOCAL);t->set_goal_id(goal.toStdString());
    for(quint32 i=0;i<count;++i){double px,py,pz,qx,qy,qz,qw,vlx,vly,vlz,vax,vay,vaz,alx,aly,alz,aax,aay,aaz; qint32 ds;if(!readPose(r,px,py,pz,qx,qy,qz,qw,e)||!readTwist(r,vlx,vly,vlz,vax,vay,vaz,e)||!readTwist(r,alx,aly,alz,aax,aay,aaz,e)||!r.i32(ds,e)||!r.u32(dns,e))return false;auto*p=t->add_point();auto*pp=p->mutable_path_point();pp->mutable_position()->set_x_m(px);pp->mutable_position()->set_y_m(py);pp->mutable_position()->set_z_m(pz);pp->set_heading_rad(yaw(qx,qy,qz,qw));auto*pose=p->mutable_pose();pose->mutable_position()->set_x_m(px);pose->mutable_position()->set_y_m(py);pose->mutable_position()->set_z_m(pz);pose->set_quaternion_x(qx);pose->set_quaternion_y(qy);pose->set_quaternion_z(qz);pose->set_quaternion_w(qw);auto*velocity=p->mutable_velocity_3d();velocity->mutable_linear()->set_x(vlx);velocity->mutable_linear()->set_y(vly);velocity->mutable_linear()->set_z(vlz);velocity->mutable_angular()->set_x(vax);velocity->mutable_angular()->set_y(vay);velocity->mutable_angular()->set_z(vaz);auto*acceleration=p->mutable_acceleration_3d();acceleration->mutable_linear()->set_x(alx);acceleration->mutable_linear()->set_y(aly);acceleration->mutable_linear()->set_z(alz);acceleration->mutable_angular()->set_x(aax);acceleration->mutable_angular()->set_y(aay);acceleration->mutable_angular()->set_z(aaz);p->set_speed_mps(vlx);p->set_acceleration_mps2(alx);const double rel=ds+dns*1e-9;p->set_relative_time_s(rel);p->set_absolute_time_s(source*1e-9+rel);}
    t->set_total_length_m(trajectoryLength(*t));if(t->point_size())t->set_total_time_s(t->point(t->point_size()-1).relative_time_s());return true;
}

bool decodeGlobalPath(CdrReader&r,quint64 ts,wire::VisualizationSnapshot*s,QString*e)
{
    qint32 sec,psec;quint32 ns,pns,count;QString frame,pframe;if(!readHeader(r,sec,ns,frame,e)||!r.sequenceSize(count,e))return false;const quint64 source=headerNs(sec,ns,ts);auto*t=s->mutable_global_trajectory();fillHeader(t->mutable_header(),source,ts,"robot_ws.global_path",frame);t->set_kind(wire::Trajectory::KIND_GLOBAL);
    for(quint32 i=0;i<count;++i){double px,py,pz,qx,qy,qz,qw;if(!readHeader(r,psec,pns,pframe,e)||!readPose(r,px,py,pz,qx,qy,qz,qw,e))return false;auto*point=t->add_point();auto*p=point->mutable_path_point();p->mutable_position()->set_x_m(px);p->mutable_position()->set_y_m(py);p->mutable_position()->set_z_m(pz);p->set_heading_rad(yaw(qx,qy,qz,qw));auto*pose=point->mutable_pose();pose->mutable_position()->set_x_m(px);pose->mutable_position()->set_y_m(py);pose->mutable_position()->set_z_m(pz);pose->set_quaternion_x(qx);pose->set_quaternion_y(qy);pose->set_quaternion_z(qz);pose->set_quaternion_w(qw);}t->set_total_length_m(trajectoryLength(*t));return true;
}

bool readCustomPoint(CdrReader&r,double values[8],QString*e){for(int i=0;i<8;++i)if(!r.f64Raw(values[i],e))return false;return true;}
bool decodeFinalTargets(CdrReader&r,quint64 ts,wire::VisualizationSnapshot*s,QString*e)
{
    qint32 sec,tsec; quint32 ns,task,count,tns; quint16 mineNumber; QString frame,tframe;
    if(!readHeader(r,sec,ns,frame,e)||!r.u32(task,e)||!r.u16(mineNumber,e)||!r.sequenceSize(count,e))return false;
    if(count>100000U){if(e)*e=QStringLiteral("当前 FinalTargetArray targets 过多");return false;}
    const quint64 source=headerNs(sec,ns,ts); auto*set=s->mutable_final_targets();
    fillHeader(set->mutable_header(),source,ts,"robot_ws.final_targets",frame);set->set_source_task_id(task);set->set_mine_number(mineNumber);
    if(frame!=QStringLiteral("odom")){set->set_rejection_reason("final target set rejected: array frame is not odom");return true;}
    QSet<quint16> ids;
    for(quint32 i=0;i<count;++i){
        quint16 id; double point[8],radius; quint8 cls; quint32 boundaryCount;
        if(!readHeader(r,tsec,tns,tframe,e)||!r.u16(id,e)||!readCustomPoint(r,point,e)||!r.f64Raw(radius,e)||!r.u8(cls,e)||!r.sequenceSize(boundaryCount,e))return false;
        if(boundaryCount>128U){if(e)*e=QStringLiteral("FinalTarget boundary 超过 128 点");return false;}
        QVector<QPair<double,double>> boundary;boundary.reserve(static_cast<int>(boundaryCount));
        for(quint32 vertex=0;vertex<boundaryCount;++vertex){double values[8];if(!readCustomPoint(r,values,e))return false;boundary.push_back({values[4],values[5]});}
        if(ids.contains(id)){set->clear_target();set->set_rejection_reason("final target set rejected: duplicate target_id="+std::to_string(id));return true;} ids.insert(id);
        if(tframe!=QStringLiteral("odom")){set->clear_target();set->set_rejection_reason("final target set rejected: target frame is not odom (target_id="+std::to_string(id)+")");return true;}
        if(!std::isfinite(point[4])||!std::isfinite(point[5])){set->clear_target();set->set_rejection_reason("final target set rejected: non-finite reference point (target_id="+std::to_string(id)+")");return true;}
        if(!std::isfinite(radius)||radius<=0.0){set->clear_target();set->set_rejection_reason("final target set rejected: invalid radius (target_id="+std::to_string(id)+")");return true;}
        if(cls>3){set->clear_target();set->set_rejection_reason("final target set rejected: invalid final_class (target_id="+std::to_string(id)+")");return true;}
        auto*target=set->add_target();fillHeader(target->mutable_header(),headerNs(tsec,tns,source),ts,"robot_ws.final_target",tframe);target->set_target_id(id);target->set_final_class(cls);target->mutable_reference_point()->set_x_m(point[4]);target->mutable_reference_point()->set_y_m(point[5]);target->set_radius_m(radius);
        if(cls==2&&!boundary.isEmpty()){
            bool valid=boundary.size()>=3; double twiceArea=0.0;
            for(int vertex=0;valid&&vertex<boundary.size();++vertex){const auto&current=boundary.at(vertex);const auto&next=boundary.at((vertex+1)%boundary.size());valid=std::isfinite(current.first)&&std::isfinite(current.second);twiceArea+=current.first*next.second-next.first*current.second;}
            valid=valid&&std::isfinite(twiceArea)&&std::abs(twiceArea)>1.0e-9;
            target->set_boundary_valid(valid);
            if(valid){for(const auto&vertex:boundary){auto*point2d=target->mutable_boundary()->add_point();point2d->set_x_m(vertex.first);point2d->set_y_m(vertex.second);}}
            else target->set_boundary_note("渔网边界无效，已回退保守圆");
        }
    }
    return true;
}

ChassisCdrLayout chassisCdrLayout(const QByteArray& payload)
{
    // All ChassisStates fields are fixed-size scalars. Layout selection is
    // strictly length based; no speculative parse/fallback is allowed.
    const int size = payload.size();
    if (size >= 277 && size <= 284) return ChassisCdrLayout::Legacy;
    if (size >= 341 && size <= 348) return ChassisCdrLayout::LegacyWithTail;
    if (size >= 325 && size <= 332) return ChassisCdrLayout::Current;
    if (size >= 389 && size <= 396) return ChassisCdrLayout::CurrentWithTail;
    if (size >= 485 && size <= 492) return ChassisCdrLayout::CurrentWithAllThrusterMotors;
    return ChassisCdrLayout::Invalid;
}

ChassisCommandCdrLayout chassisCommandCdrLayout(const QByteArray& payload)
{
    // ChassisCommand is scalar-only. The legacy layout has the removed
    // dive_speed float64 after heading; do not infer layouts by trial parse.
    if (payload.size() == 64) return ChassisCommandCdrLayout::Current;
    if (payload.size() == 72) return ChassisCommandCdrLayout::LegacyWithDiveSpeed;
    return ChassisCommandCdrLayout::Invalid;
}
}

QString RobotWsCdrDecoder::expectedType(const QString& topic)
{
    static const QMap<QString,QString> m={{"/location","custom_msgs/msg/Location"},{"/targets/final_objects","custom_msgs/msg/FinalTargetArray"},{"/detection/range_motion_request","custom_msgs/msg/RangeMotionRequest"},{"/detection/inspection_request_goal","custom_msgs/msg/InspectionRequestGoal"},{"/chassis_command","custom_msgs/msg/ChassisCommand"},{"/chassis_states","custom_msgs/msg/ChassisStates"},{"/system_run_states","custom_msgs/msg/SystemRunStates"},{"/task_params","custom_msgs/msg/TaskParams"},{"/local_path","custom_msgs/msg/TrajectoryMsg"},{"/global_path","nav_msgs/msg/Path"},{"/depth_command_action/_action/status","action_msgs/msg/GoalStatusArray"},{"/move_action/_action/status","action_msgs/msg/GoalStatusArray"},{"/depth_command_action/_action/feedback","custom_msgs/action/DepthCommand_FeedbackMessage"},{"/move_action/_action/feedback","custom_msgs/action/Move_FeedbackMessage"}};return m.value(topic);
}
bool RobotWsCdrDecoder::isSupported(const QString&t,const QString&type){return !expectedType(t).isEmpty()&&expectedType(t)==type;}
QStringList RobotWsCdrDecoder::supportedTopics(){return {"/location","/targets/final_objects","/detection/range_motion_request","/detection/inspection_request_goal","/chassis_command","/chassis_states","/system_run_states","/task_params","/local_path","/global_path","/depth_command_action/_action/status","/move_action/_action/status","/depth_command_action/_action/feedback","/move_action/_action/feedback"};}
wire::DataKind RobotWsCdrDecoder::dataKind(const QString&t){if(t=="/location")return wire::DATA_KIND_VEHICLE_STATE;if(t=="/targets/final_objects")return wire::DATA_KIND_OBSTACLES;if(t=="/detection/range_motion_request")return wire::DATA_KIND_RANGE_MOTION_DIRECTIVE;if(t=="/detection/inspection_request_goal")return wire::DATA_KIND_INSPECTION_GOAL;if(t=="/chassis_command")return wire::DATA_KIND_CONTROL_COMMAND;if(t=="/chassis_states")return wire::DATA_KIND_CHASSIS_STATE;if(t=="/system_run_states"||t.contains("/_action/"))return wire::DATA_KIND_ACTION_STATE;if(t=="/task_params")return wire::DATA_KIND_TASK_STATE;if(t=="/local_path")return wire::DATA_KIND_LOCAL_TRAJECTORY;if(t=="/global_path")return wire::DATA_KIND_GLOBAL_TRAJECTORY;return wire::DATA_KIND_UNKNOWN;}

bool RobotWsCdrDecoder::sameGoalUuid(const QString& a, const QString& b)
{
    if (a == b) return true;
    if (a.size() == 32 && lossyUuidHex(a) == b) return true;
    if (b.size() == 32 && lossyUuidHex(b) == a) return true;
    return false;
}

bool RobotWsCdrDecoder::decode(const QString&topic,const QString&type,const QByteArray&payload,quint64 ts,wire::VisualizationSnapshot*snapshot,QString*error,ActionDiagnosticCache* actionDiagnostics)
{
    if(!snapshot){if(error)*error=QStringLiteral("snapshot 为空");return false;}if(!isSupported(topic,type)){if(error)*error=QStringLiteral("不支持 %1 (%2)").arg(topic,type);return false;}CdrReader r(payload);if(!r.begin(error))return false;
    const ChassisCdrLayout chassisLayout = topic == "/chassis_states" ? chassisCdrLayout(payload) : ChassisCdrLayout::Legacy;
    if (topic == "/chassis_states" && chassisLayout == ChassisCdrLayout::Invalid) {
        if (error) *error = QStringLiteral("ChassisStates CDR 长度不匹配旧布局或当前布局: %1 字节").arg(payload.size());
        return false;
    }
    const ChassisCommandCdrLayout commandLayout = topic == "/chassis_command"
                                                      ? chassisCommandCdrLayout(payload)
                                                      : ChassisCommandCdrLayout::Current;
    if (topic == "/chassis_command" && commandLayout == ChassisCommandCdrLayout::Invalid) {
        if (error) *error = QStringLiteral("ChassisCommand CDR 长度不匹配旧布局或当前布局: %1 字节").arg(payload.size());
        return false;
    }
    // FinalTargetArray 的旧布局和截断 CDR 都可能在读取到一半时失败。先解码到
    // 临时快照，且只有完全消费 payload 后才替换当前集合，避免“跳过”坏帧时清空
    // 上一条合法目标或遗留半帧数据。
    if (topic == "/targets/final_objects") {
        wire::VisualizationSnapshot decoded;
        const bool decodedOk = decodeFinalTargets(r, ts, &decoded, error);
        if (!decodedOk || !r.finished(error)) return false;
        snapshot->mutable_final_targets()->Swap(decoded.mutable_final_targets());
        return true;
    }

    // rosbag 的一条 topic 消息与 Server 发送的同名 snapshot 字段语义相同：它是
    // 当前完整值，不是对上一条消息的增量。尤其 Path、障碍物和底盘状态包含 repeated
    // 字段；不先清理就会在本地回放中把旧点/旧状态反复累加。
    if(topic=="/location") snapshot->clear_vehicle_state();
    else if(topic=="/detection/range_motion_request" && snapshot->has_perception_state()) {
        snapshot->mutable_perception_state()->clear_range_motion_directive();
        if (!snapshot->perception_state().has_inspection_goal()) snapshot->clear_perception_state();
    } else if(topic=="/detection/inspection_request_goal" && snapshot->has_perception_state()) {
        snapshot->mutable_perception_state()->clear_inspection_goal();
        if (!snapshot->perception_state().has_range_motion_directive()) snapshot->clear_perception_state();
    }
    else if(topic=="/chassis_command") snapshot->clear_control_command();
    else if(topic=="/chassis_states") snapshot->clear_chassis_state();
    else if(topic=="/task_params") snapshot->clear_task_state();
    else if(topic=="/local_path") snapshot->clear_local_trajectory();
    else if(topic=="/global_path") snapshot->clear_global_trajectory();
    bool ok=false;
    if(topic=="/location")ok=decodeLocation(r,ts,snapshot,error);else if(topic=="/detection/range_motion_request")ok=decodeRangeMotion(r,ts,snapshot,error);else if(topic=="/detection/inspection_request_goal")ok=decodeInspectionGoal(r,ts,snapshot,error);else if(topic=="/chassis_command")ok=decodeCommand(r,ts,snapshot,error,commandLayout);else if(topic=="/chassis_states")ok=decodeChassis(r,ts,snapshot,error,chassisLayout);else if(topic=="/system_run_states")ok=decodeAction(r,ts,snapshot,error,actionDiagnostics);else if(topic=="/task_params")ok=decodeTask(r,ts,snapshot,error);else if(topic=="/local_path")ok=decodeLocalPath(r,ts,snapshot,error);else if(topic=="/global_path")ok=decodeGlobalPath(r,ts,snapshot,error);else if(topic.endsWith("/_action/status"))ok=updateActionStatus(r,ts,snapshot,error,actionDiagnostics);else if(topic.endsWith("/_action/feedback"))ok=updateActionFeedback(r,ts,snapshot,error,actionDiagnostics);
    if(!ok||!r.finished(error))return false;
    return true;
}

}  // namespace autoviz::playback
