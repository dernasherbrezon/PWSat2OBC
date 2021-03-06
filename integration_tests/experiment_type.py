from enum import unique, IntEnum


@unique
class ExperimentType(IntEnum):
    Detumbling = 1
    EraseFlash = 2
    SunS = 3
    LEOP = 4
    RadFET = 5
    SADS = 6
    Sail = 7
    Fibo = 8
    Payload = 9
    Camera = 10


@unique
class StartResult(IntEnum):
    Success = 0
    Failure = 1


@unique
class IterationResult(IntEnum):
    NoResult = 0
    Finished = 1
    LoopImmediately = 2
    WaitForNextCycle = 3
    Failure = 4
