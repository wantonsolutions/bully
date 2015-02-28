/*
 * group.h represents the group of nodes within the system
 *
 * Written by Stewart Grant Feb 15 2015
 */
#ifndef GROUP
#define GROUP
#include "msg.h"


struct member{
	unsigned int nodeId;
	struct sockaddr nodeAddr;
};

struct group {
	unsigned int size;
	struct member members[MAX_NODES];
};
#endif
