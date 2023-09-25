from .utils import Cmd, run_cmd, curr_time, chdir
from loguru import logger
import platform


def __setup_netserver():
    pass
    # logger.info("Writing grub config")
    # with open(grub_file, 'w') as f:
    #     pass


grub_setup = {
    "netserver": __setup_netserver,
    "lab-pc": __setup_netserver,
}

grub_file = "/etc/default/grub"
grub_backup_dir = "/etc/default/grub_backup"

def backup():
    logger.info("Backing up grub config")
    Cmd().sudo().mkdir(grub_backup_dir).run()

    backup_file = f'{curr_time()}.grub'
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
