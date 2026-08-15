#include "core/playback/RobotWsCdrDecoder.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <QMap>
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
    bool f64(double& v, QString* e) { if (!scalar(v,8,e)) return false; return std::isfinite(v) || fail(QStringLiteral("浮点字段不是有限值"),e); }

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
    int bytesRemaining() const { return m_data.size() - m_pos; }

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
    h->set_source_time_ns(source); h->set_server_receive_time_ns(receive); h->set_module_name(module);
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
{ if(owner==2&&chassisMode==1)return wire::VERTICAL_CONTROL_MODE_DEPTH_HOLD; if(owner==2&&chassisMode==2)return wire::VERTICAL_CONTROL_MODE_HEIGHT_HOLD; return verticalMode(navigationMode); }

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

bool updateActionFeedback(CdrReader& r, quint64 ts, wire::VisualizationSnapshot* snapshot, QString* error)
{
    QString goal; double progress=0.0;
    if(!readUuid(r,goal,error)||!r.f64(progress,error))return false;
    if(snapshot->has_action_state() && snapshot->action_state().goal_id()==goal.toStdString()) {
        auto* action=snapshot->mutable_action_state(); action->set_feedback_progress(progress); action->set_feedback_time_ns(ts);
    }
    return true;
}

bool updateActionStatus(CdrReader& r, quint64 ts, wire::VisualizationSnapshot* snapshot, QString* error)
{
    qint32 sec=0; quint32 nsec=0; QString frame; quint32 count=0;
    if(!readHeader(r,sec,nsec,frame,error)||!r.sequenceSize(count,error))return false;
    const QString current=snapshot->has_action_state()?QString::fromStdString(snapshot->action_state().goal_id()):QString{};
    for(quint32 index=0;index<count;++index) {
        QString goal; qint32 stampSec=0; quint32 stampNs=0; qint8 status=0;
        if(!readUuid(r,goal,error)||!r.i32(stampSec,error)||!r.u32(stampNs,error)||!r.i8(status,error))return false;
        if(!current.isEmpty() && goal==current) { auto* action=snapshot->mutable_action_state(); action->set_native_status(status); action->set_native_status_time_ns(ts); }
    }
    return true;
}

bool readPose(CdrReader& r, double& px,double& py,double& pz,double& qx,double& qy,double& qz,double& qw,QString*e)
{ return r.f64(px,e)&&r.f64(py,e)&&r.f64(pz,e)&&r.f64(qx,e)&&r.f64(qy,e)&&r.f64(qz,e)&&r.f64(qw,e); }

bool readTwist(CdrReader&r,double&lx,double&ly,double&lz,double&ax,double&ay,double&az,QString*e)
{ return r.f64(lx,e)&&r.f64(ly,e)&&r.f64(lz,e)&&r.f64(ax,e)&&r.f64(ay,e)&&r.f64(az,e); }

bool decodeLocation(CdrReader&r,quint64 ts,wire::VisualizationSnapshot*s,QString*e)
{
    qint64 start,gps,status,error,usbl; double d[30],ux,uy,uz;
    if(!r.i64(start,e)||!r.i64(gps,e)||!r.i64(status,e)||!r.i64(error,e)) return false;
    for(double&v:d) if(!r.f64(v,e)) return false;
    for(int i=0;i<4;++i) if(!r.i64(usbl,e)) return false;
    if(!r.f64(ux,e)||!r.f64(uy,e)||!r.f64(uz,e)) return false;
    auto* v=s->mutable_vehicle_state(); fillHeader(v->mutable_header(),ts,ts,"robot_ws.location","odom");
    // d: lon,lat,height,depth,gauss3,origin5,odom3,odom_heading,velocity4,pitch,roll,heading,omega3,acc4
    v->mutable_position()->set_x_m(d[12]); v->mutable_position()->set_y_m(d[13]); v->mutable_position()->set_z_m(d[14]);
    v->set_heading_rad(d[22]); v->set_pitch_rad(d[20]); v->set_roll_rad(d[21]);
    v->mutable_linear_velocity_mps()->set_x(d[16]); v->mutable_linear_velocity_mps()->set_y(d[17]); v->mutable_linear_velocity_mps()->set_z(d[18]); v->set_speed_mps(d[19]);
    v->set_yaw_rate_radps(d[25]); v->mutable_linear_acceleration_mps2()->set_x(d[26]); v->mutable_linear_acceleration_mps2()->set_y(d[27]); v->mutable_linear_acceleration_mps2()->set_z(d[28]); v->set_longitudinal_acceleration_mps2(d[29]);
    auto*u=v->mutable_underwater(); u->set_odom_z_m(d[14]); u->set_depth_m(d[3]); u->set_height_above_bottom_m(d[2]); u->set_vertical_velocity_mps(d[18]);
    v->set_localization_status(status); v->set_localization_error(error); if(gps>0)v->set_gps_time(static_cast<quint64>(gps));
    return true;
}

