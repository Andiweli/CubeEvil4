"""Cross-project Windows mutex held by PowerShell for the whole hardware suite.

Keep byte-identical copies in BioShock/tools and RE4-Fixed/port/tools. There is
no environment flag that bypasses acquisition. Local-only invocations skip it.
"""
import functools
import json
import os
from pathlib import Path
import queue
import re
import shutil
import subprocess
import sys
import threading
import time
from datetime import datetime, timezone

MUTEX_NAME = r'Global\Retroid_ADB_Exclusive'


class LockUnavailable(RuntimeError):
    pass


def powershell():
    bundled = Path(r'C:\Users\andiw\.cache\codex-runtimes\codex-primary-runtime\dependencies\native\powershell\pwsh.exe')
    candidate = shutil.which('pwsh') or (str(bundled) if bundled.is_file() else shutil.which('powershell'))
    if not candidate:
        raise LockUnavailable('PowerShell unavailable')
    return candidate


def lock_script():
    directory = Path(__file__).resolve().parent
    candidates = (directory.parent / 'android/scripts/Retroid-Adb-Exclusive.ps1',
                  directory.parent / 'scripts/Retroid-Adb-Exclusive.ps1')
    for candidate in candidates:
        if candidate.is_file():
            return candidate
    raise LockUnavailable('Shared mutex script missing')


class RetroidLock:
    def __init__(self, timeout_ms=60000):
        if not 0 <= timeout_ms <= 300000:
            raise ValueError('Mutex timeout must be 0..300000 ms')
        self.timeout_ms = timeout_ms
        self.process = None
        self.evidence = dict(mutex=MUTEX_NAME, timeout_ms=timeout_ms, status='pending')

    def __enter__(self):
        if os.name != 'nt':
            raise LockUnavailable('Windows System.Threading.Mutex is required')
        self.process = subprocess.Popen([powershell(), '-NoLogo', '-NoProfile', '-NonInteractive',
            '-ExecutionPolicy', 'Bypass', '-File', str(lock_script()), '-Broker',
            '-TimeoutMs', str(self.timeout_ms)], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
            stderr=subprocess.PIPE, text=True, encoding='utf-8', creationflags=subprocess.CREATE_NO_WINDOW)
        # Bound startup too: ReadLine must not hang if PowerShell fails to start.
        result = queue.Queue()
        threading.Thread(target=lambda: result.put(self.process.stdout.readline()), daemon=True).start()
        try:
            line = result.get(timeout=self.timeout_ms / 1000 + 15)
            record = json.loads(line)
            if record.get('status') != 'acquired' or record.get('mutex') != MUTEX_NAME:
                raise LockUnavailable('Unexpected mutex broker response')
            self.evidence.update(record, acquired_at_utc=datetime.now(timezone.utc).isoformat())
            self.ensure_held()
            return self
        except (queue.Empty, ValueError, LockUnavailable) as error:
            self.close()
            raise LockUnavailable(f'Mutex not acquired: {error}; {self.evidence.get("broker_stderr", "")}') from error

    def ensure_held(self):
        if self.process is None or self.process.poll() is not None:
            raise LockUnavailable('Mutex owner died; further ADB operations prohibited')

    def close(self):
        if self.process is None:
            return
        try:
            if self.process.poll() is None:
                try:
                    self.process.stdin.write('release\n')
                    self.process.stdin.flush()
                except (OSError, ValueError):
                    pass
                try:
                    self.process.stdin.close()
                except OSError:
                    pass  # Broken broker pipe after timeout/owner termination.
            self.process.wait(timeout=10)
        except subprocess.TimeoutExpired:
            self.process.terminate()
            self.process.wait(timeout=10)
        finally:
            self.evidence['broker_exit_code'] = self.process.returncode
            self.evidence['broker_stderr'] = self.process.stderr.read()
            self.evidence['released_at_utc'] = datetime.now(timezone.utc).isoformat()
            self.process.stdout.close()
            self.process.stderr.close()
            if not self.process.stdin.closed:
                try:
                    self.process.stdin.close()
                except OSError:
                    pass

    def __exit__(self, *exception):
        self.close()


