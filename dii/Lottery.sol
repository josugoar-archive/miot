// SPDX-License-Identifier: GPL-3.0

pragma solidity ^0.8.28;

contract Lottery {

    uint public immutable fee;

    address public immutable manager;

    address[] public players;

    address public winner;

    event Played(address indexed player, uint deposit);

    event Won(address indexed winner, uint deposit);

    constructor(uint _fee) {
        fee = _fee;
        manager = msg.sender;
    }

    function play() external payable {
        require(msg.sender != manager, "Manager cannot play the lottery.");
        require(winner == address(0), "Lottery has been resolved.");
        require(msg.value == fee, "Payment must match fee.");
        players.push(msg.sender);
        uint deposit = address(this).balance;
        emit Played(msg.sender, deposit);
    }

    function resolve() external {
        require(msg.sender == manager, "Only manager can resolve the lottery.");
        require(winner == address(0), "Lottery has been resolved.");
        require(players.length > 0, "Lottery has no players.");
        winner = players[block.prevrandao % players.length];
        uint deposit = address(this).balance;
        emit Won(winner, deposit);
        payable(winner).transfer(deposit);
    }

}

