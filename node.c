#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/udp.h>
//include <arpa/inet.h>
#include <netdb.h>

#include "msg.h"
#include "group.h"
#include "node.h"

/* Global Node variables */
unsigned long  port;
char *         groupListFileName;
char *         logFileName;
unsigned long  timeoutValue;
unsigned long  AYATime;
unsigned long  sendFailureProbability;

struct group   myGroup;
int soc;		//listening socket discriptor
struct msg myMsg;	//message used for sending to each node
struct clock myClock[MAX_NODES];
unsigned int *ourTime;

struct timeval socketTimeout;
struct timeval *timeoutPtr;
FILE *logFile;
unsigned int myID;
unsigned int coordID;
unsigned int state = STATE_INIT;
unsigned int myElectionID = 0;
/* /Global Node Variables */

void usage(char * cmd) {
	printf("usage: %s  portNum groupFileList logFile timeoutValue averageAYATime failureProbability \n",
			cmd);
}

/*
 * initClock creates a clock for this node, the values are set under the assumption that the group has been set up successfully
 */

int clockMergeError(struct clock* vclock){
	int i, j;
	//check for remote node having a higher value then the local clock
	for(j=0;j<MAX_NODES;j++){
		if(vclock[j].nodeId == port){
			if(*ourTime < vclock[j].time){
				fprintf(stderr,"Error: remote node has clock value greater than local time\n");
				return -1;
			}
		}
	}
	//check for existence of group members
	for(i=0;i<MAX_NODES;i++){
		int found = 0;
		for(j=0;j<MAX_NODES;j++){
			if(myClock[i].nodeId == vclock[j].nodeId){
				found = 1;
                break;
			}
		}
		if(!found){
			fprintf(stderr,"Error: remote node has inconsistant group list, group member N%u missing\n",vclock[i].nodeId);
			return -2;
		}
	}
	return 1;
}

/*
 * mergeClock combines a vector clock with the global one for this node
 */
int mergeClock(struct clock* vclock){
	if(clockMergeError(vclock) < 0){
		return -1;
	}
	int i, j;
	for(i=0;i<MAX_NODES;i++){
		for(j=0;j<MAX_NODES;j++){
			if((myClock[i].nodeId == vclock[j].nodeId) && (vclock[j].time > myClock[i].time)){
				myClock[i].time = vclock[j].time;
				continue;
			}
		}
	}
    return 0;
}

void incrementClock(){
	int i;
	for(i=0;i<MAX_NODES;i++){
		if(myClock[i].nodeId == port){
			myClock[i].time++;
			return;
		}
	}
}

/**************************************************************************/
/*			 Group						  */
/**************************************************************************/


/**
 *  Returns the index of the node in the group list 
 *  or -1 if it is not in the list.
 *
 */
int getGroupIndex(unsigned short nodeId){
    int i;
    for(i = 0; i < MAX_NODES; i++){
        if (myGroup.members[i].nodeId == nodeId){
            return i;
        }
    }
    return -1;
}

/* returns the addrInfo of a group member 
 * if the group member is not in the group returns NULL
 */
struct sockaddr_storage * getGroupAddr(unsigned short nodeId){
    int index;
    if(index = getGroupIndex(nodeId) != -1){
        return &myGroup.members[index].nodeAddr;
    }

	fprintf(stderr,"Error, node N%hu not in group\n",nodeId);
	return NULL;
}


/**************************************************************************/
/*			 /Group						  */
/**************************************************************************/

/**************************************************************************/
/*			Initalization					  */
/**************************************************************************/
void initClock( void ){
	int i;
	for(i=0;i<MAX_NODES;i++){
		if(i<myGroup.size){
			//if the entry is for this node
			if(myGroup.members[i].nodeId == port){
				ourTime = &myClock[i].time;
				myClock[i].nodeId = port;
				myClock[i].time = 1;
			} else {
			//if the entry is for another valid group member
				myClock[i].nodeId = myGroup.members[i].nodeId;
				myClock[i].time = 0;
			}
		} else {
		//if the entry is for a non existant node
			myClock[i].nodeId = 0;
			myClock[i].time = 0;
		}
	}
}

