import os

from dotenv import load_dotenv
from web3 import Web3
from web3.middleware import SignAndSendRawMiddlewareBuilder


# TODO: CancelledRequest
def oracle_service(
    self,
    endpoint_uri,
    private_key,
    address,
    abi,
    task_id,
    handler,
):
    w3 = Web3(Web3.HTTPProvider(endpoint_uri=endpoint_uri))
    w3.middleware_onion.inject(
        SignAndSendRawMiddlewareBuilder.build(private_key), layer=0
    )
    oracle = w3.eth.contract(address=address, abi=abi)
    sent_requests = oracle.events.SentRequest.create_filter(
        from_block="latest", argument_filters={"taskId": task_id}
    )
    while True:
        for sent_request in sent_requests.get_new_entries():
            oracle.functions.fulfillRequest(
                sent_request["args"]["requestId"],
                handler(sent_request["args"]["data"]),
            ).transact()


def main():
    load_dotenv()
    oracle_service(
        os.environ["ENDPOINT_URI"],
        os.environ["PRIVATE_KEY"],
        os.environ["ADDRESS"],
        os.environ["ABI"],
        os.environ["TASK_ID"],
        eval(os.environ["HANDLER"]),
    )
