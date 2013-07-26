#include "common.h"

char *homeDir;
char *logFileName;
char *nodeID;
char *hostname;

struct connection_struct connections[1000];

struct node_struct beacon_nodes[100],neighbor_nodes[100];
list<struct message_struct> msgTable;
list<struct message_struct>::iterator it1,it2;
map<int,struct neighbor_struct> neighborMap;
map<int,struct neighbor_struct>::iterator it3,it4,it5;

pthread_cond_t clientConnect_cond[10];
pthread_mutex_t msgTable_mutex,shutDown_mutex,clientConnect_mutex,connection_mutex;
pthread_t tempReadT,tempWriteT,client[20],server,timer,readT[50],writeT[50],userInput;

unsigned int ID;
struct arg_struct argForThread[100];
ofstream logFile;
ifstream initNeighborListFile;

bool reset;
bool iAmABeacon;
bool shutDown;
bool client_killed[20];
bool initNeighborListFileAvailable;
bool tempWriteDie;
bool softRestart;

unsigned short port;
unsigned int location;
int autoShutDown;
int retry;
unsigned int noOfBeacons;
unsigned int TTL;
unsigned int msgLifeTime;
unsigned int noOfConnections;
unsigned int noOfNeighbors;
unsigned int initNeighbors;
unsigned int minNeighbors;
unsigned int joinTimeOut;

char *initNeighborListFileName;
char *statusFileName;
