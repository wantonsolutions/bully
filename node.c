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
int lsocd;		//listening socket discriptor
struct msg myMsg;	//message used for sending to each node
struct clock myClock[MAX_NODES];

/* /Global Node Variables */

void usage(char * cmd) {
	printf("usage: %s  portNum groupFileList logFile timeoutValue averageAYATime failureProbability \n",
			cmd);
}
/*
int nodeInGroup(int nodeId){
	int nodeIndex = 0;
	while(nodeIndex < MAX_NODES){
		if(nodeId == myGroup.members[nodeIndex].nodeId){
			return 1;
		}
		nodeIndex++;
	return 0;
}

int getNodeIndex(int nodeId){
	nodeIndex = 0;
	while(nodeIndex < MAX_NODES){
		if(nodeId == myGroup.members[nodeIndex].nodeId){
			return nodeIndex;
		}
		nodeIndex++;
	return -1;
}

int sendMsg(int nodeId){
	int nodeIndex;
	int sentBytes;
	if(nodeId == port){
		fprintf(stderr,"Error atempting to send message to self\n");
		return -1;
	}
	if(!nodeInGroup(nodeId)){
		fprintf(stderr,"Error recipient node %d not in group\n",nodeId);
		return -2;
	}
	nodeIndex = getNodeIndex(nodeId);
	// TODO check to ensure that the message totally sent
	// TODO roll a random number before sending the message to determine if it was sent
	// TODO network byte order make sure or push that responsability
	if((sentBytes = sendto(
					myGroup.members[nodeIndex].sockId, 
					myMsg,
				       	sizeof (struct msg),
					0,
				       	myGroup.members[nodeIndex].info.ai_addr,
					myGroup.members[nodeIndex].info.ai_addrlen)) == -1) {
		perr;or("talker: sendto");
		return -3;
	}
	printf("message sent to %d \n",nodeId);
	// TODO increment clock
	// TODO log clock
	return 0
}

struct msg* receiveMsg( void ){
	struct sockadder_storage their_addr;
	socketlen_t adder_len;
	int numBytes;
	char buf[sizeof (struct msg) + 1];

	adder_len = sizeof(their_addr);
	if((numbytes = recvfrom(lsocd,buf, sizeof(struct msg), 0,(struct sockaddr *)&their_addr, &addr_len)) == -1){
		perror("recvfrom");
		return NULL;
	}
	printf("lister got package from %s\n", inet_ntop(their_addr.ss_family, get_in_addr((struct sockaddr *)&their_addr),s, sizeof s));
	printf("listener: packet is %d bytes long\n", numbytes);
	buf[numbytes] = '\0';
	printf("listener: packet contains \"%s\"\n",buf);
	}
*/

/*
 * initClock creates a clock for this node, the values are set under the assumption that the group has been set up successfully
 */


/*
 * mergeClock combines a vector clock with the global one for this node
 */
void mergeClock(struct clock* vclock){
	int i;
	for(i=0;i<MAX_NODES;i++){
		if(vclock[i].time > myClock[i].time){
			myClock[i].time = vclock[i].time;
		}
	}

}

/**************************************************************************/
/*			Initalization					  */
/**************************************************************************/
void initClock( void ){
	int i;
	for(i=0;i<MAX_NODES;i++){
		if(i<myGroup.size){
			//if the entry is for this node
			if(myGroup.members[i].nodeId == port){
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
	while (fscanf(fp, "%1023s%",buf) == 1){//read 1 feild into buffer at a time
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
				myGroup.members[fields/2].nodeId = port;
				myGroup.members[fields/2].sockId = -1;// set the sending socket to -1
				includesSelf = 1;
			} else {
				if(initMember((char *)&buf,(char *)&addr,fields/2) < 0){
					exit(0);
				}
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
	myGroup.members[groupIndex].sockId = sockfd;
	myGroup.members[groupIndex].info = *p;
	return;
}

/* 
 * listingSocket creates an unconnected UDP socket discriptor for listening on.
 *
 * param lPort, the lPort number to listin on this node
 *
 *
 * return the socket which can be used for listening
 * return -1 if the address could not be resolved
 */
int listeningSocket(char *lPort){
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
	if (argv[1] == end) {
		printf("Port conversion error\n");
		err++;
	}

	lsocd = listeningSocket(argv[1]);
	if(lsocd < 0){
		fprintf(stderr, "listening socket initalization error on port %s\n", argv[1]);
		err++;
	}

	groupListFileName = argv[2];
	initGroup(groupListFileName);
	initClock();

	logFileName       = argv[3];
	logInit();

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
int logInit( void ){
	FILE *f = fopen(logFileName,"w+");
	if( f == NULL){
		fprintf(stderr,"Error opening log file\n");
		exit(0);
	}
	fprintf(f,"Starting N%lu\n",port);
	printClock(f,(struct clock *)myClock);
	fclose(f);
}

int logSend(struct clock* vclock, struct msg* message, unsigned int receipiant){
	FILE *f = fopen(logFileName,"a");
	if( f == NULL){
		fprintf(stderr,"Error opening log file\n");
		return -1;
	}
	fprintf(f,"Send");
	printMessageType(f,message->msgID);
	fprintf(f,"to N%u E:%u\n",receipiant,message->electionID);
	printClock(f,(struct clock *)myClock);
	fclose(f);
}

int logReceive(struct clock* vclock, struct msg * message, unsigned int sender){
	FILE *f = fopen(logFileName,"a");
	if( f == NULL){
		fprintf(stderr,"Error opening log file\n");
		return -1;
	}
	fprintf(f,"Receive");
	printMessageType(f,message->msgID);
	fprintf(f,"from N%u E:%u\n",sender,message->electionID);
	printClock(f,(struct clock *)myClock);
	fclose(f);
}

void printMessageType(FILE *f, msgType msg){
	switch(msg){
		case ELECT:
			fprintf(f," ELECT ");
			break;
		case ANSWER:
			fprintf(f," ANSWER ");
			break;
		case COORD:
			fprintf(f," COORD ");
			break;
		case AYA:
			fprintf(f," AYA ");
			break;
		case IAA:
			fprintf(f," IAA ");
			break;
		default:
			fprintf(stderr,"ERROR: Unknown Message type\n");
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
		int rn;
		rn = random(); 

		// scale to number between 0 and the 2*AYA time so that 
		// the average value for the timeout is AYA time.

		int sc = rn % (2*AYATime);
		printf("Random number %d is: %d\n", i, sc);
	}

	//testing
	struct msg message;
	message.msgID = COORD;
	message.electionID = 1;
	for(i=0;i<MAX_NODES;i++){
		message.vectorClock[i].nodeId = i + 8888;
		message.vectorClock[i].time = i;
	}

	//logging tests
	myClock[0].time++;
	mergeClock((struct clock *)message.vectorClock);
	logReceive((struct clock *)myClock,&message, 8889);



	return 0;
}