bool decodeCommand(CdrReader&r,quint64 ts,wire::VisualizationSnapshot*s,QString*e)
{
    quint8 mode,gear,navi,buoy; bool enabled,emergency,useWater,sonar; double speed,rate,depth,height,heading; qint8 left,right;
    if(!r.u8(mode,e)||!r.boolean(enabled,e)||!r.boolean(emergency,e)||!r.f64(speed,e)||!r.f64(rate,e)||!r.u8(gear,e)||!r.boolean(useWater,e)||!r.u8(navi,e)||!r.f64(depth,e)||!r.f64(height,e)||!r.f64(heading,e))return false;
    // 2026-08-06 前的 robot_ws ChassisCommand 在 heading 后还包含一个
    // dive_speed(float64)。样例库同时存在 72 字节旧布局和 64 字节新布局。
    double legacyDiveSpeed=0.0;
    if(r.bytesRemaining()>4 && !r.f64(legacyDiveSpeed,e))return false;
    if(!r.i8(left,e)||!r.i8(right,e)||!r.u8(buoy,e)||!r.boolean(sonar,e))return false;
    auto*c=s->mutable_control_command(); fillHeader(c->mutable_header(),ts,ts,"robot_ws.chassis_command");
    const bool crawl=mode==6||mode==8||mode==11, sailing=(mode>=1&&mode<=5)||mode==7||mode==10;
    c->set_mode(crawl?wire::ControlCommand::MODE_CRAWL:sailing?wire::ControlCommand::MODE_SAILING:wire::ControlCommand::MODE_UNKNOWN); c->set_maneuver((mode==10||mode==11||gear==4)?wire::ControlCommand::MANEUVER_YAW_IN_PLACE:wire::ControlCommand::MANEUVER_NONE); c->set_enabled(enabled); c->set_target_speed_mps(speed); c->set_target_yaw_rate_radps(rate); c->set_target_heading_rad(heading); c->set_target_gear(gear);
    auto*u=c->mutable_underwater();u->set_water_actuator_enabled(useWater);u->set_navigation_mode(navi);u->set_target_depth_m(depth);u->set_target_height_above_bottom_m(height);u->set_left_thruster_command(left);u->set_right_thruster_command(right);u->set_buoyancy_command(buoyancy(buoy));u->set_vertical_control_mode(verticalMode(navi));u->set_sonar_power_enabled(sonar);u->set_emergency_ascent(emergency); return true;
}

struct Motor { double speed=0,torque=0,bus=0,cmdSpeed=0,cmdTorque=0; qint16 temp=0,utemp=0,vtemp=0; bool ready=false,output=false,fault=false,cmdEnable=false,cmdSpeedMode=false,cmdReverse=false; quint8 faultCode=0; };
bool readMotor(CdrReader&r,Motor&m,QString*e){return r.f64(m.speed,e)&&r.f64(m.torque,e)&&r.i16(m.temp,e)&&r.f64(m.bus,e)&&r.boolean(m.ready,e)&&r.boolean(m.output,e)&&r.i16(m.utemp,e)&&r.i16(m.vtemp,e)&&r.boolean(m.fault,e)&&r.u8(m.faultCode,e);}
bool readMotorCommand(CdrReader&r,Motor&m,QString*e){return r.boolean(m.cmdEnable,e)&&r.boolean(m.cmdSpeedMode,e)&&r.boolean(m.cmdReverse,e)&&r.f64(m.cmdSpeed,e)&&r.f64(m.cmdTorque,e);}
void fillMotor(wire::CrawlMotorState*t,const Motor&m,quint8 actuator){t->set_valid(true);t->set_speed_rpm(m.speed);t->set_torque_or_q_axis_current(m.torque);t->set_temperature_c(m.temp);t->set_bus_voltage_v(m.bus);t->set_controller_ready(m.ready);t->set_output_enabled(m.output);t->set_controller_u_temperature_c(m.utemp);t->set_controller_v_temperature_c(m.vtemp);t->set_fault(m.fault);t->set_motor_fault_code(m.faultCode);t->set_actuator_fault_code(actuator);t->set_command_enable(m.cmdEnable);t->set_command_speed_mode(m.cmdSpeedMode);t->set_command_reverse(m.cmdReverse);t->set_command_speed_rpm(m.cmdSpeed);t->set_command_torque_or_q_axis_current(m.cmdTorque);}

