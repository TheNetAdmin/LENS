import subprocess
from loguru import logger
from contextlib import contextmanager
import os
from pathlib import Path
from enum import Enum


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


def __run_cmd(cmd, *args, **kwargs):
    return subprocess.run(cmd, *args, **kwargs)


def make_cmd(cmd):
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
    *args,
    **kwargs,
):
    # Form a command
    if not cmd:
        assert script
        cmd = f"bash {script}"

    if type(cmd) is not list:
        cmd = make_cmd(cmd)

    # Print
    if script:
        with open(script, "r") as f:
            for line in f:
                logger.log("SCRIPT", f"    {line.rstrip()}")

    comment = f"# {comment}" if comment else ""
    logger.log("CMD", " ".join(cmd) + comment)

    # Execute
    try:
        return __run_cmd(cmd, check=check, *args, **kwargs)
    except subprocess.CalledProcessError as e:
        match on_cmd_fail:
            case cmd_fail.capture_and_exit:
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


class cmd(object):
    def __init__(self, cmd, parent_runner = None) -> None:
        self.cmd = make_cmd(self.setup_cmd(cmd))
        self.parent_cmd = parent_runner.cmd if parent_runner else []

        if parent_runner:
            self.cmd = self.parent_cmd + self.cmd

    def setup_cmd(self, cmd):
        return cmd

    def __enter__(self):
        return self
    
    def run(self, cmd, *args, **kwargs):
        cmd = self.cmd + make_cmd(cmd)
        return run_cmd(cmd, *args, **kwargs)

    def __exit__(self, exc_type, exc_value, exc_tb):
        self.cmd = self.parent_cmd

class ssh(cmd):
    def setup_cmd(self, cmd):
        return f"ssh {cmd} --"
