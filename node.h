#define BUFFLEN 1024


void mergeClock(struct clock* vclock);

void usage(char * cmd);
void initClock( void );
void initGroup(char * groupListFileName);
int initMember(char * rPort, char * rAddress, int groupIndex);
int listeningSocket(char *lPort);
int init(int argc, char **argv);


int logInit( void );
int logSend(struct clock* vclock, struct msg* message, unsigned int receipiant);
int logReceive(struct clock* vclock, struct msg * message, unsigned int sender);

void printMessageType(FILE *f, msgType msg);
void printClock(FILE* f, struct clock* vclock);