bool decodeChassis(CdrReader&r,quint64 ts,wire::VisualizationSnapshot*s,QString*e)
{
    quint8 thr[5],waterHb,tank, tankStatus,gear,leftAct,rightAct,crawlHb; bool tankRaw; double speed,rate;
    for(auto&v:thr)if(!r.u8(v,e))return false; if(!r.u8(waterHb,e)||!r.u8(tank,e)||!r.boolean(tankRaw,e)||!r.u8(tankStatus,e)||!r.u8(gear,e)||!r.f64(speed,e)||!r.f64(rate,e)||!r.u8(leftAct,e)||!r.u8(rightAct,e)||!r.u8(crawlHb,e))return false;
    Motor left,right; if(!readMotor(r,left,e)||!readMotor(r,right,e)||!readMotorCommand(r,left,e)||!readMotorCommand(r,right,e))return false;
    quint8 bmsStatus,bmsHb,alarm,current,maxVi,minVi,maxTi,minTi; double packV,packI,maxV,minV; qint8 maxT,minT;
    if(!r.u8(bmsStatus,e)||!r.u8(bmsHb,e)||!r.u8(alarm,e)||!r.u8(current,e)||!r.f64(packV,e)||!r.f64(packI,e)||!r.u8(maxVi,e)||!r.f64(maxV,e)||!r.u8(minVi,e)||!r.f64(minV,e)||!r.u8(maxTi,e)||!r.i8(maxT,e)||!r.u8(minTi,e)||!r.i8(minT,e))return false;
    quint8 warnings[12],power[16],soc; for(auto&v:warnings)if(!r.u8(v,e))return false; bool dcdc; if(!r.boolean(dcdc,e))return false; for(auto&v:power)if(!r.u8(v,e))return false; double inputV; bool emergency; if(!r.u8(soc,e)||!r.f64(inputV,e)||!r.boolean(emergency,e))return false;
    auto*c=s->mutable_chassis_state();fillHeader(c->mutable_header(),ts,ts,"robot_ws.chassis_states");c->set_speed_mps(speed);c->set_yaw_rate_radps(-rate);c->set_gear(gear);
    auto*u=c->mutable_underwater();u->set_water_tank_level(tank);u->set_water_tank_level_is_raw(tankRaw);u->set_water_tank_state(tankState(tankStatus));u->set_water_heartbeat(waterHb);u->set_emergency_ascent_active(emergency);const char*ids[]={"left_tail_thruster","right_tail_thruster","left_vertical_thruster","right_vertical_thruster","back_vertical_thruster"};for(int i=0;i<5;++i){auto*t=u->add_thruster();t->set_id(ids[i]);t->set_fault_code(thr[i]);}
    auto*p=c->mutable_platform();p->set_crawl_heartbeat(crawlHb);p->set_dcdc_enabled(dcdc);p->set_smart_power_input_voltage_v(inputV);fillMotor(p->mutable_left_crawl_motor(),left,leftAct);fillMotor(p->mutable_right_crawl_motor(),right,rightAct);
    auto*b=p->mutable_battery();b->set_valid(true);b->set_self_check_status(bmsStatus);b->set_heartbeat(bmsHb);b->set_alarm_level(alarm);b->set_current_status(current);b->set_pack_voltage_v(packV);b->set_pack_current_a(packI);b->set_max_cell_voltage_index(maxVi);b->set_max_cell_voltage_v(maxV);b->set_min_cell_voltage_index(minVi);b->set_min_cell_voltage_v(minV);b->set_max_temperature_index(maxTi);b->set_max_temperature_c(maxT);b->set_min_temperature_index(minTi);b->set_min_temperature_c(minT);b->set_state_of_charge_percent(soc);for(auto v:warnings)b->add_warning_code(v);for(int i=0;i<16;++i){auto*ch=p->add_power_channel();ch->set_index(i+1);ch->set_status(power[i]);} return true;
}

