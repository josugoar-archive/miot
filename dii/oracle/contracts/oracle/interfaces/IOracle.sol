// SPDX-License-Identifier: MIT
pragma solidity ^0.8.18;

interface IOracle {
    event SentRequest(
        bytes32 indexed taskId,
        bytes32 indexed requestId,
        bytes data
    );

    event CancelledRequest(bytes32 indexed requestId);

    function sendRequest(
        bytes4 callbackSelector,
        bytes32 taskId,
        bytes32 requestId,
        bytes calldata data
    ) external;

    function cancelRequest(bytes32 requestId) external;

    function fulfillRequest(
        bytes32 requestId,
        bytes calldata data
    ) external returns (bool success);
}