/*
 * initgroup creates a group based on an input file. If the input file name is '-' the file is read from standard input.
 * 
 * Preconditions : the global port value has allready been set for this node, it is used to ensure the node itself is within the group
 *
 * the file is to be of the form
 *
 * address port
 * address port
 *
 * with EOF indicating the end of the file
 *
 * param the name of the file containing the group list
 * 
 * return a populated group with a set of members
 */
void initGroup(char * groupListFileName){
	FILE *fp;
	char buf[BUFFLEN];
	if(groupListFileName[0] == '-' && strlen(groupListFileName) == 1){
		fp = stdin;
	} else {
		fp = fopen(groupListFileName, "r");
		//Cannot open file
		if (fp == NULL){
			fprintf(stderr, "Error, cannot open group file %s\n", groupListFileName);
			exit(0);
		}
	}
	int fields = 0;
	char addr[BUFFLEN];
	int includesSelf = 0;
	//alternate feilds and set up node id's and sockets
	while (fscanf(fp, "%1023s",buf) == 1){//read 1 feild into buffer at a time
		if((fields /2) >= MAX_NODES){
			fprintf(stderr, "Error group nodes exceed maximum of %d\n", MAX_NODES);
		}
		if(!(fields %2)){ //Address field
			strcpy(addr,buf);
		} else {	//port field
			//the case where the node finds itself in the group list
			if(port == atoi(buf)){
				//set the self referencing member to have ID value and nothing else
				printf("Adding Self To group\n");
				includesSelf = 1;
			}
			//create yourself like any other group member
			if(initMember((char *)&buf,(char *)&addr,fields/2) < 0){
				exit(0);
			}
			myGroup.size++;
		}
		fields++;
	}

	if(!includesSelf){
		fprintf(stderr, "Error the node id of this node was not in the group list\n");
		exit(0);
	}
	return;
}

  
/*
 * Talking socket creates a UDP socket discriptor for talking on. and returns that socket discriptor.
 * 
 * param rPort, the port number of the remote listening node
 * param rAddresss, the address of the listening node
 * param groupIndex, is the index of the groupmember that can be sent to with this socket.
 *
 * return on success the socket which can be used for sending
 * return -1 if address info could not be found
 * return -2 if binding failed
 * return -3 if port is non integer value
 */
