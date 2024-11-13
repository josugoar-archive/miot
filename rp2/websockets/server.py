#!/usr/bin/env python

import asyncio
import websockets

async def echo(websocket, path):
    data = await websocket.recv()
    print(f"< {data}")

    await websocket.send(data)

start_server = websockets.serve(echo, "192.168.8.125", 8765)

asyncio.get_event_loop().run_until_complete(start_server)
asyncio.get_event_loop().run_forever()
