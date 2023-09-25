import os
import subprocess
from contextlib import contextmanager
from datetime import datetime
from enum import Enum
from pathlib import Path

from loguru import logger


@contextmanager
def chdir(path, mkdir=False):
    origin = Path().absolute()
    target = Path(path).absolute()
    try:
        if mkdir:
            run_cmd(f"mkdir -p {target}")
        logger.log("CMD", f"pushd {target}")
        os.chdir(target)
        yield
    finally:
        logger.log("CMD", f"popd #{origin}")
        os.chdir(origin)


def __run_cmd(cmd, popen=False, *args, **kwargs):
    if popen:
        return subprocess.popen(cmd, *args, **kwargs)
    else:
        return subprocess.run(cmd, *args, **kwargs)


def make_cmd(cmd):
    if isinstance(cmd, list):
        return cmd
    else:
        return cmd.split(" ")


class cmd_fail(Enum):
    capture_and_exit = 1
    raise_exception = 2


def run_cmd(
    cmd,
    script=None,
    comment=None,
    check=True,
    on_cmd_fail=cmd_fail.capture_and_exit,
    pretty_print_exception=False,
    popen=False,
    *args,
    **kwargs,
):
    # Form a command
    if not cmd:
        assert script
        cmd = f"bash {script}"

    # popen accepts cmd as a single string
    if not popen and not type(cmd) is list:
        cmd = make_cmd(cmd)

    # Print
    if script:
        with open(script, "r") as f:
            for line in f:
                logger.log("SCRIPT", f"    {line.rstrip()}")

    comment = f"# {comment}" if comment else ""
    cmd_print = cmd if popen else " ".join(cmd)
    logger.log("CMD", cmd_print + comment)

    # Execute
    try:
        return __run_cmd(cmd, popen=popen, check=check, *args, **kwargs)
    except subprocess.CalledProcessError as e:
        match on_cmd_fail:
            case cmd_fail.capture_and_exit:
                if pretty_print_exception:
                    logger.exception(e)
                exit(e.returncode)
            case cmd_fail.raise_exception:
                raise e


def run_cmd_list(cmds, *args, **kwargs):
    return [run_cmd(cmd, *args, **kwargs) for cmd in cmds]


def run_cmd_script(cmds, script="lens_script.sh", *args, **kwargs):
    script = Path(script).resolve()
    try:
        with open(script, "w") as f:
            cmds = ["#! /bin/bash"] + cmds
            f.write("\n".join(cmds))
        return run_cmd(script=script, *args, **kwargs)
    finally:
        script.unlink()


class Cmd(object):
    def __init__(self, cmd = None, parent_runner = None) -> None:
        self.cmds = []
        if parent_runner:
            self.cmds = parent_runner.cmds
        if cmd:
            self.cmds.append(make_cmd(cmd))

    def setup_cmd(self, cmd):
        return cmd

    def chain(self, cmd):
        self.cmds.append(make_cmd(cmd))
        return self

    def __enter__(self):
        return self
    
    def run(self, cmd = None, *args, **kwargs):
        cmd_to_run = [c for m in self.cmds for c in m]
        if cmd:
            cmd_to_run += make_cmd(cmd)
        return run_cmd(cmd_to_run, *args, **kwargs)

    def print(self):
        logger.info(f"cmd: {self.cmds}")

    def __exit__(self, exc_type, exc_value, exc_tb):
        if len(self.cmds) > 0:
            self.cmds.pop()

    def __call__(self, *args, **kwargs):
        return self.run(*args, **kwargs)

    def ssh(self, hostname):
        return self.chain(f"ssh {hostname} --")

    def cd(self, dir):
        return self.chain(f"cd {dir} &&")

    def conda(self, name):
        return self.chain(f". ~/.bashrc ; conda activate {name} &&")


def curr_time():
    return datetime.now().strftime("%Y-%m-%d-%H-%M-%S")
