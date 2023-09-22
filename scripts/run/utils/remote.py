from contextlib import contextmanager
from .utils import run_cmd, make_cmd

class ssh_runner(object):
    def __init__(self, hostname: str):
        self.hostname = hostname
    
    def run_cmd(self, cmd, *args, **kwargs):
        cmd = make_cmd(cmd)
        cmd = ["ssh", self.hostname, "--"] + cmd
        return run_cmd(cmd, *args, **kwargs)

# https://packetpushers.net/using-python-context-managers/
@contextmanager
def ssh_manager(hostname):
    runner = ssh_runner(hostname)
    yield runner
