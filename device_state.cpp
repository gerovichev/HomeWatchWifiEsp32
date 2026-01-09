#include "device_state.h"

DeviceState& DeviceState::getInstance() {
    static DeviceState instance;
    return instance;
}

