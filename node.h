#define BUFFLEN 1024
#define STATE_INIT 0
#define STATE_NORMAL 1
#define STATE_AYA 2
#define STATE_ELECTION 3
#define STATE_ANSWERED 4


unsigned int myID;
unsigned int coordID;

char * stateStr(int state){
    switch(state){
        case STATE_INIT:
            return "INIT";
        case STATE_NORMAL:
            if (myID == coordID){
                return "COORDINATOR";
            } else {
                return "NORMAL";
            }
        case STATE_AYA:
            return "AYA_WAIT";
        case STATE_ELECTION:
            return "ELECTION";
        case STATE_ANSWERED:
            return "ANSWERED";
        default:
            return "INVALID";
    }
}

int mergeClock(struct clock* vclock);

void usage(char * cmd);
void initClock( void );
void initGroup(char * groupListFileName);
int initMember(char * rPort, char * rAddress, int groupIndex);
int listeningSocket(char* lPort);
int init(int argc, char **argv);


int logInit( char *logFileName );
int logSend(struct clock* vclock, struct msg* message, unsigned int receipiant);
int logReceive(struct clock* vclock, struct msg * message, unsigned int sender);

void printMessageType(FILE *f, msgType msg);
void printClock(FILE* f, struct clock* vclock);
