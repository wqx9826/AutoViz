#pragma once

#include "autoviz/transport.pb.h"
#include "core/datacenter/DataManager.h"

namespace autoviz::network {

class ProtocolModelConverter {
public:
    static datacenter::VisualizationSnapshot toModelSnapshot(
        const ::autoviz::VisualizationSnapshot& source);
};

}  // namespace autoviz::network