bool decodeAction(CdrReader&r,quint64 ts,wire::VisualizationSnapshot*s,QString*e)
{
    quint8 owner,state,mode,navi,buoy;QString goal,message;bool enabled,emergency;double depth,height,speed,heading,rate;
    if(!r.u8(owner,e)||!r.string(goal,e)||!r.u8(state,e)||!r.string(message,e)||!r.u8(mode,e)||!r.boolean(enabled,e)||!r.boolean(emergency,e)||!r.u8(navi,e)||!r.f64(depth,e)||!r.f64(height,e)||!r.u8(buoy,e)||!r.f64(speed,e)||!r.f64(heading,e)||!r.f64(rate,e))return false;
    wire::ActionState next;fillHeader(next.mutable_header(),ts,ts,"robot_ws.system_run_states");next.set_owner(owner);next.set_state(state);next.set_goal_id(goal.toStdString());next.set_message(message.toStdString());next.set_action_name(actionName(owner).toStdString());next.set_chassis_mode(mode);next.set_enabled(enabled);next.set_navigation_mode(navi);next.set_target_speed_mps(speed);next.set_target_heading_rad(heading);next.set_target_yaw_rate_radps(rate*kDegreesToRadians);auto*u=next.mutable_underwater();u->set_navigation_mode(navi);u->set_target_depth_m(depth);u->set_target_height_above_bottom_m(height);u->set_buoyancy_command(buoyancy(buoy));u->set_vertical_control_mode(actionVerticalMode(owner,mode,navi));u->set_emergency_ascent(emergency);
    if(s->has_action_state()&&s->action_state().goal_id()==goal.toStdString()){
        const auto& previous=s->action_state();
        if(previous.has_native_status()){next.set_native_status(previous.native_status());next.set_native_status_time_ns(previous.native_status_time_ns());}
        if(previous.has_feedback_progress()){next.set_feedback_progress(previous.feedback_progress());next.set_feedback_time_ns(previous.feedback_time_ns());}
    }
    if(state==2||state==3||state==4){wire::ActionState terminal=next;terminal.clear_recent_terminal();next.mutable_recent_terminal()->CopyFrom(terminal);}
    else if(s->has_action_state()&&s->action_state().has_recent_terminal())next.mutable_recent_terminal()->CopyFrom(s->action_state().recent_terminal());
    s->mutable_action_state()->Swap(&next);return true;
}

bool decodeTask(CdrReader&r,quint64 ts,wire::VisualizationSnapshot*s,QString*e)
{
    quint8 task,id,remote,power,buoy,gear;bool enabled,emergency,release,action;double crawlSpeed,crawlRate; qint8 perc;bool ps;
    if(!r.u8(task,e)||!r.u8(id,e)||!r.boolean(enabled,e)||!r.boolean(emergency,e)||!r.boolean(release,e)||!r.u8(remote,e)||!r.u8(power,e)||!r.boolean(action,e)||!r.u8(buoy,e)||!r.u8(gear,e)||!r.f64(crawlSpeed,e)||!r.f64(crawlRate,e))return false;
    for(int i=0;i<8;++i)if(!r.i8(perc,e))return false;for(int i=0;i<16;++i)if(!r.boolean(ps,e))return false;
    auto*t=s->mutable_task_state();fillHeader(t->mutable_header(),ts,ts,"robot_ws.task_params");t->set_task_type(task);t->set_task_id(id);t->set_enabled(enabled);t->set_emergency_stop(emergency);t->set_remote_mode(remote);t->set_power_enable(power);t->mutable_underwater()->set_release_emergency_ascent(release);return true;
}

