/**********************************
 * FILE NAME: MP2Node.h
 *
 * DESCRIPTION: MP2Node class header file
 **********************************/

#ifndef MP2NODE_H_
#define MP2NODE_H_

/**
 * Header files
 */
#include "stdincludes.h"
#include "EmulNet.h"
#include "Node.h"
#include "HashTable.h"
#include "Log.h"
#include "Params.h"
#include "Message.h"
#include "Queue.h"
#include <list>

#define NUM_KEY_REPLICAS 3
#define QUORUM_COUNT ((NUM_KEY_REPLICAS)/2+1)
#define RESPONSE_WAIT_TIME 20
/**
 * CLASS NAME: MP2Node
 *
 * DESCRIPTION: This class encapsulates all the key-value store functionality
 * 				including:
 * 				1) Ring
 * 				2) Stabilization Protocol
 * 				3) Server side CRUD APIs
 * 				4) Client side CRUD APIs
 */

/* Custom message wrapper needed for replicate messages */
class MyMessage:public Message{
public:
	enum MyMessageType{ REPUPDATE,QUERY };
	MyMessage(string message);
	MyMessage(MyMessageType mType,string normalMsg);
	MyMessage(MyMessageType mType,Message normalMsg);
	string toString();
	static string stripMyHeader(string message);
	MyMessageType msgType;
};

/* Custom class implementation for storing transaction info that will be used in MP2Node*/
struct transaction{
public:
	int gtransID;       //globally unique transaction ID
	int local_ts;      //local TS at node when transaction was initiated
	int quorum_count;   //represents the number of nodes need to achieve quorum
	MessageType trans_type; //type of transaction requested
	string key;         //the key associated with the transaction
	pair<int,string> latest_val; //value of the key received with the latest timestamp.
	transaction(int tid, int lts, int qc, MessageType ttype,string k,string value):
			gtransID(tid),
			local_ts(lts),
			quorum_count(qc),
			trans_type(ttype),
			key(k),
			latest_val(0,value){
	}
};
/* custom map */
typedef std::map<string,Entry> KeyMap;
/* Custom class implementations that will be used in MP2Node*/

class MP2Node {
private:
	// Vector holding the next two neighbors in the ring who have my replicas
	vector<Node> hasMyReplicas;
	// Vector holding the previous two neighbors in the ring whose replicas I have
	vector<Node> haveReplicasOf;
	// Ring
	vector<Node> ring;
	// Hash Table
	HashTable * ht;
	// Member representing this member
	Member *memberNode;
	// Params object
	Params *par;
	// Object of EmulNet
	EmulNet * emulNet;
	// Object of Log
	Log * log;

	//Transaction Log
	list<transaction> translog;
	//Custom KeyMap
	KeyMap keymap;

	//first time init
	bool inited ;

	/* server side message handlers */
	void processKeyCreate(Message message);
	void processKeyUpdate(Message message);
	void processKeyDelete(Message message);
	void processKeyRead(Message message);

	/* client side message handlers */
	void processReadReply(Message message);
	void processReply(Message message);

	/* Util functions for sending messages */
	void unicastMessage(MyMessage message,Address& toaddr);
	void multicastMessage(MyMessage message,vector<Node>& recp);

	/* for checking for transaction timeouts*/
	void updateTransactionLog();

	/*event handlers for stabilization protocol*/
	void processReplicate(Node toNode, ReplicaType rType);
	void processReplicaUpdate(Message msg);

public:
	MP2Node(Member *memberNode, Params *par, EmulNet *emulNet, Log *log, Address *addressOfMember);
	Member * getMemberNode() {
		return this->memberNode;
	}

	// ring functionalities
	void updateRing();
	vector<Node> getMembershipList();
	size_t hashFunction(string key);
	void findNeighbors();

	// client side CRUD APIs
	void clientCreate(string key, string value);
	void clientRead(string key);
	void clientUpdate(string key, string value);
	void clientDelete(string key);

	// receive messages from Emulnet
	bool recvLoop();
	static int enqueueWrapper(void *env, char *buff, int size);

	// handle messages from receiving queue
	void checkMessages();

	// coordinator dispatches messages to corresponding nodes
	void dispatchMessages(MyMessage message);

	// find the addresses of nodes that are responsible for a key
	vector<Node> findNodes(string key);

	// server
	bool createKeyValue(string key, string value, ReplicaType replica);
	string readKey(string key);
	bool updateKeyValue(string key, string value, ReplicaType replica);
	bool deletekey(string key);

	// stabilization protocol - handle multiple failures
	void stabilizationProtocol();

	~MP2Node();
};

#endif /* MP2NODE_H_ */