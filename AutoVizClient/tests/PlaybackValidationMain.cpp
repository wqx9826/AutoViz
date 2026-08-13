#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTextStream>

#include "core/playback/RobotWsCdrDecoder.h"

int main(int argc,char**argv)
{
    QCoreApplication app(argc,argv);QTextStream out(stdout),err(stderr);
    for(const bool little:{false,true}){QByteArray payload(64,0);payload[1]=little?char(1):char(0);::autoviz::VisualizationSnapshot snapshot;QString detail;if(!autoviz::playback::RobotWsCdrDecoder::decode("/task_params","custom_msgs/msg/TaskParams",payload,1,&snapshot,&detail)){err<<"CDR endian self-test failed: "<<detail<<'\n';return 1;}QByteArray truncated=payload.left(12);if(autoviz::playback::RobotWsCdrDecoder::decode("/task_params","custom_msgs/msg/TaskParams",truncated,1,&snapshot,&detail)){err<<"CDR truncation self-test failed\n";return 1;}payload[6]=char(2);if(autoviz::playback::RobotWsCdrDecoder::decode("/task_params","custom_msgs/msg/TaskParams",payload,1,&snapshot,&detail)){err<<"CDR bool validation self-test failed\n";return 1;}}
    out<<"CDR endian/truncation/bool self-tests: OK\n";const QStringList args=app.arguments().mid(1);if(args.isEmpty()){err<<"usage: AutoVizClientPlaybackTests BAG_DIR...\n";return 2;}
    int failures=0;for(int ai=0;ai<args.size();++ai){const QDir dir(args[ai]);const auto files=dir.entryList({"*.db3"},QDir::Files,QDir::Name);if(files.isEmpty()){err<<dir.absolutePath()<<": no db3\n";++failures;continue;}qint64 total=0;
        for(int fi=0;fi<files.size();++fi){const QString cn=QStringLiteral("validation_%1_%2").arg(ai).arg(fi);{QSqlDatabase db=QSqlDatabase::addDatabase("QSQLITE",cn);db.setDatabaseName(dir.filePath(files[fi]));if(!db.open()){err<<files[fi]<<": "<<db.lastError().text()<<'\n';++failures;continue;}QSqlQuery check(db);if(!check.exec("PRAGMA quick_check")||!check.next()||check.value(0).toString()!="ok"){err<<files[fi]<<": quick_check failed\n";++failures;continue;}QSqlQuery q(db);q.setForwardOnly(true);if(!q.exec("SELECT m.timestamp,t.name,t.type,m.data FROM messages m JOIN topics t ON t.id=m.topic_id WHERE t.name IN ('/location','/targets/final_objects','/chassis_command','/chassis_states','/system_run_states','/task_params','/local_path','/global_path') ORDER BY m.timestamp")){err<<q.lastError().text()<<'\n';++failures;continue;}while(q.next()){const QString topic=q.value(1).toString(),type=q.value(2).toString();::autoviz::VisualizationSnapshot snapshot;QString detail;if(!autoviz::playback::RobotWsCdrDecoder::decode(topic,type,q.value(3).toByteArray(),q.value(0).toULongLong(),&snapshot,&detail)){err<<dir.dirName()<<' '<<topic<<" row "<<(total+1)<<": "<<detail<<'\n';++failures;break;}++total;}db.close();}QSqlDatabase::removeDatabase(cn);if(failures)break;}
        if(!failures)out<<dir.dirName()<<": OK, "<<total<<" supported messages\n";
    }return failures?1:0;
}
