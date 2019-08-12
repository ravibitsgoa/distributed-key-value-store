/**********************************
 * FILE NAME: MP2Node.cpp
 *
 * DESCRIPTION: MP2Node class definition
 **********************************/
#include "MP2Node.h"
static int debug = 0;
/*Custom log redirector*/
#define mylog(fmt,...)  if(debug) log->LOG(&(memberNode->addr),fmt,__VA_ARGS__);


/* Definitons of the custom message wrapper */
string MyMessage::stripMyHeader(string message){
	/*Strip the custom header from the message and return the rest */
	int pos = message.find('@');
	return message.substr(pos+1);
}
MyMessage::MyMessage(string message):Message(MyMessage::stripMyHeader(message)){
	int  header = stoi(message.substr(0,message.find('@')));
	msgType = static_cast<MyMessageType>(header);
}
MyMessage::MyMessage(MyMessage::MyMessageType mt, string normalMsg):Message(normalMsg),msgType(mt){
}
MyMessage::MyMessage(MyMessage::MyMessageType mt, Message normalMsg):Message(normalMsg),msgType(mt){
}
string MyMessage::toString(){
	return to_string(msgType) + '@' + Message::toString();
}


/**
 * constructor
 */
MP2Node::MP2Node(Member *memberNode, Params *par, EmulNet * emulNet, Log * log, Address * address) {
	this->memberNode = memberNode;
	this->par = par;
	this->emulNet = emulNet;
	this->log = log;
	ht = new HashTable();
	this->memberNode->addr = *address;
	this->inited = false;
}

/**
 * Destructor
 */
MP2Node::~MP2Node() {
	delete ht;
	delete memberNode;
}

/**
 * FUNCTION NAME: updateRing
 *
 * DESCRIPTION: This function does the following:
 * 				1) Gets the current membership list from the Membership Protocol (MP1Node)
 * 				   The membership list is returned as a vector of Nodes. See Node class in Node.h
 * 				2) Constructs the ring based on the membership list
 * 				3) Calls the Stabilization Protocol
 */
void MP2Node::updateRing() {
	/*
	 * Implement this. Parts of it are already implemented
	 */
	vector<Node> curMemList;
	bool change = false;
	/*
	 *  Step 1. Get the current membership list from Membership Protocol / MP1
	 */
	curMemList = getMembershipList();

	/*
	 * Step 2: Construct the ring
	 */
	// Sort the list based on the hashCode
	sort(curMemList.begin(), curMemList.end());
	/* right now create the ring as a copy of the sorted member list */
	ring = curMemList;
	/*Check the status of replicas relative to your position in the ring */
	/*
	 * Step 3: Run the stabilization protocol IF REQUIRED
	 */
	stabilizationProtocol();
}

/**
 * FUNCTION NAME: getMemberhipList
 *
 * DESCRIPTION: This function goes through the membership list from the Membership protocol/MP1 and
 * 				i) generates the hash code for each member
 * 				ii) populates the ring member in MP2Node class
 * 				It returns a vector of Nodes. Each element in the vector contain the following fields:
 * 				a) Address of the node
 * 				b) Hash code obtained by consistent hashing of the Address
 */
vector<Node> MP2Node::getMembershipList() {
	unsigned int i;
	vector<Node> curMemList;
	for ( i = 0 ; i < this->memberNode->memberList.size(); i++ ) {
		Address addressOfThisMember;
		int id = this->memberNode->memberList.at(i).getid();
		short port = this->memberNode->memberList.at(i).getport();
		memcpy(&addressOfThisMember.addr[0], &id, sizeof(int));
		memcpy(&addressOfThisMember.addr[4], &port, sizeof(short));
		curMemList.emplace_back(Node(addressOfThisMember));
	}
	return curMemList;
}

/**
 * FUNCTION NAME: hashFunction
 *
 * DESCRIPTION: This functions hashes the key and returns the position on the ring
 * 				HASH FUNCTION USED FOR CONSISTENT HASHING
 *
 * RETURNS:
 * size_t position on the ring
 */
size_t MP2Node::hashFunction(string key) {
	std::hash<string> hashFunc;
	size_t ret = hashFunc(key);
	return ret%RING_SIZE;
}

/**
 * FUNCTION NAME: clientCreate
 *
 * DESCRIPTION: client side CREATE API
 * 				The function does the following:
 * 				1) Constructs the message
 * 				2) Finds the replicas of this key
 * 				3) Sends a message to the replica
 */
