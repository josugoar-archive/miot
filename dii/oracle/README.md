# oracle

> Open request-response oracle supporting arbitrary data

## How to use

1. Deploy **Oracle.sol** or implement and deploy custom **IOracle.sol** interface.

2. Inherit from **OracleConsumer.sol** and deploy contract defining a callback for oracle.

3. Define **ENDPOINT_URI**, **PRIVATE_KEY**, **ADDRESS**, **ABI** and **TASK_ID** in **.env** file and run **oracle_service** or use provided interface in custom program.

4. Call oracle from oracle consumer to access off-chain data.
