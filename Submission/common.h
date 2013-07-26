#include<stdio.h>
#include<pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include<time.h>
#include<netdb.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <iomanip>
#include <list>
#include <map>
#ifndef min
	#define min(A,B) (((A)>(B)) ? (B) : (A))
#endif
#include <sys/types.h>
#include "openssl/sha.h"

#define JNRQ 0xFC
#define JNRS 0xFB
#define HLLO 0xFA
#define KPAV 0xF8
#define NTFY 0xF7
#define CKRQ 0xF6
#define CKRS 0xF5
#define SHRQ 0xEC
#define SHRS 0xEB
#define GTRQ 0xDC
#define GTRS 0xDB
#define STOR 0xCC
#define DELT 0xBC
#define STRQ 0xAC
#define STRS 0xAB
#define STRQN 0x01
#define STRQF 0x02

using namespace std;

struct node_struct{
	int port;
	char hostname[50];
};

struct arg_struct{
	int ID;
};

struct neighbor_struct{
	int noOfNeighbors;
	int neighborsList[20];
};

struct commonHeader_struct{
	unsigned char msgType;
	//unsigned unsigned char UOID[20];
	unsigned char UOID[20];
	char TTL;
	char reserverd;
	int dataLength;
	
	void print(){
		printf("\tMsgType\t\t: 0X%x\n\tUOID\t\t: %s\n\tTTL\t\t: %d\n\tDataLength\t: %d \n",toupper(msgType),UOID,TTL,dataLength);
	}
};

struct hello_struct{
	unsigned short port;
	char hostname[100];
	void print(){
		printf("\tHostname : %s\t Port : %d \n",hostname,port);
	}
};

struct joinRequest_struct{
	unsigned short port;
	char hostname[20];
	unsigned int location;
	
	void print(){
		printf("\tHostname\t\t: %s\n\tPort\t\t: %d\n\tLocation\t\t: %d \n",hostname,port,location);
	}
};

struct joinResponse_struct{
	//unsigned unsigned char UOID[20];
	unsigned char UOID[20];
	unsigned int distance;
	unsigned short port;
	char hostname[20];
	
	void print(){
		printf("\tRequest UOID\t\t: %s\n\tHostname\t\t: %s\n\tPort\t\t: %d\n\tDistance\t\t: %d \n",UOID,hostname,port,distance);
	}
};

struct statusRequest_struct{
	unsigned char statusType;
	
	void print(){
		printf("\tType : 0X%x \n",statusType);
	}
};

struct statusResponse_struct{
	//unsigned unsigned char UOID[20];
	unsigned char UOID[20];
	unsigned short hostInfoLength;
	unsigned short port;
	char hostname[20];
	
	char msgData[200];
	
	void print(){
		printf("\tUOID\t: %s\n\tHosInfoLength\t: %d\n\t Replying Port\t:%d\n\tReplying Hostname\t:%s\n",UOID,hostInfoLength,port,hostname);
		printf(" Data : \n");
		
		/*char hname[24];
		int recordLength = -1;
		unsigned short p;
		int h = 0;
		memcpy(&recordLength,msgData,4);
		printf(" Record Length : %d \n",recordLength);
		while(recordLength != 0){
			
			memset(&p,0,sizeof(p));
			memset(hname,0,sizeof(hname));
			
			h +=4;
			printf(" Record Length : %d \n",recordLength);
			memcpy(&p,(msgData+h),sizeof(p));
			h += sizeof(p);
			printf(" Port : %d \n",p);
			memcpy(hname,(msgData+h),recordLength - sizeof(p));
			h += recordLength - sizeof(p);
			printf(" Hostname : %s \t Port : %d \n",hname,p);
					
			memcpy(&recordLength,(msgData+h),4);			
		}
		
		h += 4;
		memcpy(&p,(msgData+h),sizeof(p));
		h += sizeof(p);
		printf(" Port : %d \n",p);
		memcpy(hname,(msgData+h),sizeof(msgData) - h);
		printf(" Hostname : %s \t Port : %d \n",hname,p);
		*/
	}
};

