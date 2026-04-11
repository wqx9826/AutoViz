#pragma once

#include <memory>
#include <QString>

#include "core/datacenter/DataManager.h"

namespace autoviz::ros {

enum class SubscribeBackend {
    None,
    Ros1,
    Ros2
};

class RosMsgSubscribeBase {
public:
    explicit RosMsgSubscribeBase(datacenter::DataManager* dataManager);
    virtual ~RosMsgSubscribeBase();

    virtual SubscribeBackend backend() const = 0;
    virtual bool initialize(QString* errorMessage = nullptr) = 0;
    virtual bool start(QString* errorMessage = nullptr) = 0;
    virtual void stop() = 0;
    virtual QString statusSummary() const = 0;
    void resetVisualizationData();

protected:  
    // 目的是为了 让子类能够直接访问到 m_dataManager
    datacenter::DataManager* dataManager() const;

private:
    datacenter::DataManager* m_dataManager = nullptr;
};

std::shared_ptr<RosMsgSubscribeBase> createRosMsgSubsrcribe(SubscribeBackend backend, datacenter::DataManager* dataManager);

}  // namespace autoviz::ros