int initMember(char * rPort, char * rAddress, int groupIndex){
	int sockfd;
	struct addrinfo hints, *servinfo, *p;
	int rv;
	int numbytes;
	int nodeId;

	printf("Address: %s\nPort:%s\n",rAddress,rPort);
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_DGRAM;

	if((rv = getaddrinfo(rAddress,rPort, &hints, &servinfo)) != 0){
		fprintf(stderr, "getaddrinfo :%s\n", gai_strerror(rv));
		return -1;
	}

	//loop through all the results and make a socket
	for(p = servinfo; p!= NULL; p = p->ai_next){
		if((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1){
			perror("talker: socket");
			continue;
		}
		break;
	}

	if( p == NULL){
		fprintf(stderr, "faild to bind socket for node %s\n",rPort);
		return -2;
	}
	//check for valid port value	
	if((nodeId = atoi(rPort)) < 0){
		fprintf(stderr, "Error port value :%s is a non integer value\n",rPort);
		return -3;
	}

	//set member variables
	myGroup.members[groupIndex].nodeId = atoi(rPort);
	
    memcpy(&(myGroup.members[groupIndex].nodeAddr), &(p->ai_addr), sizeof(p->ai_addr));
	return 0;
}

/* 
 * listeningSocket creates an unconnected UDP socket discriptor for listening on.
 *
 * param lPort, the lPort number to listin on this node
 *
 *
 * return the socket which can be used for listening
 * return -1 if the address could not be resolved
 */
int listeningSocket(char* lPort){
	int sockfd;
	struct addrinfo hints, *servinfo, *p;
	int rv;
	int numbytes;

	memset(&hints, 0 ,sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_flags = AI_PASSIVE;
	
	printf("the lPort in the method is %s\n",lPort);

	if (( rv = getaddrinfo(NULL, lPort, &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return -1;
	}

	//loop through results and find a valid one
	for(p = servinfo; p!=NULL; p = p->ai_next){
		if((sockfd = socket(p->ai_family, p->ai_socktype,p->ai_protocol)) == -1){
			perror("listener: socket");
			continue;
		}

		if(bind(sockfd, p->ai_addr, p->ai_addrlen) == -1){
			close(sockfd);
			perror("listener :bind");
			continue;
		}
		printf("listener bind success");
		break;
	}

	if(p == NULL){
		fprintf(stderr, "listener: faild to bind socket\n");
		return -2;
	}

	freeaddrinfo(servinfo);
    return sockfd;
}
/*
 * Init sets the values for all of the nodes global variables by parsing the input parameters
 *
 * return an error number assosciated with how many error occured while parsing the input and setting up the global variables
 */
int init(int argc, char **argv) {
	
	if (argc < 7) {
		usage(argv[0]);
		return -1;
	}


	char * end;
	int err = 0;

	port = strtoul(argv[1], &end, 10);
    myID = port;
	if (argv[1] == end) {
		printf("Port conversion error\n");
		err++;
	}

	soc = listeningSocket(argv[1]);
	if(soc < 0){
		fprintf(stderr, "listening socket initalization error on port %s\n", argv[1]);
		err++;
	}

	groupListFileName = argv[2];
	initGroup(groupListFileName);
	initClock();

	logFileName       = argv[3];
	logInit(logFileName);

	timeoutValue      = strtoul(argv[4], &end, 10);
	if (argv[4] == end) {
		printf("Timeout value conversion error\n");
		err++;
	}

	AYATime  = strtoul(argv[5], &end, 10);
	if (argv[5] == end) {
		printf("AYATime conversion error\n");
	err++;
	}

	sendFailureProbability  = strtoul(argv[6], &end, 10);
	if (argv[5] == end) {
		printf("sendFailureProbability conversion error\n");
		err++;
	}

	//Set up parsing for optional extra input parameters

	return err;
}
/**************************************************************************/
/*			/Initalization					  */
/**************************************************************************/

/********************************************************************/
/*			LOGGING					   */
/********************************************************************/


char * msgTypeStr(msgType msg){
    switch(msg){
        case ELECT:
            return "ELECT";
        case ANSWER:
            return "ANSWER";
        case COORD:
            return "COORD";
        case AYA:
            return "AYA";
        case IAA:
            return "IAA";
        case TIMEOUT:
            // If state is normal, the timeout is definitely AYATime 
            // since we don't timeout on anything else.
            if (state == STATE_NORMAL) {
                return "AYATIME";
            } else {
                return "TIMEOUT";
            }
        default:
            return "INVALID";
    }
}

int logInit( char *logFileName ){
	if(logFileName[0] == '-' && strlen(logFileName) == 1){
        logFile = stdout;
        printf("Log file is stdout.");
    } else {
        logFile = fopen(logFileName,"w+");
    }
    if( logFile == NULL){
		fprintf(stderr,"Error opening log file\n");
		exit(0);
	}
	fprintf(logFile,"Starting N%lu\n",port);
	printClock(logFile,(struct clock *)myClock);
}

int logSend(struct clock* vclock, struct msg* message, unsigned int receipiant){
	fprintf(logFile,"Send %s ", msgTypeStr(message->msgID));
	fprintf(logFile,"to N%u E:%u\n",receipiant,message->electionID);
	printClock(logFile,(struct clock *)myClock);
    fflush(logFile);
}

int logReceive(struct clock* vclock, struct msg * message, unsigned int sender){
	fprintf(logFile,"Receive %s ", msgTypeStr(message->msgID));
	fprintf(logFile,"from N%u E:%u\n",sender,message->electionID);
	printClock(logFile,(struct clock *)myClock);
    fflush(logFile);
}

/*
 * log Timeout prints a log based on the state of the system when the timeout occured. It takes not parameters as the state is a global variable
 */
int logTimeout( void ){
	switch(state){
		case STATE_INIT:
			fprintf(logFile,"INIT TIMEOUT -> Send ELECTs\n");
			break;
		case STATE_NORMAL:
			fprintf(logFile,"TIMEOUT on AYA -> Send to Coord%d\n",coordID);
			break;
		case STATE_AYA:
			fprintf(logFile,"TIMEOUT on IAA -> Calling Election\n");
			break;
		case STATE_ELECTION:
			fprintf(logFile,"TIMEOUT on ANSWER -> I am the new coordinator\n");
			break;
		case STATE_ANSWERED:
			fprintf(logFile,"TIMEOUT on COORD -> calling a new election\n");
			break;
	}
	printClock(logFile,(struct clock *)myClock);
	fflush(logFile);
}


void printMessage(struct msg * message){
	fprintf(stdout,"Msg TYPE:\t%d\nMsg ElId:\t%d\n",message->msgID,message->electionID);
	fprintf(stdout,"----CLOCK----\n");
	int i;
	for(i=0;i<MAX_NODES;i++){
		fprintf(stdout,"id: %u\ttime%u\t\n",message->vectorClock[i].nodeId,message->vectorClock[i].time);
	}
}


void printClock(FILE* f, struct clock* vclock){
	fprintf(f,"N%lu {",port);
	int i;
	int atLeastOne = 0;
	for(i=0;i<MAX_NODES;i++){
		if(vclock[i].time > 0){
			if(atLeastOne){
				fprintf(f,", ");
			}
			fprintf(f,"\"N%d\" : %d",vclock[i].nodeId,vclock[i].time);
			atLeastOne++;
		}
	}
	fprintf(f,"}\n");
}
/********************************************************************/
/*			/LOGGING		     		    */
/********************************************************************/


/********************************************************************/
/*			HELPERS     	     		    */
/********************************************************************/

/**
 * Copies and converts a message from host order to network order.
 *
 * We know the size of struct msg, so we don't need to pass 
 * it as a parameter
 */
void htonMsg(struct msg *hostMsg, struct msg *networkMsg){
    networkMsg->msgID = htonl(hostMsg->msgID);
    networkMsg->electionID = htonl(hostMsg->electionID);
    int i;
    for (i = 0; i < MAX_NODES; i++){
        networkMsg->vectorClock[i].nodeId = htonl(hostMsg->vectorClock[i].nodeId);
        networkMsg->vectorClock[i].time = htonl(hostMsg->vectorClock[i].time);
    }
}

/**
 * Copies and converts a message from network order to host order.
 *
 * We know the size of struct msg, so we don't need to pass 
 * it as a parameter.
 */
void ntohMsg(struct msg *networkMsg, struct msg *hostMsg){
    hostMsg->msgID = ntohl(networkMsg->msgID);
    hostMsg->electionID = ntohl(networkMsg->electionID);
    int i;
    for (i = 0; i < MAX_NODES; i++){
        hostMsg->vectorClock[i].nodeId = ntohl(networkMsg->vectorClock[i].nodeId);
        hostMsg->vectorClock[i].time = ntohl(networkMsg->vectorClock[i].time);
    }
}

/**
 * Generates the next AYATime interval.
 *
 */
int generateAYA(){
    int rn;
    rn = random(); 

    // scale to number between 0 and the 2*AYA time so that 
    // the average value for the timeout is AYA time.

    return rn % (2*AYATime);
}


int sendFailed(void){
	int rn;
	rn = random() % 100;
	if( rn > sendFailureProbability){
		return 0;
	}
	return 1;
}

/**
 * Extracts the port number from the sockaddr and converts it to host-order.
 */
in_port_t get_in_port(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return ntohs(((struct sockaddr_in*)sa)->sin_port);
    }
    return ntohs(((struct sockaddr_in6*)sa)->sin6_port);
}

/**
 *  Copies the current vector clock into the message.
 *
 */
void copyClock(struct msg* buf){
    int i;
    for(i = 0; i < MAX_NODES; i++){
        buf->vectorClock[i].nodeId = myClock[i].nodeId;
        buf->vectorClock[i].time = myClock[i].time;
    }
}

/**
 * Creates a message with the current vector clock 
 * using the messageType and election ID.
 *
 */
void constructMessage(msgType messageType, unsigned int electionID, struct msg* buf){
    buf->msgID = messageType;
    buf->electionID = electionID;
    int i;
    copyClock(buf);
}

/**
 *  
 */
void updateTimeout(long newTimeout){
    if (newTimeout == -1){
        //Block indefinitely.
        timeoutPtr = NULL;
    } else if (newTimeout == 0) {
        //Do nothing. Timeout gets updated on Linux.
    } else {
        timeoutPtr = &socketTimeout;
        socketTimeout.tv_sec = newTimeout;
        socketTimeout.tv_usec = 0;
    }

}
/********************************************************************/
/*			/HELPERS		     		    */
/********************************************************************/


/********************************************************************/
/*			Send Recive Wrappers 			    */
/********************************************************************/
ssize_t recvMessage(struct msg * buf, struct sockaddr *from, socklen_t *fromlen){
	int ret;
	struct msg netMsg;
	ret = recvfrom(soc, (void *) &netMsg, sizeof (struct msg), 0, from, fromlen);
	ntohMsg(&netMsg,buf);
	if (mergeClock(buf->vectorClock) == -1){
        return -1;
    }
    incrementClock();
	logReceive(myClock,buf,get_in_port(from));
	return ret;
}

size_t sendMessage(msgType messageType, unsigned int electionID, unsigned short nodeId){
	struct msg buf;
	struct sockaddr_storage* nodeAddr;
	//TODO catch null exception
        nodeAddr = getGroupAddr(nodeId);
	incrementClock();
	constructMessage(messageType, electionID, &buf);
	logSend(myClock,&buf,nodeId);
	if(sendFailed()){
		return sizeof (struct msg);
	} else {	
		struct msg networkMsg;
		htonMsg(&buf, &networkMsg);
		return sendto(soc, (void *)&networkMsg, sizeof(networkMsg), 0, (struct sockaddr *) nodeAddr, sizeof(nodeAddr));
	}
}

/********************************************************************/
/*			/Send Recive Wrappers 			    */
/********************************************************************/


/********************************************************************/
/*			MESSAGE HANDLER HELPERS		     		    */
/********************************************************************/

/**
 * Responds to the message with an ANSWER
 */
void sendANSWER(struct msg *hostBuf, unsigned int nodeID){
    // Use current election ID.
    sendMessage(ANSWER, hostBuf->electionID, nodeID);
}

/**
 * Sends an ELECT message to all nodes above you.
 * We forward the ID if we receive an ELECT, and increment it if we didn't.
 * This is done in the message handler.
 */
void sendELECTs(unsigned int electionID){
    int i;
    for(i = 0; i < MAX_NODES; i++){
        unsigned int memberID = myGroup.members[i].nodeId;
        if(memberID > myID) {
            sendMessage(ELECT, electionID, memberID);
        }
    }
}

/**
 * Send AYA to the coordinator,
 */
void sendAYA(){
    sendMessage(AYA, myID, coordID);
}

/**
 * Send an IAA back to the sender.
 * We can extract the nodeID from the message
 */
void sendIAA(struct msg *message, unsigned int nodeID){
    sendMessage(IAA, message->electionID, nodeID);
}


/**
 * Send COORD to all nodes below you and set CoordID to myID.
 */
void sendCOORDs(){
    coordID = myID;
    int i;
    for(i = 0; i < MAX_NODES; i++){
        unsigned int memberID = myGroup.members[i].nodeId;
        if(memberID < myID && memberID != 0) {
            sendMessage(COORD, myElectionID, memberID);
        }
    }
}

/********************************************************************/
/*			/MESSAGE HANDLER HELPERS		     		    */
/********************************************************************/

/**
 * Handles messages arriving and returns the timeout for the next read.
 */
long handleMsg(msgType messageType, struct msg *message, struct sockaddr *src_addr){
    in_port_t srcNodeID = get_in_port(src_addr);
    if (messageType != TIMEOUT){
        printf("State: %s\nHandling message %s from %hu\n", stateStr(state), msgTypeStr(messageType), srcNodeID);
    } else {
        printf("State: %s \nEvent: %s\n", stateStr(state), msgTypeStr(messageType));
    }
    /** New timeout is 0. If it is -1, we set timeout to -1.
    *  If it is 0, we use our previous timeout.
    *  If it is anything else, we use that value.
    */
    long newTimeout = 0;
    switch(messageType){
        case ELECT:
            /*
             * When we get an ELECT message, in every state except for elect 
             * and answered, we start a new election. We also respond with an answer if we need to.
             */
            switch(state){
                //Respond with Answer. Do not start a new election and do not switch states.
                case STATE_ANSWERED:
                case STATE_ELECTION:
                    sendANSWER(message, srcNodeID);
                    // We keep our timeout since we're already waiting for a response.
                    newTimeout = 0;
                    break;
                case STATE_INIT:
                case STATE_NORMAL:
                case STATE_AYA:
                    // Respond with ANSWER.
                    sendANSWER(message, srcNodeID);
                    // Start Election for INIT, NORMAL and AYA.
                    sendELECTs(message->electionID);
                    state = STATE_ELECTION;
                    // Waiting for an ELECT.
                    newTimeout = timeoutValue;
                    break;
                default:
                    break;
            }
            break;
        case ANSWER:
            /**
             * ANSWER is not valid for anything that isn't state ELECTION.
             */
            switch (state) {
                case STATE_ELECTION:
                    // Transition to state_ANSWERED 
                    newTimeout = (MAX_NODES + 1) * timeoutValue;
                    state = STATE_ANSWERED;
                    break;
                default:
                    break;
            }
            break;
        case COORD:
            /**
             * COORD cancels all previous activities and sets a new coordinator.
             */
            switch (state) {
                case STATE_INIT:
                case STATE_NORMAL:
                case STATE_AYA:
                case STATE_ANSWERED:
                case STATE_ELECTION:
                default:
                    myElectionID = myElectionID > message->electionID ? myElectionID : message->electionID;
                    coordID = srcNodeID;
                    printf("Coordinator is %hu\n", srcNodeID);
                    state = STATE_NORMAL;
                    newTimeout = generateAYA();
                    break;
            }
            break;
        case AYA:
            /**
             * Only respond to AYA if you are the current coordinator and are in state NORMAL or ELECTION.
             * At state ANSWERED, there is a node above you.
             */
            switch (state) {
                case STATE_NORMAL:
                case STATE_ELECTION:
                    // Node was coordinator before the election started.
                    if (coordID == myID){
                        sendIAA(message, srcNodeID);
                        // Keep same timeout.
                        newTimeout = 0;
                    }
                    break;
                default:
                    break;
            }
            break;
        case IAA:
            /**
             * IAA is only really valid if you are in state AYA and the election id is the same.
             */
            switch (state) {
                case STATE_AYA:
                    newTimeout = generateAYA();
                    state = STATE_NORMAL;
                    break;
                default:
                    break;
            }
            break;
        case TIMEOUT:
            switch (state) {
                /**
                 * I won, and I'm the coordinator!
                 * Send out COORDs.
                 */
                case STATE_ELECTION:
                    coordID = myID;
                    sendCOORDs();
                    newTimeout = -1;
                    state = STATE_NORMAL;
                    break;
                /**
                 * I timed out, but since I'm normal, it's really an AYATime.
                 */
                case STATE_NORMAL:
                    if (myID != coordID){
                        sendAYA();
                        newTimeout = timeoutValue;
                        state = STATE_AYA;
                    }
                    break;
                /**
                 *  Expecting a message, but it didn't show up. Start an election.
                 */
                case STATE_INIT:
                case STATE_AYA:
                case STATE_ANSWERED:
                    myElectionID++;
                    sendELECTs(myElectionID);
                    state = STATE_ELECTION;
                    newTimeout = timeoutValue;
                    break;
                default:
                    break;
            }
            break;
        default:
            break;
    }
    return newTimeout;
}

/**
 * The main loop where we wait for a message and call a msg handler.
 *
 */
int mainLoop(int fd){
    int retval;
    msgType messageType;
    fd_set rfds;

    // Set socket timeout.
    long newTimeout = timeoutValue;

    // msg in host format.
    while(1){
        printf("LOOP: State: %s\n", stateStr(state));

        struct sockaddr_storage src_addr;
        socklen_t addrlen = sizeof(struct sockaddr_storage);

        if(!FD_ISSET(fd,&rfds)){
            FD_SET(fd, &rfds);
        }
        // Set up the timeout structure.
        updateTimeout(newTimeout);
        retval = select(fd + 1, &rfds, NULL, NULL, timeoutPtr);
        struct msg hostBuf;
        if(retval == 0) {
            // We timed out for various reasons.
            // This covers normal timeouts and AYATime.
            messageType = TIMEOUT;
            logTimeout();
            // Removed from set on timeout, so add it back.
        } else if (retval == -1) {
            perror("select()");
            continue;
        } else {
            // Receive message and fill src_addr struct.
            retval = recvMessage(&hostBuf, (struct sockaddr *) &src_addr, &addrlen);
            if (retval == -1){
                printf("Invalid message received.\n");
            }
            // Set msg_type.
            messageType = hostBuf.msgID;
        }
        // We know the message length, so we don't need to process it.
        newTimeout = handleMsg(messageType, &hostBuf, (struct sockaddr *) &src_addr);
        // Handle message for current state and transition if needed.
    }
}




int main(int argc, char ** argv) {

	int err = init(argc, argv);

	printf("Port number:              %lu\n", port);
	printf("Group list file name:     %s\n", groupListFileName);
	printf("Log file name:            %s\n", logFileName);
	printf("Timeout value:            %lu\n", timeoutValue);  
	printf("AYATime:                  %lu\n", AYATime);
	printf("Send failure probability: %lu\n", sendFailureProbability);
	printf("Starting up Node %lu\n", port);

	if (err) {
		printf("%d conversion error%sencountered, program exiting.\n",
				err, err>1? "s were ": " was ");
		return -1;
	}

	// If you want to produce a repeatable sequence of "random" numbers
	// replace the call time() with an integer.
	srandom(time());

	int i;
	for (i = 0; i < 10; i++) {
		int sc = generateAYA();
		printf("Random number %d is: %d\n", i, sc);
	}
	/*
	//testing
	struct msg message;
	message.msgID = COORD;
	message.electionID = 1;
	for(i=0;i<MAX_NODES;i++){
		message.vectorClock[i].nodeId = i + 8888;
		message.vectorClock[i].time = i + 1;
	}

	//logging tests
	
	incrementClock();
	mergeClock((struct clock *)message.vectorClock);
	logReceive((struct clock *)myClock,&message, 8889);
	*/
    mainLoop(soc);
	return 0;
}


