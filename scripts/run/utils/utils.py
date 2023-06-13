import subprocess
from loguru import logger

def run_cmd(cmd, check_output=False, shell=True):
    if isinstance(cmd, str):
        cmd = cmd.split(' ')
    if check_output:
        try:
            subprocess.check_output(cmd, shell=shell)
        except subprocess.CalledProcessError as ex:
            logger.error(f"Failed running command '{cmd}', return code: {ex.returncode}")
    else:
        subprocess.Popen(cmd, shell=shell)