void MP2Node::clientCreate(string key, string value) {
	/*
	 * Implement this
	 */
	g_transID++;
	vector<Node> recipients = findNodes(key);
	assert(recipients.size()==NUM_KEY_REPLICAS);
	Address* sendaddr = &(this->memberNode->addr);
	for (int i=0;i<NUM_KEY_REPLICAS;++i){
		MyMessage createMsg(MyMessage::QUERY,Message(g_transID,this->memberNode->addr,MessageType::CREATE,key,value,static_cast<ReplicaType>(i)));
		unicastMessage(createMsg,recipients[i].nodeAddress);
	}
	/*Store the transaction ID in your list*/
	transaction tr(g_transID,par->getcurrtime(),QUORUM_COUNT,MessageType::CREATE,key,value);
	translog.push_front(tr);
}

/**
 * FUNCTION NAME: clientRead
 *
 * DESCRIPTION: client side READ API
 * 				The function does the following:
 * 				1) Constructs the message
 * 				2) Finds the replicas of this key
 * 				3) Sends a message to the replica
 */
void MP2Node::clientRead(string key){
	/*
	 * Implement this
	 */
	g_transID++;
	MyMessage createMsg(MyMessage::QUERY,Message(g_transID,this->memberNode->addr,MessageType::READ,key));
	/*Store the transaction ID in your list*/
	transaction tr(g_transID,par->getcurrtime(),QUORUM_COUNT,MessageType::READ,key,"");
	translog.push_front(tr);
	vector<Node> recipients = findNodes(key);
	multicastMessage(createMsg,recipients);
}

/**
 * FUNCTION NAME: clientUpdate
 *
 * DESCRIPTION: client side UPDATE API
 * 				The function does the following:
 * 				1) Constructs the message
 * 				2) Finds the replicas of this key
 * 				3) Sends a message to the replica
 */
void MP2Node::clientUpdate(string key, string value){
	/*
	 * Implement this
	 */
	g_transID++;
	vector<Node> recipients = findNodes(key);
	assert(recipients.size()==NUM_KEY_REPLICAS);
	Address* sendaddr = &(this->memberNode->addr);
	for (int i=0;i<NUM_KEY_REPLICAS;++i){
		MyMessage createMsg(MyMessage::QUERY,Message(g_transID,this->memberNode->addr,MessageType::UPDATE,key,value,static_cast<ReplicaType>(i)));
		unicastMessage(createMsg,recipients[i].nodeAddress);
	}
	/*Store the transaction ID in your list*/
	transaction tr(g_transID,par->getcurrtime(),QUORUM_COUNT,MessageType::UPDATE,key,value);
	translog.push_front(tr);
}

/**
 * FUNCTION NAME: clientDelete
 *
 * DESCRIPTION: client side DELETE API
 * 				The function does the following:
 * 				1) Constructs the message
 * 				2) Finds the replicas of this key
 * 				3) Sends a message to the replica
 */
void MP2Node::clientDelete(string key){
	/*
	 * Implement this
	 */
	if (key=="invalidKey")
		mylog("Got and invalid key client %s","key");
	g_transID++;
	MyMessage createMsg(MyMessage::QUERY,Message(g_transID,this->memberNode->addr,MessageType::DELETE,key));
	/*Store the transaction ID in your list*/
	transaction tr(g_transID,par->getcurrtime(),QUORUM_COUNT,MessageType::DELETE,key,"");
	translog.push_front(tr);
	vector<Node> recipients = findNodes(key);
	multicastMessage(createMsg,recipients);
}

/**
 * FUNCTION NAME: createKeyValue
 *
 * DESCRIPTION: Server side CREATE API
 * 			   	The function does the following:
 * 			   	1) Inserts key value into the local hash table
 * 			   	2) Return true or false based on success or failure
 */
bool MP2Node::createKeyValue(string key, string value, ReplicaType replica) {
	/*
	 * Implement this
	 */
	// Insert key, value, replicaType into the hash table
	Entry e(value,par->getcurrtime(),replica);
	keymap.insert(pair<string,Entry>(key,e));
	return true;

}

/**
 * FUNCTION NAME: readKey
 *
 * DESCRIPTION: Server side READ API
 * 			    This function does the following:
 * 			    1) Read key from local hash table
 * 			    2) Return value
 */
