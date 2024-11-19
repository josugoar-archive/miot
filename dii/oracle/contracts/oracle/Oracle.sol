// SPDX-License-Identifier: MIT
pragma solidity ^0.8.18;

import {IOracle} from "./interfaces/IOracle.sol";

contract Oracle is IOracle {
    struct Callback {
        address addr;
        bytes4 selector;
    }

    address public owner;
    mapping(bytes32 requestId => Callback callback) public callbacks;

    constructor(address owner_) {
        require(owner_ != address(0), "Invalid owner address");
        owner = owner_;
    }

    function setOwner(address owner_) external {
        require(owner == msg.sender, "Unauthorized sender");
        require(owner_ != address(0), "Invalid owner address");
        owner = owner_;
    }

    function sendRequest(
        bytes4 callbackSelector,
        bytes32 taskId,
        bytes32 requestId,
        bytes calldata data
    ) external override {
        require(callbacks[requestId].addr == address(0), "Invalid request");
        callbacks[requestId] = Callback({
            addr: msg.sender,
            selector: callbackSelector
        });
        emit SentRequest(taskId, requestId, data);
    }

    function cancelRequest(bytes32 requestId) external {
        require(callbacks[requestId].addr == msg.sender, "Invalid request");
        delete callbacks[requestId];
        emit CancelledRequest(requestId);
    }

    function fulfillRequest(
        bytes32 requestId,
        bytes calldata data
    ) external override returns (bool success) {
        require(owner == msg.sender, "Unauthorized sender");
        require(callbacks[requestId].addr != address(0), "Invalid request");
        Callback memory callback = callbacks[requestId];
        delete callbacks[requestId];
        (success, ) = callback.addr.call(
            abi.encodeWithSelector(callback.selector, requestId, data)
        );
        return success;
    }
}
