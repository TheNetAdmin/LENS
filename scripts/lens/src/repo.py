from loguru import logger

from .utils import Cmd, run_cmd, curr_time


def status(remote_host=None, remote_dir=None):
    if remote_host:
        with Cmd().ssh(remote_host).cd(remote_dir) as c:
            c("git status")
    else:
        run_cmd("git status")


def commit_and_push():
    with Cmd("git") as c:
        c("add *")
        res = c("diff-index HEAD --", capture_output=True, text=True)
        res = res.stdout.strip().split("\n")
        res = [r for r in res if r != ""]
        num_diff_files = len(res)

        if num_diff_files > 0:
            logger.info(f"{num_diff_files} files modified, needs commit and push")
            commit_msg = f"Auto commit [{curr_time()}]"
            c(["commit", "-am", f"{commit_msg}"])
            c("push")


def pull(remote_host, remote_dir):
    with Cmd().ssh(remote_host).cd(remote_dir) as c:
        c("git pull")
