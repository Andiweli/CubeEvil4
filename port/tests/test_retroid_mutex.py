"""Real independent PowerShell processes; no ADB or device mutations."""
import json
import sys
import unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from retroid_adb import RetroidLock,LockUnavailable

class MutexTests(unittest.TestCase):
    def test_exclusion_timeout_and_exception_release(self):
        first=RetroidLock(1000)
        try:
            first.__enter__()
        except LockUnavailable as error:
            self.skipTest('Shared device lock unavailable: '+str(error))
        try:
            with self.assertRaises(LockUnavailable):
                with RetroidLock(100):
                    self.fail('Second independent PowerShell process entered')
            first.ensure_held()
        finally:
            first.close()
        with self.assertRaisesRegex(ValueError,'injected suite failure'):
            with RetroidLock(1000):
                raise ValueError('injected suite failure')
        with RetroidLock(1000) as after:
            after.ensure_held()
        print(json.dumps(dict(mutex='Global\\Retroid_ADB_Exclusive',
              independent_brokers=True,timeout_excluded=True,exception_released=True,
              hardware_access=False)))

if __name__=='__main__':unittest.main()