string MP2Node::readKey(string key) {
	/*
	 * Implement this
	 */
	// Read key from local hash table and return value
	KeyMap::iterator it;
	if ((it=keymap.find(key))!=keymap.end())
		return it->second.convertToString();
	else return "";
}

/**
 * FUNCTION NAME: updateKeyValue
 *
 * DESCRIPTION: Server side UPDATE API
 * 				This function does the following:
 * 				1) Update the key to the new value in the local hash table
 * 				2) Return true or false based on success or failure
 */
bool MP2Node::updateKeyValue(string key, string value, ReplicaType replica) {
	/*
	 * Implement this
	 */
	// Update key in local hash table and return true or false
	Entry e(value,par->getcurrtime(),replica);
	KeyMap::iterator it;
	if ((it=keymap.find(key))!=keymap.end()){
		keymap.insert(pair<string,Entry>(key,e));
		return true;
	}else return false;
}

/**
 * FUNCTION NAME: deleteKey
 *
 * DESCRIPTION: Server side DELETE API
 * 				This function does the following:
 * 				1) Delete the key from the local hash table
 * 				2) Return true or false based on success or failure
 */
bool MP2Node::deletekey(string key) {
	/*
	 * Implement this
	 */
	// Delete the key from the local hash table
	if(keymap.erase(key)) return true;
	else return false;
}

/**
 * FUNCTION NAME: checkMessages
 *
 * DESCRIPTION: This function is the message handler of this node.
 * 				This function does the following:
 * 				1) Pops messages from the queue
 * 				2) Handles the messages according to message types
 */
void MP2Node::checkMessages() {
	/*
	 * Implement this. Parts of it are already implemented
	 */
	char * data;
	int size;

	/*
	 * Declare your local variables here
	 */

	// dequeue all messages and handle them
	while ( !memberNode->mp2q.empty() ) {
		/*
		 * Pop a message from the queue
		 */
		data = (char *)memberNode->mp2q.front().elt;
		size = memberNode->mp2q.front().size;
		memberNode->mp2q.pop();

		string message(data, data + size);
		mylog("got the message :%s",data);

		/*
		 * Handle the message types here
		 */
		MyMessage msg(message);
		dispatchMessages(msg);

	}

	/*
	 * This function should also ensure all READ and UPDATE operation
	 * get QUORUM replies
	 */
}

/**
 * FUNCTION NAME: findNodes
 *
 * DESCRIPTION: Find the replicas of the given keyfunction
 * 				This function is responsible for finding the replicas of a key
 */
vector<Node> MP2Node::findNodes(string key) {
	size_t pos = hashFunction(key);
	vector<Node> addr_vec;
	if (ring.size() >= 3) {
		// if pos <= min || pos > max, the leader is the min
		if (pos <= ring.at(0).getHashCode() || pos > ring.at(ring.size()-1).getHashCode()) {
			addr_vec.emplace_back(ring.at(0));
			addr_vec.emplace_back(ring.at(1));
			addr_vec.emplace_back(ring.at(2));
		}
		else {
			// go through the ring until pos <= node
			for (int i=1; i<ring.size(); i++){
				Node addr = ring.at(i);
				if (pos <= addr.getHashCode()) {
					addr_vec.emplace_back(addr);
					addr_vec.emplace_back(ring.at((i+1)%ring.size()));
					addr_vec.emplace_back(ring.at((i+2)%ring.size()));
					break;
				}
			}
		}
	}
	return addr_vec;
}

/* Called everytime in the recvLoop to decrement transaction timers */
void MP2Node::updateTransactionLog(){

	list<transaction>::iterator it=translog.begin();
	while(it!=translog.end())
		if((par->getcurrtime()-it->local_ts)>RESPONSE_WAIT_TIME) {
			MessageType mtype = it->trans_type;
			int transid = it->gtransID;
			mylog("Transaction %d timeout",transid);
			switch(mtype){
			case MessageType::CREATE: log->logCreateFail(&memberNode->addr,true,transid,it->key,it->latest_val.second);break;
			case MessageType::UPDATE: log->logUpdateFail(&memberNode->addr,true,transid,it->key,it->latest_val.second);break;
			case MessageType::READ: log->logReadFail(&memberNode->addr,true,transid,it->key);break;
			case MessageType::DELETE: log->logDeleteFail(&memberNode->addr,true,transid,it->key);break;
			}
			translog.erase(it++);
		}else it++;

}

