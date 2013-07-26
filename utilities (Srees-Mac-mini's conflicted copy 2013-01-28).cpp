#include "common.h"


extern ofstream & operator<<(ofstream & ofObject, const message_struct & msgEntry)
{
	struct timeval tv;
	gettimeofday(&tv,NULL);
	int n = strlen((char *)msgEntry.cHeader.UOID),i;
	char *temp = new char[4];
	memset(temp,0,4);
	
	ostringstream ret;
	ofObject<<"\n"<<msgEntry.cHeader.UOID;
	for(i = 0;i<n;i++)
		ret << hex << setfill('0') << setw(2) <<uppercase<<static_cast<unsigned long>(msgEntry.cHeader.UOID[i]);
		//memcpy((temp+(i-(n-4))),(msgEntry.cHeader.UOID+i),1);
	temp[i] = '\0';
	ofObject<<"\n"<<ret;
	ofObject<<"\n\t"<<msgEntry.s_r_f<<"\t"<<setw(10)<<tv.tv_sec<<"."<<setw(03)<<tv.tv_usec<<"\t"<<msgEntry.hostname<<"_"<<msgEntry.port<<"\t"<<hex<<showbase<<uppercase<<(int)msgEntry.cHeader.msgType<<"\t"<<(int)temp<<dec<<nouppercase<<noshowbase<<msgEntry.cHeader.TTL<<"\t";
	if(msgEntry.cHeader.msgType == (unsigned char)HLLO){
		ofObject<<msgEntry.helloMsg.hostname<<"\t"<<msgEntry.helloMsg.port;
	}
	else if(msgEntry.cHeader.msgType == (unsigned char)STRQ){
		ofObject<<hex<<showbase<<uppercase<<(int)msgEntry.statusRequestMsg.statusType<<dec<<nouppercase<<noshowbase;
	}
	return ofObject;
}


extern void initialize(struct commonHeader_struct *cHeader,struct hello_struct *helloMsg,struct joinRequest_struct *joinRequestMsg){
	//cHeader->UOID = (unsigned char *)malloc(20);;
	memset(&cHeader,0,sizeof(struct commonHeader_struct));
	memset(&helloMsg,0,sizeof(struct hello_struct));
	memset(&joinRequestMsg,0,sizeof(struct joinRequest_struct));
}


unsigned char *getUOID(unsigned char *node_inst_id,unsigned char *obj_type,unsigned char *uoid_buf,unsigned int uoid_buf_sz)
{
	static unsigned long seq_no=(unsigned long)1;
	char sha1_buf[SHA_DIGEST_LENGTH], str_buf[104];

	snprintf(str_buf, sizeof(str_buf), "%s_%s_%1ld",node_inst_id, obj_type, (long)seq_no++);
	SHA1((const unsigned char *)str_buf,strlen(str_buf),(unsigned char *)sha1_buf);
	memset(uoid_buf, 0, uoid_buf_sz);
	memcpy(uoid_buf, sha1_buf,min(uoid_buf_sz,sizeof(sha1_buf)));
	//printf("\n %s\t%d",uoid_buf,strlen((char *)uoid_buf));
	
	return uoid_buf;  
}

char *getNodeInstanceID(){
	time_t current_time;
	char *nodeInstanceID = new char[50];
	
	current_time = time(NULL);
	sprintf(nodeInstanceID,"%s_%ld",nodeID,current_time);
	
	return nodeInstanceID;
}

bool timeIsUp(){
	pthread_mutex_lock(&shutDown_mutex);
	if(shutDown == 1){
		pthread_mutex_unlock(&shutDown_mutex);
		return true;
	}
	else{
		pthread_mutex_unlock(&shutDown_mutex);
		return false;
	}
}

bool thePacketIsDuplicate(struct commonHeader_struct cHeader){
	
	bool flag = 0;															// Checking if the request message is a duplicate, if so, drop the message 
	//printf(" READ THREAD %d : Message table \n",argForThread->ID);
	for(it1 = msgTable.begin();it1 != msgTable.end();++it1){
		//printf(" %c\t%s\t0X%x\t%s\t%d \n",it1->s_r_f,it1->cHeader.UOID,(int)it1->cHeader.msgType,it1->hostname,it1->port);
		if(!memcmp(it1->cHeader.UOID,cHeader.UOID,20)){
			printf(" THREAD : Entry already in the table, dropping the request \n");
			flag = 1;
			break;
		}
	}
	
	return flag;
}

void forwardThisPacket(int exceptThisConnection,struct connection_struct connectionDataTobeForwarded){
	unsigned int i = 0;
	pthread_mutex_lock(&connection_mutex);
				
	for(i = 0;i<noOfConnections;i++){
		if((int)i != exceptThisConnection){
			
			sendProcedure(i);
			//printf(" READ THREAD %d : Forwarding the request \n",argForThread->ID);
			//connections[i].cHeader = connectionDataTobeForwarded.cHeader;
			
			memcpy(connections[i].cHeader.UOID,connectionDataTobeForwarded.cHeader.UOID,20);
			connections[i].s_r_f = connectionDataTobeForwarded.s_r_f;
			connections[i].cHeader.TTL = connectionDataTobeForwarded.cHeader.TTL;
			connections[i].cHeader.msgType = connectionDataTobeForwarded.cHeader.msgType;
			connections[i].cHeader.dataLength = connectionDataTobeForwarded.cHeader.dataLength;
			
			if(connections[i].cHeader.msgType == (unsigned char)JNRQ)
				connections[i].joinRequestMsg = connectionDataTobeForwarded.joinRequestMsg;
			else if(connections[i].cHeader.msgType == (unsigned char)STRQ)
				connections[i].statusRequestMsg = connectionDataTobeForwarded.statusRequestMsg;
				
			//printf(" READ THREAD %d : Signalling interaction CV of Connection %d \n",argForThread->ID,i);
			
			pthread_cond_signal(&connections[i].writeThread_cond);
			//printf(" READ THREAD %d : Waiting in the interaction CV of Connection %d \n",argForThread->ID,i);
			pthread_cond_wait(&connections[i].writeThread_cond,&connections[i].writeThread_mutex);
			//printf(" READ THREAD %d : Signalling interaction CV of Connection %d \n",argForThread->ID,i);
			pthread_cond_signal(&connections[i].writeThread_cond);
			pthread_mutex_unlock(&connections[i].writeThread_mutex);
		}
	}
	pthread_mutex_unlock(&connection_mutex);
	return;
}
	
bool UOIDCompare(unsigned char *UOID1,unsigned char *UOID2,int count){
	uint32_t a,b;
	int equal = 0;
	int i = 0;
	for(i=0;i<count;i++){
		a = (uint32_t)UOID1[i];
		b = (uint32_t)UOID2[i];
		if(a == b){
			equal++;
			//printf(" THREAD : UOID didnt match \n");
		}
	}
	if(equal == 20)
		return 0;
	return 1;
}

double secSinceLastMessage(){
	struct timeval time;
	gettimeofday(&time,NULL);
	return (time.tv_sec+(time.tv_usec/1000000.0));
}

void printUOID(unsigned char *UOID){
	for (int i=16; i < 20; i++)
		myWriteByte(UOID[i]);
	return;
}

void myWriteByte(unsigned char c) {
    myWrite4Bits((c>>4)&0x0f);
    myWrite4Bits(c&0x0f);
}

void myWrite4Bits(unsigned char c) {
    switch (c) {
		case 0: printf("0"); break;
		case 1: printf("1"); break;
		case 2: printf("2"); break;
		case 3: printf("3"); break;
		case 4: printf("4"); break;
		case 5: printf("5"); break;
		case 6: printf("6"); break;
		case 7: printf("7"); break;
		case 8: printf("8"); break;
		case 9: printf("9"); break;
		case 10: printf("a"); break;
		case 11: printf("b"); break;
		case 12: printf("c"); break;
		case 13: printf("d"); break;
		case 14: printf("e"); break;
		case 15: printf("f"); break;
	}
}

bool nodesAreNeighbors(int n1,int n2){
	bool r = 0;
	
	struct neighbor_struct tempNeighborFinder;
	
	it3 = neighborMap.find(n1);
	if(it3 == neighborMap.end()){
		it4 = neighborMap.find(n2);
		if(it4 == neighborMap.end()){
			//statusFile<<"l -t * -s "<<n2<<" -d "<<n1<<" -c blue"<<endl;
			r = 0;
			tempNeighborFinder.noOfNeighbors = 0;
			tempNeighborFinder.neighborsList[tempNeighborFinder.noOfNeighbors] = n1;
			tempNeighborFinder.noOfNeighbors++;
			neighborMap[n2] = tempNeighborFinder;
			
			tempNeighborFinder.noOfNeighbors = 0;
			tempNeighborFinder.neighborsList[tempNeighborFinder.noOfNeighbors] = n2;
			tempNeighborFinder.noOfNeighbors++;
			neighborMap[n1] = tempNeighborFinder;
			
		}
		else{
			//statusFile<<"l -t * -s "<<n2<<" -d "<<n1<<" -c blue"<<endl;
			r= 0;
			it5 = neighborMap.find(n2);
			tempNeighborFinder = it5->second;
			tempNeighborFinder.neighborsList[tempNeighborFinder.noOfNeighbors] = n1;
			tempNeighborFinder.noOfNeighbors++;
			neighborMap[n2] = tempNeighborFinder;
			
			
			tempNeighborFinder.noOfNeighbors = 0;
			tempNeighborFinder.neighborsList[tempNeighborFinder.noOfNeighbors] = n2;
			tempNeighborFinder.noOfNeighbors++;
			neighborMap[n1] = tempNeighborFinder;
				
			//statusFile<<"l -t * -s "<<n2<<" -d "<<n1<<" -c blue"<<endl;
		}
	}
	else{
		it4 = neighborMap.find(n2);
		if(it4 == neighborMap.end()){
			//statusFile<<"l -t * -s "<<n2<<" -d "<<n1<<" -c blue"<<endl;
			r = 0;
			it5 = neighborMap.find(n1);
			tempNeighborFinder = it5->second;
			tempNeighborFinder.neighborsList[tempNeighborFinder.noOfNeighbors] = n2;
			tempNeighborFinder.noOfNeighbors++;
			neighborMap[n1] = tempNeighborFinder;
			
			
			tempNeighborFinder.noOfNeighbors = 0;
			tempNeighborFinder.neighborsList[tempNeighborFinder.noOfNeighbors] = n1;
			tempNeighborFinder.noOfNeighbors++;
			neighborMap[n2] = tempNeighborFinder;
		
		}
		else{
			it5 = neighborMap.find(n1);
			tempNeighborFinder = it5->second;
			int f;
			for(f = 0;f<tempNeighborFinder.noOfNeighbors;f++){
				if(tempNeighborFinder.neighborsList[f] == n2)
					break;
			}
			if(f == tempNeighborFinder.noOfNeighbors){
				
				tempNeighborFinder.neighborsList[tempNeighborFinder.noOfNeighbors] = n2;
				tempNeighborFinder.noOfNeighbors++;
				neighborMap[n1] = tempNeighborFinder;
				
				it5 = neighborMap.find(n2);
				tempNeighborFinder = it5->second;
				tempNeighborFinder.neighborsList[tempNeighborFinder.noOfNeighbors] = n1;
				tempNeighborFinder.noOfNeighbors++;
				neighborMap[n2] = tempNeighborFinder;
															
				//statusFile<<"l -t * -s "<<n2<<" -d "<<n1<<" -c blue"<<endl;
				r = 0;
			}
			else
				r = 1;
		}
	}
	return r;
}

void readInitFile(){
	unsigned int i = 0;
	char *line=  new char[100];
	char *temp1=  new char[100];
	char *temp2=  new char[100];
	while(initNeighborListFile.good()){
		initNeighborListFile.getline(line,100);
		temp1 = strtok(line,":");
		if(temp1 != NULL){
			sprintf(neighbor_nodes[i].hostname,"%s",temp1);
			temp2 = strtok(NULL,"\0");
			neighbor_nodes[i].port = atoi(temp2);
			printf(" %s:%d \n",neighbor_nodes[i].hostname,neighbor_nodes[i].port);
			i++;
		}
	}
	noOfNeighbors = i;
	for(i=0;i<noOfNeighbors;i++)
	printf(" %s:%d \n",neighbor_nodes[i].hostname,neighbor_nodes[i].port);
	if(noOfNeighbors < minNeighbors)
		softRestart = 1;
	return;
}

void cleanUp(){
	for(unsigned int i = 0;i<noOfConnections;i++)
		memset(&connections[i],0,sizeof(connections[i]));
	noOfConnections = 0;
	msgTable.clear();
	pthread_mutex_destroy(&msgTable_mutex);
	pthread_mutex_destroy(&connection_mutex);
	if(initNeighborListFileAvailable)
		remove(initNeighborListFileName);
	remove(logFileName);
	return;
	
}
