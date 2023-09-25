from loguru import logger
from contextlib import chdir  # Available starting from python 3.11
import os
import subprocess
from .utils import run_cmd


def build_modules(config):
    with chdir("src"):
        logger.info(f"Src dir: {os.getcwd()}")

        logger.info("Compiling")
        run_cmd(f"make -j {os.cpu_count()}")


def mount_modules(config):
    with chdir("src"):
        cmd = 'sudo bash -c "echo 0 > /proc/sys/kernel/soft_watchdog"'
        run_cmd(
            cmd, popen=True, shell=True, comment="Hard lock watchdog at nmi_watchdog"
        )


def setup_kernel_modules(config):
    logger.info("Building the kernel module")
    build_modules(config)

    logger.info("Mounting the kernel module")
