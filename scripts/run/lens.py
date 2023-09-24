#! /usr/bin/env python3
import click
from config.config import config
from loguru import logger
from utils.logging import setup_logger
from utils.mount import mount_kernel_modules
from utils.utils import Cmd


@click.group()
@click.pass_context
def lens(ctx):
    ctx.ensure_object(dict)
    ctx.obj["config"] = config
    logger.info("Launching a LENS job")


@lens.command()
@click.pass_context
def mount(ctx):
    mount_kernel_modules(ctx.obj["config"])


@lens.command()
@click.pass_context
def umount(ctx):
    pass


@lens.command()
@click.pass_context
def run(ctx):
    pass


@lens.command()
@click.pass_context
def test(ctx):
    logger.info("This is a testing message")
    with Cmd().ssh("netserver") as c:
        with c.cd("$HOME/code"):
            c("pwd")
        with c.cd("$HOME/code"):
            c("pwd")
            c("ls")
        c("pwd")


if __name__ == "__main__":
    setup_logger()
    lens()
