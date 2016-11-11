#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "gmock/gmock-matchers.h"
#include "MissionTestHelpers.h"
#include "mission/antenna.h"
#include "rapidcheck.hpp"
#include "rapidcheck/gtest.h"
#include "state/state.h"
#include "system.h"
#include "time/TimeSpan.hpp"

using testing::Test;
using testing::Eq;

class DeployAntennaActionTest : public Test
{
  public:
    DeployAntennaActionTest() : runnable(false)
    {
        SystemStateEmpty(&state);

        AntennaInitializeActionDescriptor(&openAntenna);
    }

  protected:
    SystemActionDescriptor openAntenna;
    bool runnable;

    SystemState state;

    void DetermineActions();
};

void DeployAntennaActionTest::DetermineActions()
{
    SystemActionDescriptor actionDescriptors[] = {openAntenna};
    SystemActionDescriptor* runnableDescriptors[1] = {0};

    size_t count = SystemDetermineActions(&state, actionDescriptors, COUNT_OF(actionDescriptors), runnableDescriptors);

    runnable = count > 0;
}

TEST_F(DeployAntennaActionTest, OpenDeployAfterSilencePhaseIfNotAlreadyOpenned)
{
    state.Time = TimePointFromTimeSpan(TimeSpanAdd(TimeSpanFromMinutes(30), TimeSpanFromSeconds(1)));

    DetermineActions();

    ASSERT_THAT(runnable, Eq(true));
}

TEST_F(DeployAntennaActionTest, ShouldNotDeployAntennaInSilencePhase)
{
    state.Time = TimePointFromTimeSpan(TimeSpanAdd(TimeSpanFromMinutes(20), TimeSpanFromSeconds(1)));

    DetermineActions();

    ASSERT_THAT(runnable, Eq(false));
}

TEST_F(DeployAntennaActionTest, ShouldNotDeployAntennaIfAlreadyOpenned)
{
    state.Time = TimePointFromTimeSpan(TimeSpanAdd(TimeSpanFromMinutes(30), TimeSpanFromSeconds(1)));
    state.AntennaDeployed = true;

    DetermineActions();

    ASSERT_THAT(runnable, Eq(false));
}

RC_GTEST_FIXTURE_PROP(DeployAntennaActionTest, CanOpenAntennaOnlyAfterSilentPhaseAndIfNotAlreadyOpenned, (const SystemState& state))
{
    this->state = state;

    DetermineActions();

    if (runnable)
    {
        RC_ASSERT(TimePointFromTimeSpan(TimeSpanFromMinutes(30)) < state.Time);
        RC_ASSERT(!state.AntennaDeployed);
    }
}