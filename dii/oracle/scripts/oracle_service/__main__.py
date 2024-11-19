import asyncio
import sys
import dotenv
import os

from . import oracle_service


async def handler(data):
    try:
        return str(eval(data.decode(encoding="utf-8"))).encode(encoding="utf-8")
    except asyncio.CancelledError:
        raise


def main():
    dotenv.load_dotenv()
    oracle_service(
        os.environ["ENDPOINT_URI"],
        os.environ["PRIVATE_KEY"],
        os.environ["ADDRESS"],
        os.environ["ABI"],
        os.environ["TASK_ID"],
        handler,
    )


if __name__ == "__main__":
    sys.exit(main())