def option(name, default=None):
    for index, value in enumerate(sys.argv[1:], 1):
        if value == name and index + 1 < len(sys.argv):
            return sys.argv[index + 1]
        if value.startswith(name + '='):
            return value[len(name) + 1:]
    return default


def hardware_sequence(*, always=False, default_serial=None, default_adb=None):
    """Guard a whole main(), including its existing error/log/cleanup paths.

    ADB commands are additionally checked immediately before invocation. A
    timed-out acquisition writes a separate lock journal, never overwrites a
    previous successful hardware report and never calls the original main().
    """
    def decorate(function):
        @functools.wraps(function)
        def guarded(*args, **kwargs):
            if '-h' in sys.argv or '--help' in sys.argv or (not always and option('--adb') is None):
                return function(*args, **kwargs)
            adb = option('--adb', default_adb)
            serial = option('--serial', default_serial)
            if not adb or not serial or not re.fullmatch(r'[A-Za-z0-9_.:-]+', serial):
                print('HARDWARE_PENDING: explicit valid --adb/--serial required; no ADB executed.', file=sys.stderr)
                return 73
            timeout_ms = int(os.environ.get('RETROID_ADB_LOCK_TIMEOUT_MS', '60000'))
            folder = Path(__file__).resolve().parent.parent / 'build/adb-coordination'
            folder.mkdir(parents=True, exist_ok=True)
            stamp = datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S.%fZ')
            journal = dict(runner=str(Path(sys.argv[0]).resolve()), serial=serial,
                           pid=os.getpid(), hardware_status='pending', commands=[])
            lease = RetroidLock(timeout_ms)
            original_run = subprocess.run
            def checked_run(argv, *call_args, **call_kwargs):
                if isinstance(argv, (list, tuple)) and str(argv[0]).lower() == str(adb).lower():
                    lease.ensure_held()
                    if len(argv) < 4 or list(map(str, argv[1:3])) != ['-s', serial]:
                        raise RuntimeError('Every ADB invocation must use the verified -s SERIAL')
                    if str(argv[3]) in ('kill-server', 'start-server', 'reboot', 'root', 'unroot'):
                        raise RuntimeError('Global ADB/device management prohibited in a test suite')
                    journal['commands'].append(list(map(str, argv)))
                return original_run(argv, *call_args, **call_kwargs)
            try:
                with lease:
                    subprocess.run = checked_run
                    verified = checked_run([str(adb), '-s', serial, 'get-state'], capture_output=True,
                        text=True, encoding='utf-8', errors='replace', timeout=30)
                    journal['device_check'] = dict(exit_code=verified.returncode,
                        stdout=verified.stdout, stderr=verified.stderr)
                    if verified.returncode or verified.stdout.strip() != 'device':
                        raise LockUnavailable('Requested ADB serial is not connected/authorized')
                    journal['hardware_status'] = 'suite_started'
                    value = function(*args, **kwargs)
                    journal['suite_return_code'] = value
                    journal['hardware_status'] = 'suite_finished'  # Not a hardware pass claim.
                    return value
            except LockUnavailable as error:
                journal.update(hardware_status='hardware_pending', reason=str(error))
                print('HARDWARE_PENDING: ' + str(error), file=sys.stderr)
                return 73
            except BaseException as error:
                journal.update(hardware_status='suite_error', reason=repr(error))
                raise
            finally:
                subprocess.run = original_run
                journal['lock'] = lease.evidence
                (folder / f'{Path(sys.argv[0]).stem}_{stamp}_{os.getpid()}.json').write_text(
                    json.dumps(journal, indent=2) + '\n', encoding='utf-8')
        return guarded
    return decorate
