#pragma once

#include "autoviz/transport.pb.h"
#include "core/datacenter/DataManager.h"

namespace autoviz::network {

class ProtocolModelConverter {
public:
    static datacenter::VisualizationSnapshot toModelSnapshot(
        const protocol::v1::VisualizationSnapshot& source);
    static void applyUpdate(const protocol::v1::ChannelUpdate& update,
                            datacenter::DataManager* dataManager);
};

}  // namespace autoviz::network