/**
 * FUNCTION NAME: recvLoop
 *
 * DESCRIPTION: Receive messages from EmulNet and push into the queue (mp2q)
 */
bool MP2Node::recvLoop() {
	if ( memberNode->bFailed ) {
		return false;
	}
	else {
		/*Update the timeout counters on the transaction log*/
		updateTransactionLog();
		return emulNet->ENrecv(&(memberNode->addr), this->enqueueWrapper, NULL, 1, &(memberNode->mp2q));
	}
}

/**
 * FUNCTION NAME: enqueueWrapper
 *
 * DESCRIPTION: Enqueue the message from Emulnet into the queue of MP2Node
 */
int MP2Node::enqueueWrapper(void *env, char *buff, int size) {
	Queue q;
	return q.enqueue((queue<q_elt> *)env, (void *)buff, size);
}
/* Handles the message type and sends it to the appropriate handler */
void MP2Node::dispatchMessages(MyMessage message){
	if(message.msgType==MyMessage::QUERY){
		switch(message.type){
			/* server side message types */
		case MessageType::CREATE: processKeyCreate(message);break;
		case MessageType::UPDATE: processKeyUpdate(message);break;
		case MessageType::DELETE: processKeyDelete(message);break;
		case MessageType::READ: processKeyRead(message);break;

			/* client side message types */
			/* reply message to read requests*/
		case MessageType::READREPLY: processReadReply(message);break;
			/* generic REPLY message (sent/recived in response to CREATE , READ , DELETE requests */
		case MessageType::REPLY: processReply(message);break;
		}
	}else if(message.msgType==MyMessage::REPUPDATE)
		processReplicaUpdate(message);
	else{
		/*message type is unkown, do nothign*/
		mylog("Dropping corrupted packet %s",message.toString().c_str());
		return;
	}
}
/*Stabilization process message handlers */
void MP2Node::processReplicate(Node toNode, ReplicaType repType){
	//push out these keys to the node as a replicate message
	KeyMap::iterator it;
	for(it=keymap.begin();it!=keymap.end();++it){
		if(it->second.replica==repType){
			MyMessage keyupdate(MyMessage::REPUPDATE,Message(-1,(memberNode->addr),MessageType::CREATE,it->first,it->second.value,ReplicaType::TERTIARY));
			unicastMessage(keyupdate,toNode.nodeAddress);
		}
	}
}
void MP2Node::processReplicaUpdate(Message message){
	/*Add the key to the map withe desired key type buffer*/
	createKeyValue(message.key,message.value,message.replica);
}
/* server side message handlers */
void MP2Node::processKeyCreate(Message message){
	MyMessage reply(MyMessage::QUERY,Message(message.transID,(this->memberNode->addr),MessageType::REPLY,false));
	if( createKeyValue(message.key,message.value,message.replica)){
		reply.success=true;
		mylog("create server node with replica %d for key %s",message.replica,message.key.c_str());
		log->logCreateSuccess(&memberNode->addr,false,message.transID,message.key,message.value);
	}else{
		reply.success=false;
		log->logCreateFail(&memberNode->addr,false,message.transID,message.key,message.value);
	}
	unicastMessage(reply,message.fromAddr);
}
void MP2Node::processKeyUpdate(Message message){
	MyMessage reply(MyMessage::QUERY,Message(message.transID,(this->memberNode->addr),MessageType::REPLY,false));
	if( updateKeyValue(message.key,message.value,message.replica)){
		reply.success=true;
		mylog("update server node with replica %d for key %s",message.replica,message.key.c_str());
		log->logUpdateSuccess(&memberNode->addr,false,message.transID,message.key,message.value);
	}else{
		reply.success=false;
		log->logUpdateFail(&memberNode->addr,false,message.transID,message.key,message.value);
	}
	unicastMessage(reply,message.fromAddr);
}
void MP2Node::processKeyDelete(Message message){
	MyMessage reply(MyMessage::QUERY,Message(message.transID,(this->memberNode->addr),MessageType::REPLY,false));
	if( deletekey(message.key)){
		reply.success=true;
		mylog("delete server node with replica %d for key %s",message.replica,message.key.c_str());
		log->logDeleteSuccess(&memberNode->addr,false,message.transID,message.key);
	}
	else{
		reply.success=false;
		log->logDeleteFail(&memberNode->addr,false,message.transID,message.key);
	}
	unicastMessage(reply,message.fromAddr);

}
/*The Key read message format does not have a separate flag for success*/
void MP2Node::processKeyRead(Message message){
	string keyval = readKey(message.key);
	if(!keyval.empty()){
		mylog("read server node with replica %d for key %s",message.replica,message.key.c_str());
		log->logReadSuccess(&memberNode->addr,false,message.transID,message.key,keyval);
	}else{
		log->logReadFail(&memberNode->addr,false,message.transID,message.key);
	}
	MyMessage reply(MyMessage::QUERY,Message(message.transID,(this->memberNode->addr),keyval));
	unicastMessage(reply,message.fromAddr);
}

