// SPDX-License-Identifier: MIT
pragma solidity ^0.8.18;

import {IOracle} from "../oracle/interfaces/IOracle.sol";
import {OracleConsumer} from "../OracleConsumer.sol";

contract MockOracleConsumer is OracleConsumer {
    address public owner;
    IOracle public oracle;
    bytes32 public taskId;
    string public data;

    constructor(IOracle oracle_, bytes32 taskId_) {
        require(address(oracle_) != address(0), "Invalid oracle address");
        owner = msg.sender;
        oracle = oracle_;
        taskId = taskId_;
    }

    function setOwner(address owner_) external {
        require(owner == msg.sender, "Unauthorized sender");
        require(owner_ != address(0), "Invalid owner address");
        owner = owner_;
    }

    function setOracle(IOracle oracle_) external {
        require(owner == msg.sender, "Unauthorized sender");
        require(address(oracle_) != address(0), "Invalid oracle address");
        oracle = oracle_;
    }

    function setTaskId(bytes32 taskId_) external {
        require(owner == msg.sender, "Unauthorized sender");
        taskId = taskId_;
    }

    function sendRequest(
        string memory data_
    ) external returns (bytes32 requestId) {
        require(owner == msg.sender, "Unauthorized sender");
        return
            _sendRequest(
                oracle,
                this.fulfillRequest.selector,
                taskId,
                bytes(data_)
            );
    }

    function cancelRequest(bytes32 requestId) external {
        require(owner == msg.sender, "Unauthorized sender");
        _cancelRequest(requestId);
    }

    function fulfillRequest(bytes32 requestId, bytes calldata data_) external {
        _fulfillRequest(requestId);
        data = string(data_);
    }
}
