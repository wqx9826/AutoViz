#pragma once

#include "core/ros/RosMsgSubscribeBase.h"

namespace autoviz::ros {

class Ros1MsgSubsrcribe : public RosMsgSubscribeBase {
public:
    explicit Ros1MsgSubsrcribe(datacenter::DataManager* dataManager);

    SubscribeBackend backend() const override;
    bool initialize(QString* errorMessage = nullptr) override;
    bool start(QString* errorMessage = nullptr) override;
    void stop() override;
    QString statusSummary() const override;

private:
    bool m_running = false;
};

}  // namespace autoviz::ros
