// SPDX-License-Identifier: MIT
pragma solidity ^0.8.18;

import {IOracle} from "../oracle/interfaces/IOracle.sol";

abstract contract OracleConsumer {
    mapping(bytes32 requestId => IOracle oracle) public requests;
    uint256 public requestCount;

    function _sendRequest(
        IOracle oracle,
        bytes4 callbackSelector,
        bytes32 taskId,
        bytes memory data
    ) internal returns (bytes32 requestId) {
        requestId = keccak256(abi.encodePacked(address(this), requestCount++));
        requests[requestId] = oracle;
        oracle.sendRequest(callbackSelector, taskId, requestId, data);
        return requestId;
    }

    function _cancelRequest(bytes32 requestId) internal {
        IOracle oracle = requests[requestId];
        delete requests[requestId];
        oracle.cancelRequest(requestId);
    }

    function _fulfillRequest(bytes32 requestId) internal {
        require(
            address(requests[requestId]) == msg.sender,
            "Unauthorized sender"
        );
        delete requests[requestId];
    }
}
