from loguru import logger
import platform
import sys


def setup_logger():
    logger.configure(
        handlers=[
            dict(
                sink=sys.stdout,
                format=(
                    "[<green>{extra[hostname]}</green>] "
                    "<green>{time:YYYY-MM-DD HH:mm:ss}</green>"
                    " | <level>{level: <8}</level> | "
                    "<level>{message}</level>"
                ),
                colorize=True,
            )
        ],
        extra=dict(hostname=platform.node()),
    )
    logger.level(name="CMD", no=10, color="<magenta><bold>")
    logger.level(name="SCRIPT", no=10, color="<magenta><bold>")
