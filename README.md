# Fault Tolerant Key-Value store.
A solution to fault tolerant distributed Key-Value store assignment for cloud computing concepts part 2 (Coursera)

- Designed and implemented a key-value/NoSQL storage system, which supports CRUD operations (create, read, update, delete).
- Arranged nodes storing the Key-Value pairs in a ring topology and implemented load-balancing using consistent hashing.
- Replicated each key three times to three successive nodes in the ring to achieve fault-tolerance up to two failures.
- Implemented Quorum consistency (at least 2 replicas) for both reads and writes.
- Implemented Stabilization after failures, which recreates 3 replicas.

- To detect the failures of nodes, implemented a SWIM-style membership protocol.
- The membership protocol satisfied completeness and accuracy of failure detection. 
- i.e. All non-faulty nodes detect every node join, failure and leave.
- And When there are no message losses and message delays are small, all failures are detected. 
- When there are message losses, completeness is satisfied and accuracy is high.

The membership protocol is taken from part 1 of the course : https://github.com/ravibitsgoa/membership-protocol
