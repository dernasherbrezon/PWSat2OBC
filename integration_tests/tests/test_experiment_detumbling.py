import logging
from datetime import timedelta, datetime

import telecommand
from obc.experiments import ExperimentType
from response_frames.operation import OperationSuccessFrame
from system import auto_power_on, runlevel
from tests.base import BaseTest, RestartPerTest
from utils import TestEvent


class TestExperimentDetumbling(RestartPerTest):
    @auto_power_on(auto_power_on=False)
    def __init__(self, *args, **kwargs):
        super(TestExperimentDetumbling, self).__init__(*args, **kwargs)

    def _start(self):
        e = TestEvent()

        def on_reset(_):
            e.set()
            return False
        self.system.comm.on_hardware_reset = on_reset

        self.power_on_obc()
        self.system.obc.wait_to_start()

        e.wait_for_change(1)

    @runlevel(2)
    def test_should_perform_experiment(self):
        self._start()

        log = logging.getLogger("TEST")

        start_time = datetime.now()
        self.system.rtc.set_response_time(start_time)

        log.info('Sending telecommand')
        self.system.comm.put_frame(telecommand.PerformDetumblingExperiment(correlation_id=5, duration=timedelta(hours=4), sampling_interval=timedelta(seconds=2)))

        response = self.system.comm.get_frame(5)
        self.assertIsInstance(response, OperationSuccessFrame)

        log.info('Waiting for experiment')
        self.system.obc.wait_for_experiment(ExperimentType.Detumbling, 40)

        log.info('Advancing time')
        self.system.obc.advance_time(timedelta(hours=4, minutes=1))
        self.system.rtc.set_response_time(start_time + timedelta(hours=4, minutes=1))

        log.info('Waiting for experiment finish')
        self.system.obc.wait_for_experiment(None, 25)
