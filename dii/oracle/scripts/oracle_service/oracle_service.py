import asyncio

import hexbytes
import web3

from .events import OracleEvents


def oracle_service(
    endpoint_uri, private_key, address, abi, task_id, handler, poll_interval=1
):
    event_loop = asyncio.new_event_loop()
    w3 = web3.Web3(web3.HTTPProvider(endpoint_uri=endpoint_uri))
    account = web3.Account.from_key(private_key)
    w3.middleware_onion.inject(
        web3.middleware.SignAndSendRawMiddlewareBuilder.build(account), layer=0
    )
    contract = w3.eth.contract(address=address, abi=abi)
    log_filters = [
        contract.events[event["name"]].create_filter(from_block="latest")
        for event in contract.events._events
    ]
    requests = {}
    while True:
        for log_filter in log_filters:
            for log_receipt in log_filter.get_new_entries():
                if log_receipt.event == OracleEvents.SENT_REQUEST:
                    if (
                        hexbytes.HexBytes(log_receipt.args.taskId).to_0x_hex()
                        != task_id
                        or log_receipt.args.requestId in requests
                    ):
                        continue
                    requests[log_receipt.args.requestId] = event_loop.create_task(
                        handler(log_receipt.args.data)
                    )
                elif log_receipt.event == OracleEvents.CANCELLED_REQUEST:
                    if log_receipt.args.requestId not in requests:
                        continue
                    requests[log_receipt.args.requestId].cancel()
        event_loop.run_until_complete(asyncio.sleep(poll_interval))
        for request_id, request in requests.copy().items():
            try:
                data = request.result()
            except asyncio.InvalidStateError:
                continue
            except Exception:
                pass
            else:
                contract.functions.fulfillRequest(request_id, data).transact(
                    {"from": account.address}
                )
            del requests[request_id]