bool decodeLocalPath(CdrReader&r,quint64 ts,wire::VisualizationSnapshot*s,QString*e)
{
    qint32 sec;quint32 ns,count,dns;QString frame,goal;if(!readHeader(r,sec,ns,frame,e)||!r.string(goal,e)||!r.sequenceSize(count,e))return false;const quint64 source=headerNs(sec,ns,ts);auto*t=s->mutable_local_trajectory();fillHeader(t->mutable_header(),source,ts,"robot_ws.local_path",frame);t->set_kind(wire::Trajectory::KIND_LOCAL);t->set_goal_id(goal.toStdString());
    for(quint32 i=0;i<count;++i){double px,py,pz,qx,qy,qz,qw,vlx,vly,vlz,vax,vay,vaz,alx,aly,alz,aax,aay,aaz; qint32 ds;if(!readPose(r,px,py,pz,qx,qy,qz,qw,e)||!readTwist(r,vlx,vly,vlz,vax,vay,vaz,e)||!readTwist(r,alx,aly,alz,aax,aay,aaz,e)||!r.i32(ds,e)||!r.u32(dns,e))return false;auto*p=t->add_point();auto*pp=p->mutable_path_point();pp->mutable_position()->set_x_m(px);pp->mutable_position()->set_y_m(py);pp->mutable_position()->set_z_m(pz);pp->set_heading_rad(yaw(qx,qy,qz,qw));p->set_speed_mps(vlx);p->set_acceleration_mps2(alx);const double rel=ds+dns*1e-9;p->set_relative_time_s(rel);p->set_absolute_time_s(source*1e-9+rel);}
    t->set_total_length_m(trajectoryLength(*t));if(t->point_size())t->set_total_time_s(t->point(t->point_size()-1).relative_time_s());return true;
}

bool decodeGlobalPath(CdrReader&r,quint64 ts,wire::VisualizationSnapshot*s,QString*e)
{
    qint32 sec,psec;quint32 ns,pns,count;QString frame,pframe;if(!readHeader(r,sec,ns,frame,e)||!r.sequenceSize(count,e))return false;const quint64 source=headerNs(sec,ns,ts);auto*t=s->mutable_global_trajectory();fillHeader(t->mutable_header(),source,ts,"robot_ws.global_path",frame);t->set_kind(wire::Trajectory::KIND_GLOBAL);
    for(quint32 i=0;i<count;++i){double px,py,pz,qx,qy,qz,qw;if(!readHeader(r,psec,pns,pframe,e)||!readPose(r,px,py,pz,qx,qy,qz,qw,e))return false;auto*p=t->add_point()->mutable_path_point();p->mutable_position()->set_x_m(px);p->mutable_position()->set_y_m(py);p->mutable_position()->set_z_m(pz);p->set_heading_rad(yaw(qx,qy,qz,qw));}t->set_total_length_m(trajectoryLength(*t));return true;
}

bool readCustomPoint(CdrReader&r,double values[8],QString*e){for(int i=0;i<8;++i)if(!r.f64(values[i],e))return false;return true;}
const char* classLabel(quint8 c){return c==1?"mine":c==2?"net":c==3?"obstacle":c==4?"other":"unknown";}
bool decodeObstacles(CdrReader&r,quint64 ts,wire::VisualizationSnapshot*s,QString*e)
{
    qint32 sec,tsec;quint32 ns,task,count,tns;QString frame,tframe;if(!readHeader(r,sec,ns,frame,e)||!r.u32(task,e)||!r.sequenceSize(count,e))return false;const quint64 source=headerNs(sec,ns,ts);auto*set=s->mutable_obstacles();fillHeader(set->mutable_header(),source,ts,"robot_ws.final_targets",frame);
    for(quint32 i=0;i<count;++i){quint16 id;double point[8],length,width,height,heading;bool geo,dimensions,headingValid;quint8 cls;if(!readHeader(r,tsec,tns,tframe,e)||!r.u16(id,e)||!readCustomPoint(r,point,e)||!r.boolean(geo,e)||!r.f64(length,e)||!r.f64(width,e)||!r.f64(height,e)||!r.f64(heading,e)||!r.boolean(dimensions,e)||!r.boolean(headingValid,e)||!r.u8(cls,e))return false;if(!dimensions||length<=0||width<=0)continue;auto*o=set->add_obstacle();fillHeader(o->mutable_header(),headerNs(tsec,tns,source),ts,"robot_ws.final_target",tframe);o->set_id(std::to_string(id));o->set_type(wire::Obstacle::TYPE_OTHER);o->set_source_class(cls);o->set_class_label(classLabel(cls));o->set_source("fusion");o->mutable_center()->set_x_m(point[4]);o->mutable_center()->set_y_m(point[5]);o->mutable_center()->set_z_m(point[6]);if(headingValid)o->set_heading_rad(heading);o->set_length_m(length);o->set_width_m(width);o->set_height_m(height);o->set_is_static(true);o->set_is_virtual(false);}return true;
}
}

