#include <ostream>
#include "mission/mission.h"

void showValue(const SystemState& state, std::ostream& os)
{
    os << "time=" << state.Time << ", "
       << "antennaDeployed=" << state.AntennaDeployed;
}
