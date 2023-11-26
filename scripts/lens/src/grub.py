from .utils import Cmd, run_cmd, curr_time, chdir
from loguru import logger
from pathlib import Path
import platform


def wrap_grub_config(grub):
    assert isinstance(grub, list)
    grub = ["### LENS grub start"] + grub + ["### LENS grub end"]


def __setup_netserver():
    config = [
        'GRUB_CMDLINE_LINUX="nokaslr memmap=16G!16G memmap=16G!48G log-buf-len=1G mitigations=off"'
    ]
    config = wrap_grub_config(config)
    logger.info(f"Writing grub config:\n{config}")
    with open(grub_file, "a") as f:
        pass


grub_setup = {
    "netserver": __setup_netserver,
    "lab-pc": __setup_netserver,
}

grub_file = Path("/etc/default/grub")
grub_backup_dir = Path("/etc/default/grub_backup")


def backup():
    logger.info("Backing up grub config")
    Cmd().sudo().mkdir(grub_backup_dir).run()

    backup_file = grub_backup_dir / f"{curr_time()}.grub"
    Cmd().sudo().run(f"cp {grub_file} {backup_file}")


def check_exist():
    with open(grub_file, "r") as f:
        for l in f:
            if l.startswith("### LENS grub start"):
                return True
    return False


def setup():
    hostname = platform.node()
    if hostname not in grub_setup.keys():
        raise Exception(f"Host [{hostname}] is not supported")

    if check_exist():
        logger.info("Grub config exists, skip")
        return

    logger.info("Get sudo permission")
    Cmd().sudo("echo").run()

    backup()

    logger.info(f"Set up grub for host {hostname}")
    grub_setup[hostname]()