QString RobotWsCdrDecoder::expectedType(const QString& topic)
{
    static const QMap<QString,QString> m={{"/location","custom_msgs/msg/Location"},{"/targets/final_objects","custom_msgs/msg/FinalTargetArray"},{"/chassis_command","custom_msgs/msg/ChassisCommand"},{"/chassis_states","custom_msgs/msg/ChassisStates"},{"/system_run_states","custom_msgs/msg/SystemRunStates"},{"/task_params","custom_msgs/msg/TaskParams"},{"/local_path","custom_msgs/msg/TrajectoryMsg"},{"/global_path","nav_msgs/msg/Path"},{"/depth_command_action/_action/status","action_msgs/msg/GoalStatusArray"},{"/move_action/_action/status","action_msgs/msg/GoalStatusArray"},{"/depth_command_action/_action/feedback","custom_msgs/action/DepthCommand_FeedbackMessage"},{"/move_action/_action/feedback","custom_msgs/action/Move_FeedbackMessage"}};return m.value(topic);
}
bool RobotWsCdrDecoder::isSupported(const QString&t,const QString&type){return !expectedType(t).isEmpty()&&expectedType(t)==type;}
QStringList RobotWsCdrDecoder::supportedTopics(){return {"/location","/targets/final_objects","/chassis_command","/chassis_states","/system_run_states","/task_params","/local_path","/global_path","/depth_command_action/_action/status","/move_action/_action/status","/depth_command_action/_action/feedback","/move_action/_action/feedback"};}
wire::DataKind RobotWsCdrDecoder::dataKind(const QString&t){if(t=="/location")return wire::DATA_KIND_VEHICLE_STATE;if(t=="/targets/final_objects")return wire::DATA_KIND_OBSTACLES;if(t=="/chassis_command")return wire::DATA_KIND_CONTROL_COMMAND;if(t=="/chassis_states")return wire::DATA_KIND_CHASSIS_STATE;if(t=="/system_run_states"||t.contains("/_action/"))return wire::DATA_KIND_ACTION_STATE;if(t=="/task_params")return wire::DATA_KIND_TASK_STATE;if(t=="/local_path")return wire::DATA_KIND_LOCAL_TRAJECTORY;if(t=="/global_path")return wire::DATA_KIND_GLOBAL_TRAJECTORY;return wire::DATA_KIND_UNKNOWN;}

bool RobotWsCdrDecoder::decode(const QString&topic,const QString&type,const QByteArray&payload,quint64 ts,wire::VisualizationSnapshot*snapshot,QString*error)
{
    if(!snapshot){if(error)*error=QStringLiteral("snapshot 为空");return false;}if(!isSupported(topic,type)){if(error)*error=QStringLiteral("不支持 %1 (%2)").arg(topic,type);return false;}CdrReader r(payload);if(!r.begin(error))return false;
    // rosbag 的一条 topic 消息与 Server 发送的同名 snapshot 字段语义相同：它是
    // 当前完整值，不是对上一条消息的增量。尤其 Path、障碍物和底盘状态包含 repeated
    // 字段；不先清理就会在本地回放中把旧点/旧状态反复累加。
    if(topic=="/location") snapshot->clear_vehicle_state();
    else if(topic=="/targets/final_objects") snapshot->clear_obstacles();
    else if(topic=="/chassis_command") snapshot->clear_control_command();
    else if(topic=="/chassis_states") snapshot->clear_chassis_state();
    else if(topic=="/task_params") snapshot->clear_task_state();
    else if(topic=="/local_path") snapshot->clear_local_trajectory();
    else if(topic=="/global_path") snapshot->clear_global_trajectory();
    bool ok=false;
    if(topic=="/location")ok=decodeLocation(r,ts,snapshot,error);else if(topic=="/targets/final_objects")ok=decodeObstacles(r,ts,snapshot,error);else if(topic=="/chassis_command")ok=decodeCommand(r,ts,snapshot,error);else if(topic=="/chassis_states")ok=decodeChassis(r,ts,snapshot,error);else if(topic=="/system_run_states")ok=decodeAction(r,ts,snapshot,error);else if(topic=="/task_params")ok=decodeTask(r,ts,snapshot,error);else if(topic=="/local_path")ok=decodeLocalPath(r,ts,snapshot,error);else if(topic=="/global_path")ok=decodeGlobalPath(r,ts,snapshot,error);else if(topic.endsWith("/_action/status"))ok=updateActionStatus(r,ts,snapshot,error);else if(topic.endsWith("/_action/feedback"))ok=updateActionFeedback(r,ts,snapshot,error);
    if(!ok)return false;return r.finished(error);
}

}  // namespace autoviz::playback
