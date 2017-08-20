from fs import *
from comm import *
from experiments import *
from time import *
from program_upload import *
from boot_settings import *
from power import *
from fdir import *
from sail import *
from periodic_message import *
from antenna import *
from i2c import *
from suns import *
from photo import *
from sads import *
from state import *

__all__ = [
    'DownloadFile',
    'EnterIdleState',
    'RemoveFile',
    'PerformDetumblingExperiment',
    'SetTimeCorrectionConfig',
    'EraseBootTableEntry',
    'WriteProgramPart',
    'FinalizeProgramEntry',
    'ListFiles',
    'SetBootSlots',
    'SendBeacon',
    'PowerCycleTelecommand',
    'SetErrorCounterConfig',
    'OpenSailTelecommand',
    'GetErrorCounterConfig',
    'SetPeriodicMessageTelecommand',
    'StopAntennaDeployment',
    'EraseFlash'
    'RawI2C',
    'GetSunSDataSets',
    'PerformSailExperiment',
    'TakePhotoTelecommand',
    'DeploySolarArrayTelecommand',
    'GetPersistentState'
]