struct checkResponse_struct{
	unsigned char UOID[20];
};
struct message_struct{
	char s_r_f;
	friend ofstream & operator<<(ofstream &, const message_struct &);
	int msgLifeTime;
	char hostname[20];
	unsigned short port;
	struct commonHeader_struct cHeader;
	struct hello_struct helloMsg;
	struct joinRequest_struct joinRequestMsg;
	struct joinResponse_struct joinResponseMsg;
	struct statusRequest_struct statusRequestMsg;
	struct statusResponse_struct statusResponseMsg;
	struct checkResponse_struct checkResponseMsg;
	int connectionID;
	int joinTimeOut;
};

struct connection_struct{
	int ID;
	unsigned short port;
	char hostname[20];
	int socket;
	char s_r_f;
	
	pthread_t initiator;
	pthread_mutex_t writeQueue_mutex;
	pthread_mutex_t writeThread_mutex;
	pthread_cond_t writeQueue_cond;
	pthread_cond_t writeThread_cond;
	unsigned int writeQueue;
	bool writeThreadStatus;
	
	struct commonHeader_struct cHeader;
	struct hello_struct helloMsg;
	struct joinRequest_struct joinRequestMsg;
	struct joinResponse_struct joinResponseMsg;
	struct statusRequest_struct statusRequestMsg;
	struct statusResponse_struct statusResponseMsg;
	struct checkResponse_struct checkResponseMsg;
	
	double timestamp;
	unsigned int keepAliveTimeOut;
};

extern char *homeDir;
extern char *logFileName;
extern char *nodeID;
extern char *hostname;
extern char *initNeighborListFileName;

extern struct connection_struct connections[1000];

extern struct node_struct beacon_nodes[100],neighbor_nodes[100];

extern list<struct message_struct> msgTable;
extern list<struct message_struct>::iterator it1,it2;
extern map<int,struct neighbor_struct> neighborMap;
extern map<int,struct neighbor_struct>::iterator it3,it4,it5;


extern pthread_cond_t clientConnect_cond[10],UI_cond;
extern pthread_mutex_t msgTable_mutex,shutDown_mutex,clientConnect_mutex,connection_mutex,UI_mutex;
extern pthread_t tempReadT,tempWriteT,client[20],server,timer,readT[50],writeT[50],userInput;

extern unsigned int ID;
extern struct arg_struct argForThread[100];
extern ofstream logFile;
extern ifstream initNeighborListFile;

extern bool reset;
extern bool iAmABeacon;
extern bool shutDown;
extern bool client_killed[20];
extern bool initNeighborListFileAvailable;
extern bool tempWriteDie;
extern bool softRestart;
extern unsigned short port;
extern unsigned int location;
extern int autoShutDown;
extern int retry;
extern unsigned int noOfBeacons;
extern unsigned int TTL;
extern unsigned int msgLifeTime;
extern unsigned int noOfConnections;
extern unsigned int noOfNeighbors;
extern unsigned int initNeighbors;
extern unsigned int minNeighbors;
extern unsigned int joinTimeOut;
extern unsigned int keepAliveTimeOut;

extern void initialize(struct commonHeader_struct *cHeader,struct hello_struct *helloMsg,struct joinRequest_struct *joinRequestMsg);

#ifndef read_write_H
#define read_write_H

extern void *readThread(void *);
extern void *tempReadThread(void *);
extern void *writeThread(void *);
extern void *tempWriteThread(void *);
extern void sendProcedure(int);

#endif /* ~read_write_H */

#ifndef utilities_H
#define utilities_H

extern void initialize(struct commonHeader_struct,struct hello_struct,struct joinRequest_struct);
extern ofstream & operator<<(ofstream &, const message_struct &);
extern unsigned char *getUOID(unsigned char *,unsigned char *,unsigned char *,unsigned int);
extern char *getNodeInstanceID();
extern bool timeIsUp();
extern bool thePacketIsDuplicate(struct commonHeader_struct);
extern void forwardThisPacket(int,struct connection_struct);
extern bool UOIDCompare(unsigned char *,unsigned char *,int);
extern char *printUOID(unsigned char *);
extern char *myWriteByte(unsigned char);
extern char myWrite4Bits(unsigned char);
extern double secSinceLastMessage();
extern bool nodesAreNeighbors(int ,int);
extern void readInitFile();
extern void cleanUp();
//extern bool distanceCompare(const joinResponse_struct, const joinResponse_struct);


#endif /* ~utilities_H */
