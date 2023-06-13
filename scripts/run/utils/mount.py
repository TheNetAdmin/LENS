from loguru import logger
from contextlib import chdir # Available starting from python 3.11
import os
import subprocess

def _build_modules(config):
    with chdir("src"):
        logger.info(f"Src dir: {os.getcwd()}")

        cmd = 'sudo bash -c "echo 0 > /proc/sys/kernel/soft_watchdog"'
        logger.info(f"Hard lock watchdog at nmi_watchdog: '{cmd}'")
        subprocess.Popen(cmd, shell=True)

        logger.info("Compiling")
        subprocess.run(["make", "-j", f"os.cpu_count()"], shell=True)


def mount_kernel_modules(config):
    logger.info("Building the kernel module")
    _build_modules(config)
    logger.info("Mounting the kernel module")
