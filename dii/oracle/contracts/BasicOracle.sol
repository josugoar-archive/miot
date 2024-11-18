// SPDX-License-Identifier: MIT
pragma solidity ^0.8.18;

import {OracleBase} from "./OracleBase.sol";

contract BasicOracle is OracleBase {
    constructor() OracleBase(msg.sender) {}
}