/* client side message handlers */
/* Process the reply recieved in response to a read request*/
void MP2Node::processReadReply(Message message){
	/* over here, match the reply with the transaction id , and keep
	count of the replies received . if quorum is reached then log the reply,
	else just decrement the quorum counter.
	There may be a conflict in the returned values , some may be older.
	Ideally the reply with the latest global timestamp should be returned
	*/
	string value = message.value;
	//If the node did not have the key there's nothing you can do about it
	if(value.empty()) return;
	//split the value to extract the actual value , timestamp and the replica type
	string delim = ":";
	vector<string> tuple;
	int start = 0;
	int pos = 0;
	while((pos=value.find(delim,start))!=string::npos){
		string token = value.substr(start,pos-start);
		tuple.push_back(token);
		start = pos+1;
	}
	tuple.push_back(value.substr(start));
	assert(tuple.size()==3);
	string keyval = tuple[0];
	int timestamp = stoi(tuple[1]);
	ReplicaType repType = static_cast<ReplicaType>(stoi(tuple[2]));
	int transid = message.transID;
	list<transaction>::iterator it;
	for (it = translog.begin(); it!=translog.end();++it)
		if(it->gtransID==transid)
			break;
		else mylog("unmatched transaction transid %d",it->gtransID);

	//now we have for the key, its value and the timestamp
	if(it==translog.end()){
		//The reply has come in too late and the transaction has been dropped
		//from the log. ignore the reply.
		mylog("dropping reply for transid: %d",transid);
	}else if(--(it->quorum_count)==0){
		//quorum replies received
		//LOG success with the latest val;
		mylog("Received reply for op %d for transid: %d,%d replies remaining",it->trans_type,it->gtransID,it->quorum_count);
		log->logReadSuccess(&memberNode->addr,true,message.transID,it->key,it->latest_val.second);
		//delete from translog
		translog.erase(it);
	}else{
		//LOG reply
		mylog("Received reply for op %d for transid: %d,%d replies remaining",it->trans_type,it->gtransID,it->quorum_count);
		if(timestamp>=it->latest_val.first){
			it->latest_val = pair<int,string>(timestamp,keyval);
			mylog("Changing latest val for transid :%d and key: %s",it->gtransID,it->key.c_str());
		}
	}

}
/* reply received in response to create, read and update queries */
void MP2Node::processReply(Message message){
	int transid = message.transID;
	list<transaction>::iterator it;
	for (it = translog.begin(); it!=translog.end();++it)
		if(it->gtransID==transid)
			break;
	if(it==translog.end()) {
		//The reply has come in too late and the transaction has been dropped
		//from the log. ignore the reply.
		mylog("dropping reply for transid: %d",transid);
	}else if(!message.success){
		//no luck!
	}else if(--(it->quorum_count)==0){
		//quorum replies received
		//LOG success with the latest val and for type trans_type;
		mylog("Received reply for op %d for transid: %d,%d replies remaining",it->trans_type,it->gtransID,it->quorum_count);
		switch(it->trans_type){
		case MessageType::CREATE: log->logCreateSuccess(&memberNode->addr,true,message.transID,it->key,it->latest_val.second);break;
		case MessageType::UPDATE: log->logUpdateSuccess(&memberNode->addr,true,message.transID,it->key,it->latest_val.second);break;
		case MessageType::DELETE: log->logDeleteSuccess(&memberNode->addr,true,message.transID,it->key);break;
		}
		//delete from translog
		translog.erase(it);
	}else{
		mylog("Received reply for op %d for transid: %d,%d replies remaining",it->trans_type,it->gtransID,it->quorum_count);
		//LOG reply
		//safety, as the reply must come after the request
	}
}


