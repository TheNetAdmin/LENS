#! /usr/bin/env python3
import click
import src.repo as repo
from config.config import config
from loguru import logger
from src.logging import setup_logger
from src.mount import setup_kernel_modules
from src.utils import Cmd, run_cmd
import src.grub as grub


@click.group()
@click.option("--host", help="Remote hostname", default="netserver")
@click.option("--remote_dir", help="Remote repo dir", default="$HOME/code/generic-lens")
@click.pass_context
def lens(ctx, host, remote_dir):
    ctx.ensure_object(dict)
    ctx.obj["config"] = config
    ctx.obj["host"] = host
    ctx.obj["remote_dir"] = remote_dir
    logger.info("Launching a LENS job")


@lens.command()
@click.pass_context
def setup(ctx):
    setup_kernel_modules(ctx.obj["config"])


@lens.command()
@click.pass_context
def cleanup(ctx):
    pass


@lens.command()
@click.pass_context
def run(ctx):
    pass


@lens.command(
    context_settings=dict(
        ignore_unknown_options=True,
    )
)
@click.pass_context
@click.argument("remote_lens_args", nargs=-1, type=click.UNPROCESSED)
def remote(ctx, remote_lens_args):
    ctx.invoke(sync)

    logger.info("Execute command on remote host")
    with Cmd().ssh(ctx.obj["host"]).cd(ctx.obj["remote_dir"]) as c:
        c.conda("lens")
        c.run(["./scripts/run/lens.py"] + list(remote_lens_args))


@lens.command()
@click.pass_context
def sync(ctx):
    # TODO: make the auto update to a separate branch, and periodically merge back to the main branch
    logger.info("Sync repo: [local] git push, [remote] git pull")

    logger.info("Localhost: Push repo")
    repo.commit_and_push()
    repo.status()

    logger.info("Remotehost: Pull repo")
    repo.pull(ctx.obj["host"], ctx.obj["remote_dir"])
    repo.status(ctx.obj["host"], ctx.obj["remote_dir"])


@lens.command()
@click.pass_context
def test(ctx):
    logger.info("Testing message")
    with Cmd() as c:
        c("pwd")


@lens.group()
@click.pass_context
def machine(ctx):
    pass


@machine.group(name="setup")
@click.pass_context
def machine_setup(ctx):
    pass


@machine_setup.command(name="grub")
@click.pass_context
def setup_grub(ctx):
    logger.info("Set up grub")
    grub.setup()


if __name__ == "__main__":
    setup_logger()
    lens()