/* Send a multicast message about a CRUD operation to all nodes */
void MP2Node::multicastMessage(MyMessage message,vector<Node>& recipients){

	Address* sendaddr = &(this->memberNode->addr);
	string strrep = message.toString();
	char * msgstr = (char*)strrep.c_str();
	size_t msglen = strlen(msgstr);
	mylog("Sending the client request : %s of length %d",msgstr,msglen);
	for (size_t i=0;i<recipients.size();++i)
		this->emulNet->ENsend(sendaddr,&(recipients[i].nodeAddress),msgstr,msglen);
}
/* Send a unicast message */
void MP2Node::unicastMessage(MyMessage message,Address& toaddr){
	Address* sendaddr = &(this->memberNode->addr);
	string strrep = message.toString();
	char * msgstr = (char*)strrep.c_str();
	size_t msglen = strlen(msgstr);
	mylog("Sending the client request : %s  of length %d",msgstr,msglen);
	this->emulNet->ENsend(sendaddr,&toaddr,msgstr,msglen);
}
/**
 * FUNCTION NAME: stabilizationProtocol
 *
 * DESCRIPTION: This runs the stabilization protocol in case of Node joins and leaves
 * 				It ensures that there always 3 copies of all keys in the DHT at all times
 * 				The function does the following:
 *				1) Ensures that there are three "CORRECT" replicas of all the keys in spite of failures and joins
 *				Note:- "CORRECT" replicas implies that every key is replicated in its two neighboring nodes in the ring
 */
void MP2Node::stabilizationProtocol() {
	/*
	 * Implement this
	 */
	int i=0;
	for (i=0;i<ring.size();++i){
		if((ring[i].nodeAddress==this->memberNode->addr))
			break;
	}

	/* Initialize the list of neighbours*/
	int p_2 = ((i-2)<0?i-2+ring.size():i-2)%ring.size();
	int p_1 = ((i-1)<0?i-1+ring.size():i-2)%ring.size() ;
	int n_1 = (i+1)%ring.size();
	int n_2 = (i+2)%ring.size();
	if(!inited){
		/* first time invocation, just update the neighbours, you dont want to detect failure when just joining */
		mylog("Created the member tables initially at ring pos %s",ring[i].nodeAddress.getAddress().c_str());
		haveReplicasOf.push_back(ring[p_2]);
		haveReplicasOf.push_back(ring[p_1]);
		hasMyReplicas.push_back(ring[n_1]);
		hasMyReplicas.push_back(ring[n_2]);
		inited = true;
	}else{
		/*Just another invocation of the event update*/
		mylog("Created the neighbours, position values are %s,%s,<%s>,%s,%s",
				ring[p_2].nodeAddress.getAddress().c_str(),
				ring[p_1].nodeAddress.getAddress().c_str(),
				ring[i].nodeAddress.getAddress().c_str(),
				ring[n_1].nodeAddress.getAddress().c_str(),
				ring[n_2].nodeAddress.getAddress().c_str());
		Node n1 = ring[n_1]; //next node in the ring
		Node n2 = ring[n_2]; //next to next node in the ring
		Node n3 = ring[p_1]; //previous node in the ring
		if(!(n1.nodeAddress==hasMyReplicas[0].nodeAddress)){
			//The next node in the ring failed, send primary to tertiary of next to next node n2
			processReplicate(n2,ReplicaType::PRIMARY);
		}else if(!(n2.nodeAddress==hasMyReplicas[1].nodeAddress)){
			//The next to next node in the ring failed, send primary to tertiary of next to next node n2
			processReplicate(n2,ReplicaType::PRIMARY);
		}else if(!(n3.nodeAddress==haveReplicasOf[0].nodeAddress)){
			//The node previous to this node failed, send secondary to tertiary of the next to next node n2
			processReplicate(n2,ReplicaType::SECONDARY);
		}else{
			//not bothered about processing a change
		}
		//update the has-have tables
		haveReplicasOf.clear();
		hasMyReplicas.clear();
		haveReplicasOf.push_back(ring[p_2]);
		haveReplicasOf.push_back(ring[p_1]);
		hasMyReplicas.push_back(ring[n_1]);
		hasMyReplicas.push_back(ring[n_2]);
	}

}