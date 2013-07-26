#include "common.h"

void sig_r(int signum){
	//printf(" READ THREAD - Connection : Signal received %d \n",signum);
}

void sig_w(int signum){
	//printf(" WRITE THREAD - Connection : Signal received %d \n",signum);
}

void sendProcedure(int i){
	//printf(" THREAD : Waiting for queue lock \n");
	pthread_mutex_lock(&connections[i].writeQueue_mutex);
						
	
	connections[i].writeQueue++;
	//printf(" THREAD : Write queue %d \n",connections[i].writeQueue);
	if(connections[i].writeThreadStatus == 1){
		//printf(" THREAD : Added to wait queue \n");
	}
	else{
		pthread_mutex_lock(&connections[i].writeThread_mutex);
		//printf(" THREAD : Signalling thread CV of Connection %d \n",i);
		pthread_cond_signal(&connections[i].writeThread_cond);
		pthread_mutex_unlock(&connections[i].writeThread_mutex);	
	}
	//printf(" THREAD : Going for wait in queue CV of Connection %d\n",i);
	pthread_cond_wait(&connections[i].writeQueue_cond,&connections[i].writeQueue_mutex);
	//printf(" THREAD : Waking up, waiting for interaction lock \n");
	pthread_mutex_unlock(&connections[i].writeQueue_mutex);
	pthread_mutex_lock(&connections[i].writeThread_mutex);
	//printf(" THREAD : Signalling inteaction CV \n");
	pthread_cond_signal(&connections[i].writeThread_cond);
	//printf(" THREAD : Waiting on inteaction CV \n");
	pthread_cond_wait(&connections[i].writeThread_cond,&connections[i].writeThread_mutex);
	
	return;
}

void *tempReadThread(void *arg){
	
	struct commonHeader_struct cHeader;
	struct hello_struct helloMsg;
	struct joinRequest_struct joinRequestMsg;
	struct joinResponse_struct joinResponseMsg;
	
	struct message_struct msgEntry;
	
	char buf[100];
	int bytes_recieved = 0;
		
	struct arg_struct *argForThread = (struct arg_struct *)arg;
	//signal(SIGUSR2,sig_r);
	signal(SIGUSR1,sig_r);
	
	static sigset_t unmask;
	sigemptyset (&unmask);
    sigaddset (&unmask, SIGUSR1);
	if(pthread_sigmask(SIG_UNBLOCK, &unmask, NULL) != 0)
		printf("\n Error blocking signal from main thread");
	
	static sigset_t mask;
	sigemptyset (&mask);
    sigaddset (&mask, SIGUSR2);
	sigaddset (&mask, SIGINT);
    //sigaddset (&mask, SIGUSR1);
	if(pthread_sigmask(SIG_BLOCK, &mask, NULL) != 0)
		printf("\n Error blocking signal from READ THREAD - Connection");
		
	while(1){
		memset(buf,0,100);
		memset(&msgEntry,0,sizeof(msgEntry));
		initialize(&cHeader,&helloMsg,&joinRequestMsg);
		//printf("\n READ THREAD - Connection %d : Waiting for data",argForThread->ID);
		fflush(stdout);
		bytes_recieved = recv(connections[argForThread->ID].socket,buf,sizeof(cHeader),0);
		//printf("\n READ THREAD - Connection %d : Data received",argForThread->ID);
		if(bytes_recieved == 0){
			if(errno == EINTR){
				close(connections[argForThread->ID].socket);
				//printf(" READ THREAD - Connection %d : Terminating \n",argForThread->ID);
				return NULL;
			}
		}
		else if(bytes_recieved == -1){
			close(connections[argForThread->ID].socket);
			//printf(" READ THREAD - Connection %d : Terminating \n",argForThread->ID);
			return NULL;
		}
		
		memcpy(&cHeader,buf,sizeof(cHeader));
		cHeader.dataLength = ntohl(cHeader.dataLength);
		//cHeader.print();
		
		if(cHeader.dataLength > 0)
			bytes_recieved = recv(connections[argForThread->ID].socket,buf,cHeader.dataLength,0);
		if(bytes_recieved == 0){
			if(errno == EINTR){
				close(connections[argForThread->ID].socket);
				//printf(" READ THREAD - Connection %d : Terminating \n",argForThread->ID);
				return NULL;
			}
		}
		else if(bytes_recieved == -1){
			close(connections[argForThread->ID].socket);
			//printf(" READ THREAD - Connection %d : Terminating \n",argForThread->ID);
			return NULL;
		}
		
		connections[argForThread->ID].timestamp = secSinceLastMessage();
		
		//printf(" READ THREAD - Connection %d : Timestamp %f \n",argForThread->ID,connections[argForThread->ID].timestamp);
		pthread_mutex_lock(&msgTable_mutex);
		
		if(thePacketIsDuplicate(cHeader)){
			//printf(" READ THREAD %d : Duplicate message. Dropping the packet 0X%x\n",argForThread->ID,(int)cHeader.msgType);
			//cHeader.print();
			pthread_mutex_unlock(&msgTable_mutex);
			continue;		
		}
		/*else
			printf(" READ THREAD %d : New message. Proceeding \n",argForThread->ID);*/
		msgEntry.cHeader = cHeader;
		msgEntry.connectionID = argForThread->ID;
		msgEntry.msgLifeTime = msgLifeTime;		
		msgEntry.s_r_f = 'r';
		
		
		if(cHeader.msgType == (unsigned char)JNRS){						// Join Response
			memcpy(&joinResponseMsg,buf,cHeader.dataLength);
			//printf(" READ THREAD - Connection %d : Received join response \n",argForThread->ID);
			//joinResponseMsg.print();
			msgEntry.joinResponseMsg = joinResponseMsg;
			
			sprintf(msgEntry.hostname,"%s",connections[argForThread->ID].hostname);
			msgEntry.port = connections[argForThread->ID].port;
			
			
			msgTable.push_back(msgEntry);
			logFile<<msgEntry;
			
			int flag = 0;
			int connectionID = -1;
			for(it2 = msgTable.begin();it2 != msgTable.end();++it2){
				if(!memcmp(it2->cHeader.UOID,joinResponseMsg.UOID,20)){
					//printf(" READ THREAD - Connection %d : UOID matched, s_r_f : %c\n",argForThread->ID,it2->s_r_f);
					if(it2->s_r_f == 'r'){
						connectionID = it2->connectionID;
						flag = 1;
						break;						
					}
					else if(it2->s_r_f == 's'){
						//printf(" READ THREAD - Connection %d : Received a join response for my request \n",argForThread->ID);
						flag = 2;
						//statusResponseMsg.print();
					}
				}			
			}
			pthread_mutex_unlock(&msgTable_mutex);
			
			if(flag == 1){
				sendProcedure(connectionID);
				//printf(" READ THREAD - Connection %d : Forwarding the join response back to connection %d %s:%d\n",argForThread->ID,connectionID,connections[connectionID].hostname,connections[connectionID].port);
				memcpy(&connections[connectionID].cHeader,&cHeader,sizeof(cHeader));
				connections[connectionID].joinResponseMsg = joinResponseMsg;
				connections[connectionID].s_r_f = 'f';
				
				pthread_cond_signal(&connections[connectionID].writeThread_cond);
				//printf(" READ THREAD - Connection %d : Waiting for the join response to be forwarded to connection %d \n",argForThread->ID,connectionID);
				pthread_cond_wait(&connections[connectionID].writeThread_cond,&connections[connectionID].writeThread_mutex);
				//printf(" READ THREAD - Connection %d : Join response forwarded, signalling interaction CV of Connection %d so that it ll service the queue \n",argForThread->ID,connectionID);
				pthread_cond_signal(&connections[connectionID].writeThread_cond);
				pthread_mutex_unlock(&connections[connectionID].writeThread_mutex);
			}			
			else if(flag == 0){							// No join requests stored for this response, drop the response
				//printf(" READ THREAD %d : Nothing matched the entry table, dropping the join response \n",argForThread->ID);
				continue;
			}
			
		}
		else
			pthread_mutex_unlock(&msgTable_mutex);
		
	}
	return NULL;
}


void *readThread(void *arg){
	
	struct commonHeader_struct cHeader;
	struct hello_struct helloMsg;
	struct joinRequest_struct joinRequestMsg;
	struct joinResponse_struct joinResponseMsg;
	struct statusRequest_struct statusRequestMsg;
	struct statusResponse_struct statusResponseMsg;
	struct checkResponse_struct checkResponseMsg;
	struct message_struct msgEntry;
	
	char buf[300];
	unsigned int i = 0;
	int bytes_recieved = 0;
		
	struct arg_struct *argForThread = (struct arg_struct *)arg;
	signal(SIGUSR2,sig_r);
	//signal(SIGUSR1,sig_r);
	
	/*static sigset_t unmask;
	sigemptyset (&unmask);
    sigaddset (&unmask, SIGUSR2);
	if(pthread_sigmask(SIG_UNBLOCK, &unmask, NULL) != 0)
		printf("\n Error blocking signal from main thread");*/
	
	static sigset_t mask;
	sigemptyset (&mask);
    sigaddset (&mask, SIGUSR2);
    sigaddset (&mask, SIGPIPE);
    sigaddset (&mask, SIGALRM);
    sigaddset (&mask, SIGUSR1);
	sigaddset (&mask, SIGINT);
	if(pthread_sigmask(SIG_BLOCK, &mask, NULL) != 0)
		printf("\n Error blocking signal from READ THREAD - Connection");
		
	while(1){
		memset(buf,0,100);
		memset(&msgEntry,0,sizeof(msgEntry));
		initialize(&cHeader,&helloMsg,&joinRequestMsg);
		//printf("\n READ THREAD - Connection %d : Waiting for data",argForThread->ID);
		fflush(stdout);
		bytes_recieved = recv(connections[argForThread->ID].socket,buf,sizeof(cHeader),0);
		//printf("\n READ THREAD - Connection %d : Data received",argForThread->ID);
		if(bytes_recieved == 0){
			if(errno == EINTR){
				close(connections[argForThread->ID].socket);
				//printf(" READ THREAD - Connection %d : Terminating \n",argForThread->ID);
				return NULL;
			}
		}
		else if(bytes_recieved == -1){
			close(connections[argForThread->ID].socket);
			//printf(" READ THREAD - Connection %d : Terminating \n",argForThread->ID);
			return NULL;
		}
		
		memcpy(&cHeader,buf,sizeof(cHeader));
		cHeader.dataLength = ntohl(cHeader.dataLength);
		//cHeader.print();
		
		if(cHeader.dataLength > 0)
			bytes_recieved = recv(connections[argForThread->ID].socket,buf,cHeader.dataLength,0);
		if(bytes_recieved == 0){
			if(errno == EINTR){
				close(connections[argForThread->ID].socket);
				//printf(" READ THREAD - Connection %d : Terminating \n",argForThread->ID);
				return NULL;
			}
		}
		else if(bytes_recieved == -1){
			close(connections[argForThread->ID].socket);
			//printf(" READ THREAD - Connection %d : Terminating \n",argForThread->ID);
			return NULL;
		}
		
		connections[argForThread->ID].timestamp = secSinceLastMessage();
		
		//printf(" READ THREAD - Connection %d : Timestamp %f \n",argForThread->ID,connections[argForThread->ID].timestamp);
		pthread_mutex_lock(&msgTable_mutex);
		
		if(thePacketIsDuplicate(cHeader)){
			//printf(" READ THREAD %d : Duplicate message. Dropping the packet 0X%x\n",argForThread->ID,(int)cHeader.msgType);
			//cHeader.print();
			pthread_mutex_unlock(&msgTable_mutex);
			continue;		
		}
		/*else
			printf(" READ THREAD %d : New message. Proceeding \n",argForThread->ID);*/
		msgEntry.cHeader = cHeader;
		msgEntry.connectionID = argForThread->ID;
		msgEntry.msgLifeTime = msgLifeTime;		
		msgEntry.s_r_f = 'r';
		
		if(cHeader.msgType == (unsigned char)HLLO){
			memcpy(&helloMsg,buf,cHeader.dataLength);
			helloMsg.port = ntohs(helloMsg.port);
			
			sprintf(connections[argForThread->ID].hostname,"%s",helloMsg.hostname);
			connections[argForThread->ID].port = helloMsg.port;
			connections[argForThread->ID].keepAliveTimeOut = 0;

			sprintf(msgEntry.hostname,"%s",connections[argForThread->ID].hostname);
			msgEntry.port = connections[argForThread->ID].port;
			msgEntry.helloMsg = helloMsg;
			//cHeader.print();
			//helloMsg.print();
			
			
			msgTable.push_back(msgEntry);
			logFile<<msgEntry;
			pthread_mutex_unlock(&msgTable_mutex);
			
			for(i=0;i<noOfBeacons;i++){
				if(helloMsg.port == beacon_nodes[i].port){
					//if(pthread_kill(client[i],SIGUSR1) == 0)
					client_killed[i] = true;
					//printf(" READ THREAD - Connection %d : Corresponding client thread (thread ID : %d) with port %d killed \n",argForThread->ID,client[i],helloMsg.port);
					break;
				}
			}
			bool flag = 0;
			for(i=0;i<noOfNeighbors;i++){
				if(neighbor_nodes[i].port == helloMsg.port){
					flag = 1;
					break;
				}
			}
			for(i=0;i<noOfBeacons;i++){
				if(beacon_nodes[i].port == helloMsg.port && iAmABeacon){
					flag = 1;
					break;
				}
			}
			
			if(flag == 0){
				
				sendProcedure(argForThread->ID);
				struct commonHeader_struct temp_cHeader;
				
				//printf(" READ THREAD - Connection %d : Generating the join response to %s:%d\n",argForThread->ID,connections[argForThread->ID].hostname,connections[argForThread->ID].port);
				getUOID((unsigned char *)getNodeInstanceID(),(unsigned char *)"msg",temp_cHeader.UOID,20);
				temp_cHeader.msgType = (unsigned char)HLLO;
				temp_cHeader.TTL = (char)TTL;
				
				
				connections[argForThread->ID].helloMsg.port = port;
				sprintf(connections[argForThread->ID].helloMsg.hostname,"%s",hostname);
				
				temp_cHeader.dataLength = sizeof(connections[argForThread->ID].joinResponseMsg);
				temp_cHeader.dataLength = htonl(temp_cHeader.dataLength);
				
				connections[argForThread->ID].cHeader = temp_cHeader;
				connections[argForThread->ID].s_r_f = 's';
						
				pthread_cond_signal(&connections[argForThread->ID].writeThread_cond);
				//printf(" READ THREAD - Connection %d : Waiting for the join response to be sent \n",argForThread->ID);
				pthread_cond_wait(&connections[argForThread->ID].writeThread_cond,&connections[argForThread->ID].writeThread_mutex);
				//printf(" READ THREAD - Connection %d : Join response sent, signalling interaction CV of Connection %d so that it ll service the queue \n",argForThread->ID,argForThread->ID);
				pthread_cond_signal(&connections[argForThread->ID].writeThread_cond);
				pthread_mutex_unlock(&connections[argForThread->ID].writeThread_mutex);

			}
			
		}
		else if(cHeader.msgType == (unsigned char)JNRQ){
					
			memcpy(&joinRequestMsg,buf,cHeader.dataLength);
			connections[argForThread->ID].keepAliveTimeOut = 0;
			joinRequestMsg.port = ntohs(joinRequestMsg.port);
			connections[argForThread->ID].port = joinRequestMsg.port;
			sprintf(connections[argForThread->ID].hostname,"%s",joinRequestMsg.hostname);
			
			msgEntry.joinTimeOut = joinTimeOut;
			msgEntry.joinRequestMsg = joinRequestMsg;
			cHeader.TTL--;
			//printf(" READ THREAD %d : Received join request \n",argForThread->ID);
			//cHeader.print();
			//joinRequestMsg.print();
			
			sprintf(msgEntry.hostname,"%s",connections[argForThread->ID].hostname);
			msgEntry.port = connections[argForThread->ID].port;
			
			
			msgTable.push_back(msgEntry);
			logFile<<msgEntry;
			pthread_mutex_unlock(&msgTable_mutex);
			
			if(cHeader.TTL > 0){
								
				pthread_mutex_lock(&connection_mutex);
				
				for(i = 0;i<noOfConnections;i++){
					if((int)i != argForThread->ID){
						
						sendProcedure(i);
												
						memcpy(connections[i].cHeader.UOID,cHeader.UOID,20);
						connections[i].s_r_f = 'f';
						connections[i].cHeader.TTL = cHeader.TTL;
						connections[i].cHeader.msgType = cHeader.msgType;
						connections[i].cHeader.dataLength = htonl(cHeader.dataLength);
						
						connections[i].joinRequestMsg = joinRequestMsg;
													
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
			}
			
			joinRequestMsg.location = ntohl(joinRequestMsg.location);
			sendProcedure(argForThread->ID);
			struct commonHeader_struct temp_cHeader;
			
			//printf(" READ THREAD - Connection %d : Generating the join response to %s:%d\n",argForThread->ID,connections[argForThread->ID].hostname,connections[argForThread->ID].port);
			getUOID((unsigned char *)getNodeInstanceID(),(unsigned char *)"msg",temp_cHeader.UOID,20);
			temp_cHeader.msgType = (unsigned char)JNRS;
			temp_cHeader.TTL = (char)TTL;
			
			
			memcpy(&connections[argForThread->ID].joinResponseMsg.UOID,&cHeader.UOID,20);
			connections[argForThread->ID].joinResponseMsg.port = port;
			sprintf(connections[argForThread->ID].joinResponseMsg.hostname,"%s",hostname);
			connections[argForThread->ID].joinResponseMsg.distance = max(joinRequestMsg.location,location) - min(joinRequestMsg.location,location); 
			
			temp_cHeader.dataLength = sizeof(connections[argForThread->ID].joinResponseMsg);
			temp_cHeader.dataLength = htonl(temp_cHeader.dataLength);
			
			connections[argForThread->ID].cHeader = temp_cHeader;
			connections[argForThread->ID].s_r_f = 's';
					
			pthread_cond_signal(&connections[argForThread->ID].writeThread_cond);
			//printf(" READ THREAD - Connection %d : Waiting for the join response to be sent \n",argForThread->ID);
			pthread_cond_wait(&connections[argForThread->ID].writeThread_cond,&connections[argForThread->ID].writeThread_mutex);
			//printf(" READ THREAD - Connection %d : Join response sent, signalling interaction CV of Connection %d so that it ll service the queue \n",argForThread->ID,argForThread->ID);
			pthread_cond_signal(&connections[argForThread->ID].writeThread_cond);
			pthread_mutex_unlock(&connections[argForThread->ID].writeThread_mutex);
			
		}
		else if(cHeader.msgType == (unsigned char)JNRS){						// Join Response
			memcpy(&joinResponseMsg,buf,cHeader.dataLength);
			connections[argForThread->ID].keepAliveTimeOut = 0;
			//printf(" READ THREAD - Connection %d : Received join response \n",argForThread->ID);
			//joinResponseMsg.print();
			msgEntry.joinResponseMsg = joinResponseMsg;
			
			sprintf(msgEntry.hostname,"%s",connections[argForThread->ID].hostname);
			msgEntry.port = connections[argForThread->ID].port;
			
			
			msgTable.push_back(msgEntry);
			logFile<<msgEntry;
			
			int flag = 0;
			int connectionID = -1;
			for(it2 = msgTable.begin();it2 != msgTable.end();++it2){
				if(!memcmp(it2->cHeader.UOID,joinResponseMsg.UOID,20)){
					//printf(" READ THREAD - Connection %d : UOID matched, s_r_f : %c\n",argForThread->ID,it2->s_r_f);
					if(it2->s_r_f == 'r'){
						connectionID = it2->connectionID;
						flag = 1;
						break;						
					}
					else if(it2->s_r_f == 's'){
						//printf(" READ THREAD - Connection %d : Received a join response for my request \n",argForThread->ID);
						flag = 2;
						//statusResponseMsg.print();
					}
				}			
			}
			pthread_mutex_unlock(&msgTable_mutex);
			
			if(flag == 1){
				sendProcedure(connectionID);
				//printf(" READ THREAD - Connection %d : Forwarding the join response back to connection %d %s:%d\n",argForThread->ID,connectionID,connections[connectionID].hostname,connections[connectionID].port);
				cHeader.dataLength = htonl(cHeader.dataLength);
				memcpy(&connections[connectionID].cHeader,&cHeader,sizeof(cHeader));
				connections[connectionID].joinResponseMsg = joinResponseMsg;
				connections[connectionID].s_r_f = 'f';
				
				pthread_cond_signal(&connections[connectionID].writeThread_cond);
				//printf(" READ THREAD - Connection %d : Waiting for the join response to be forwarded to connection %d \n",argForThread->ID,connectionID);
				pthread_cond_wait(&connections[connectionID].writeThread_cond,&connections[connectionID].writeThread_mutex);
				//printf(" READ THREAD - Connection %d : Join response forwarded, signalling interaction CV of Connection %d so that it ll service the queue \n",argForThread->ID,connectionID);
				pthread_cond_signal(&connections[connectionID].writeThread_cond);
				pthread_mutex_unlock(&connections[connectionID].writeThread_mutex);
			}			
			else if(flag == 0){							// No join requests stored for this response, drop the response
				//printf(" READ THREAD %d : Nothing matched the entry table, dropping the join response \n",argForThread->ID);
				continue;
			}
			
		}
		else if(cHeader.msgType == (unsigned char)STRQ){						// Status Request
		
			memcpy(&statusRequestMsg,buf,cHeader.dataLength);
			connections[argForThread->ID].keepAliveTimeOut = 0;
			//printf(" READ THREAD - Connection %d : Received status request \n",argForThread->ID);
			msgEntry.statusRequestMsg = statusRequestMsg;
			cHeader.TTL--;
			//cHeader.print();
			//statusRequestMsg.print();
			
			sprintf(msgEntry.hostname,"%s",connections[argForThread->ID].hostname);
			msgEntry.port = connections[argForThread->ID].port;
			
			msgTable.push_back(msgEntry);
			logFile<<msgEntry;
			pthread_mutex_unlock(&msgTable_mutex);
			
			
			if(cHeader.TTL > 0){
							
				pthread_mutex_lock(&connection_mutex);
				
				for(i = 0;i<noOfConnections;i++){
					if((int)i != argForThread->ID){
						
						sendProcedure(i);
						//printf(" READ THREAD %d : Forwarding the request \n",argForThread->ID);
						//connections[i].cHeader = connectionDataTobeForwarded.cHeader;
						
						memcpy(connections[i].cHeader.UOID,cHeader.UOID,20);
						connections[i].s_r_f = 'f';
						connections[i].cHeader.TTL = cHeader.TTL;
						connections[i].cHeader.msgType = cHeader.msgType;
						connections[i].cHeader.dataLength = htonl(cHeader.dataLength);
						
						connections[i].statusRequestMsg = statusRequestMsg;
													
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
			}
			
			char stathostname[20];
			unsigned short statport;
			int len=0;
			int h=0;
			pthread_mutex_lock(&connection_mutex);
			
			for (unsigned int i=0; i<noOfConnections;i++){
				memset(stathostname,0,sizeof(stathostname));
				memset(&statport,0,sizeof(statport));
				strcpy(stathostname,connections[i].hostname);
				statport = (connections[i].port);
				
				if((i+1) == noOfConnections)
					len = 0;
				else
					len= 2+sizeof(stathostname);
				//printf(" Added neighbor : %s : %d \n",connections[i].hostname,connections[i].port);
				//printf(" READ THREAD %d : Added neighbor : %s : %d \n",argForThread->ID,stathostname,statport);
				memcpy((statusResponseMsg.msgData+h),&len,4);
				h=h+4;
				memcpy((statusResponseMsg.msgData+h),&statport,sizeof(statport));
				h=h+sizeof(statport);
				memcpy((statusResponseMsg.msgData+h),stathostname,sizeof(stathostname));	
				h=h+sizeof(stathostname);
			}
			
			pthread_mutex_unlock(&connection_mutex);
			sendProcedure(argForThread->ID);
						
			getUOID((unsigned char *)getNodeInstanceID(),(unsigned char *)"msg",connections[argForThread->ID].cHeader.UOID,20);
			connections[argForThread->ID].cHeader.msgType = (unsigned char)STRS;
			connections[argForThread->ID].cHeader.TTL = (char)TTL;
			connections[argForThread->ID].s_r_f = 's';
			
			memcpy(connections[argForThread->ID].statusResponseMsg.msgData,statusResponseMsg.msgData,sizeof(statusResponseMsg.msgData));	
			memcpy(&connections[argForThread->ID].statusResponseMsg.UOID,&cHeader.UOID,20);
			connections[argForThread->ID].statusResponseMsg.hostInfoLength = (sizeof(port)+strlen(hostname));
			connections[argForThread->ID].statusResponseMsg.port = port;
			strcpy(connections[argForThread->ID].statusResponseMsg.hostname,hostname);			
			
			connections[argForThread->ID].cHeader.dataLength = sizeof(connections[argForThread->ID].statusResponseMsg);
			connections[argForThread->ID].cHeader.dataLength = htonl(connections[argForThread->ID].cHeader.dataLength);
			connections[argForThread->ID].s_r_f = 's';
					
					
			pthread_cond_signal(&connections[argForThread->ID].writeThread_cond);
			pthread_cond_wait(&connections[argForThread->ID].writeThread_cond,&connections[argForThread->ID].writeThread_mutex);
			pthread_cond_signal(&connections[argForThread->ID].writeThread_cond);
			pthread_mutex_unlock(&connections[argForThread->ID].writeThread_mutex);
		}
		else if(cHeader.msgType == (unsigned char)STRS){
			
			memcpy(&statusResponseMsg,buf,cHeader.dataLength);
			connections[argForThread->ID].keepAliveTimeOut = 0;	
			//cHeader.print();
			//printf(" READ THREAD - Connection %d : Received status response \n",argForThread->ID);
						
			//statusResponseMsg.print();
			
			msgEntry.statusResponseMsg = statusResponseMsg;
			
			sprintf(msgEntry.hostname,"%s",connections[argForThread->ID].hostname);
			msgEntry.port = connections[argForThread->ID].port;
			
			
			msgTable.push_back(msgEntry);
			logFile<<msgEntry;
			
			int flag = 0;
			int connectionID = -1;
			for(it2 = msgTable.begin();it2 != msgTable.end();++it2){
				if(!memcmp(it2->cHeader.UOID,statusResponseMsg.UOID,20)){
					//printf(" READ THREAD - Connection %d : UOID matched, s_r_f : %c\n",argForThread->ID,it2->s_r_f);
					if(it2->s_r_f == 'r'){
						connectionID = it2->connectionID;
						flag = 1;
						break;						
					}
					else if(it2->s_r_f == 's'){
						//printf(" READ THREAD - Connection %d : Received a status response for my request \n",argForThread->ID);
						flag = 2;
					}
				}			
			}
			pthread_mutex_unlock(&msgTable_mutex);
			
			if(flag == 1){
				sendProcedure(connectionID);
				cHeader.dataLength = htonl(cHeader.dataLength);
				//printf(" READ THREAD - Connection %d : Forwarding the status response back to connection %d %s:%d\n",argForThread->ID,connectionID,connections[connectionID].hostname,connections[connectionID].port);
				memcpy(&connections[connectionID].cHeader,&cHeader,sizeof(cHeader));
				connections[connectionID].statusResponseMsg = statusResponseMsg;
				connections[connectionID].s_r_f = 'f';
				
				pthread_cond_signal(&connections[connectionID].writeThread_cond);
				//printf(" READ THREAD - Connection %d : Waiting for the status response to be forwarded to connection %d \n",argForThread->ID,connectionID);
				pthread_cond_wait(&connections[connectionID].writeThread_cond,&connections[connectionID].writeThread_mutex);
				//printf(" READ THREAD - Connection %d : Status response forwarded, signalling interaction CV of Connection %d so that it ll service the queue \n",argForThread->ID,connectionID);
				pthread_cond_signal(&connections[connectionID].writeThread_cond);
				pthread_mutex_unlock(&connections[connectionID].writeThread_mutex);
			}			
			else if(flag == 0){							// No status requests stored for this response, drop the response
				//printf(" READ THREAD %d : Nothing matched the entry table, dropping the join response \n",argForThread->ID);
				continue;
			}
		}
		else if(cHeader.msgType == (unsigned char)CKRQ){
			
			sprintf(msgEntry.hostname,"%s",connections[argForThread->ID].hostname);
			msgEntry.port = connections[argForThread->ID].port;
			msgTable.push_back(msgEntry);
			logFile<<msgEntry;
			pthread_mutex_unlock(&msgTable_mutex);
			//printf(" READ THREAD %d : Received check request \n",argForThread->ID);
			if(iAmABeacon){
				sendProcedure(argForThread->ID);
				memcpy(connections[argForThread->ID].checkResponseMsg.UOID,cHeader.UOID,20);
				
				getUOID((unsigned char *)getNodeInstanceID(),(unsigned char *)"msg",connections[argForThread->ID].cHeader.UOID,20);
				
				connections[argForThread->ID].cHeader.msgType = (unsigned char)CKRS;
				connections[argForThread->ID].cHeader.TTL = (char)TTL;
				connections[argForThread->ID].s_r_f = 's';
				connections[argForThread->ID].cHeader.dataLength = sizeof(checkResponseMsg);
				connections[argForThread->ID].cHeader.dataLength = htonl(connections[argForThread->ID].cHeader.dataLength);
				
				pthread_cond_signal(&connections[argForThread->ID].writeThread_cond);
				pthread_cond_wait(&connections[argForThread->ID].writeThread_cond,&connections[argForThread->ID].writeThread_mutex);
				pthread_cond_signal(&connections[argForThread->ID].writeThread_cond);
				pthread_mutex_unlock(&connections[argForThread->ID].writeThread_mutex);

				//printf(" READ THREAD %d : Sent check response \n",argForThread->ID);
			}
			else if(cHeader.TTL >= 0){
				pthread_mutex_lock(&connection_mutex);
				
				for(i = 0;i<noOfConnections;i++){
					if((int)i != argForThread->ID){
						
						sendProcedure(i);
						cHeader.dataLength = htonl(cHeader.dataLength);
						memcpy(connections[i].cHeader.UOID,cHeader.UOID,20);
						connections[i].s_r_f = 'f';
						connections[i].cHeader.TTL = cHeader.TTL;
						connections[i].cHeader.msgType = cHeader.msgType;
						connections[i].cHeader.dataLength = cHeader.dataLength;
																		
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
				
			}
		}
		else if(cHeader.msgType == (unsigned char)CKRS){
			memcpy(&checkResponseMsg,buf,cHeader.dataLength);
			msgEntry.checkResponseMsg = checkResponseMsg;
			int connectionID = -1;
			sprintf(msgEntry.hostname,"%s",connections[argForThread->ID].hostname);
			msgEntry.port = connections[argForThread->ID].port;
			msgTable.push_back(msgEntry);
			logFile<<msgEntry;
			int flag = 0;
			for(it2 = msgTable.begin();it2 != msgTable.end();++it2){
				if(!memcmp(it2->cHeader.UOID,checkResponseMsg.UOID,20)){
					//printf(" READ THREAD - Connection %d : UOID matched, s_r_f : %c\n",argForThread->ID,it2->s_r_f);
					if(it2->s_r_f == 'r'){
						connectionID = it2->connectionID;
						flag = 1;
						break;						
					}
					else if(it2->s_r_f == 's'){
						//printf(" READ THREAD - Connection %d : Received a check response for my request \n",argForThread->ID);
						flag = 2;
						//statusResponseMsg.print();
					}
				}			
			}
			pthread_mutex_unlock(&msgTable_mutex);
			
			if(flag == 1){
				sendProcedure(connectionID);
				//printf(" READ THREAD - Connection %d : Forwarding the join response back to connection %d %s:%d\n",argForThread->ID,connectionID,connections[connectionID].hostname,connections[connectionID].port);
				
				memcpy(&connections[connectionID].cHeader,&cHeader,sizeof(cHeader));
				connections[connectionID].checkResponseMsg = checkResponseMsg;
				connections[connectionID].s_r_f = 'f';
				cHeader.dataLength = htonl(cHeader.dataLength);
				pthread_cond_signal(&connections[connectionID].writeThread_cond);
				//printf(" READ THREAD - Connection %d : Waiting for the join response to be forwarded to connection %d \n",argForThread->ID,connectionID);
				pthread_cond_wait(&connections[connectionID].writeThread_cond,&connections[connectionID].writeThread_mutex);
				//printf(" READ THREAD - Connection %d : Join response forwarded, signalling interaction CV of Connection %d so that it ll service the queue \n",argForThread->ID,connectionID);
				pthread_cond_signal(&connections[connectionID].writeThread_cond);
				pthread_mutex_unlock(&connections[connectionID].writeThread_mutex);
			}			
			else if(flag == 0){							// No join requests stored for this response, drop the response
				//printf(" READ THREAD %d : Nothing matched the entry table, dropping the join response \n",argForThread->ID);
				continue;
			}			
		}
		else if(cHeader.msgType == (unsigned char)KPAV){
			//printf(" READ THREAD %d : Received keepalive message from %s : %d \n",argForThread->ID,connections[argForThread->ID].hostname,connections[argForThread->ID].port);
			connections[argForThread->ID].keepAliveTimeOut = 0;
			//printUOID(cHeader.UOID);
			
			sprintf(msgEntry.hostname,"%s",connections[argForThread->ID].hostname);
			msgEntry.port = connections[argForThread->ID].port;
			
			msgTable.push_back(msgEntry);
			logFile<<msgEntry;
			pthread_mutex_unlock(&msgTable_mutex);
		}
		else
			pthread_mutex_unlock(&msgTable_mutex);
		
	}
	return NULL;
}

void *tempWriteThread(void *arg){
	struct arg_struct *argForThread = (struct arg_struct *)arg;
	struct message_struct msgEntry;
	
	char buf[300];
	bool flag = 1;
	//signal(SIGUSR2,sig_w);
	//signal(SIGUSR1,sig_w);
	
	/*static sigset_t unmask;
	sigemptyset (&unmask);
    sigaddset (&unmask, SIGUSR2);
	if(pthread_sigmask(SIG_UNBLOCK, &unmask, NULL) != 0)
		printf("\n Error blocking signal from WRITE THREAD - Connection");
	*/
	static sigset_t mask;
	sigemptyset (&mask);
    sigaddset (&mask, SIGUSR1);
	sigaddset (&mask, SIGUSR2);
	sigaddset (&mask, SIGINT);
	sigaddset (&mask, SIGALRM);
	if(pthread_sigmask(SIG_BLOCK, &mask, NULL) != 0)
		printf("\n Error blocking signal from WRITE THREAD - Connection");
	
	pthread_mutex_lock(&connections[argForThread->ID].writeQueue_mutex);
	pthread_mutex_lock(&connections[argForThread->ID].writeThread_mutex);
	connections[argForThread->ID].writeQueue = 0;
	connections[argForThread->ID].writeThreadStatus = 1;	
	
	
	while(1){
		memset(&msgEntry,0,sizeof(struct message_struct));
		memset(buf,0,sizeof(buf));
						
		if(timeIsUp()){
			pthread_mutex_unlock(&connections[argForThread->ID].writeThread_mutex);
			pthread_mutex_unlock(&connections[argForThread->ID].writeQueue_mutex);
			break;
		}
		flag = 1;
		if(connections[argForThread->ID].writeQueue > 0){
			connections[argForThread->ID].writeThreadStatus = 1;
			
			pthread_cond_signal(&connections[argForThread->ID].writeQueue_cond);
			connections[argForThread->ID].writeQueue--;
			pthread_mutex_unlock(&connections[argForThread->ID].writeQueue_mutex);
				
			pthread_cond_wait(&connections[argForThread->ID].writeThread_cond,&connections[argForThread->ID].writeThread_mutex);
			pthread_cond_signal(&connections[argForThread->ID].writeThread_cond);
			
			pthread_cond_wait(&connections[argForThread->ID].writeThread_cond,&connections[argForThread->ID].writeThread_mutex);
			memcpy(buf,&connections[argForThread->ID].cHeader,sizeof(connections[argForThread->ID].cHeader));
			send(connections[argForThread->ID].socket,buf,sizeof(connections[argForThread->ID].cHeader),0);
			//printf(" TEMP WRITE THREAD - Connection %d Sent cHeader : \n",argForThread->ID);
			//connections[argForThread->ID].cHeader.print();
			//printf(" TEMP WRITE THREAD - Connection %d Sent data : \n",argForThread->ID);
			
			if(connections[argForThread->ID].cHeader.msgType == (unsigned char)JNRQ){
				memcpy(buf,&connections[argForThread->ID].joinRequestMsg,connections[argForThread->ID].cHeader.dataLength);
				//connections[argForThread->ID].joinRequestMsg.print();
				msgEntry.joinTimeOut = joinTimeOut;
				msgEntry.joinRequestMsg = connections[argForThread->ID].joinRequestMsg;
			}
			if(connections[argForThread->ID].cHeader.dataLength > 0)
				send(connections[argForThread->ID].socket,buf,connections[argForThread->ID].cHeader.dataLength,0);
			
			memcpy(&msgEntry.cHeader,&connections[argForThread->ID].cHeader,sizeof(connections[argForThread->ID].cHeader));
			msgEntry.s_r_f =  connections[argForThread->ID].s_r_f;
			msgEntry.msgLifeTime = msgLifeTime;
			sprintf(msgEntry.hostname,"%s",connections[argForThread->ID].hostname);
			msgEntry.port = connections[argForThread->ID].port;
					
			pthread_mutex_lock(&msgTable_mutex);
			msgTable.push_back(msgEntry);
			logFile<<msgEntry;
			pthread_mutex_unlock(&msgTable_mutex);
						
			pthread_cond_signal(&connections[argForThread->ID].writeThread_cond);
		}
		else{
			//printf(" TEMP WRITE THREAD - Connection %d : No one is waiting in the queue for me \n",argForThread->ID);
			//printf(" TEMP WRITE THREAD - Connection %d : Setting status to 0 \n",argForThread->ID);
			connections[argForThread->ID].writeThreadStatus = 0;
			pthread_mutex_unlock(&connections[argForThread->ID].writeQueue_mutex);
		}
		//printf(" TEMP WRITE THREAD - Connection %d : Going for wait \n",argForThread->ID);
		pthread_cond_wait(&connections[argForThread->ID].writeThread_cond,&connections[argForThread->ID].writeThread_mutex);
		pthread_mutex_unlock(&connections[argForThread->ID].writeThread_mutex);
		//printf(" TEMP WRITE THREAD - Connection %d : Waking up \n",argForThread->ID);
		if(tempWriteDie){
			//printf(" TEMP WRITE THREAD - Connection %d : Terminating \n",argForThread->ID);
			break;
		}
		pthread_mutex_lock(&connections[argForThread->ID].writeThread_mutex);
		pthread_mutex_lock(&connections[argForThread->ID].writeQueue_mutex);
	}
	return NULL;
}


void *writeThread(void *arg){
	struct arg_struct *argForThread = (struct arg_struct *)arg;
	struct message_struct msgEntry;
	
	char buf[300];
	bool flag = 1;
	//signal(SIGUSR2,sig_w);
	//signal(SIGUSR1,sig_w);
	
	/*static sigset_t unmask;
	sigemptyset (&unmask);
    sigaddset (&unmask, SIGUSR2);
	if(pthread_sigmask(SIG_UNBLOCK, &unmask, NULL) != 0)
		printf("\n Error blocking signal from WRITE THREAD - Connection");
	*/
	static sigset_t mask;
	sigemptyset (&mask);
    sigaddset (&mask, SIGUSR1);
	sigaddset (&mask, SIGUSR2);
	sigaddset (&mask, SIGINT);
	sigaddset (&mask, SIGALRM);
	if(pthread_sigmask(SIG_BLOCK, &mask, NULL) != 0)
		printf("\n Error blocking signal from WRITE THREAD - Connection");
	
	pthread_mutex_lock(&connections[argForThread->ID].writeQueue_mutex);
	pthread_mutex_lock(&connections[argForThread->ID].writeThread_mutex);
	connections[argForThread->ID].writeQueue = 0;
	connections[argForThread->ID].writeThreadStatus = 1;	
	
	
	while(1){
		memset(&msgEntry,0,sizeof(struct message_struct));
		memset(buf,0,sizeof(buf));
						
		if(timeIsUp()){
			pthread_mutex_unlock(&connections[argForThread->ID].writeThread_mutex);
			pthread_mutex_unlock(&connections[argForThread->ID].writeQueue_mutex);
			break;
		}
		flag = 1;
		//printf(" WRITE THREAD - Connection %d : Write queue %d \n",argForThread->ID,connections[argForThread->ID].writeQueue);
		if(connections[argForThread->ID].writeQueue > 0){
			connections[argForThread->ID].writeThreadStatus = 1;
			//printf(" WRITE THREAD - Connection %d : Someone is waiting in the queue for me, signalling queue CV \n",argForThread->ID);			
			pthread_cond_signal(&connections[argForThread->ID].writeQueue_cond);
			connections[argForThread->ID].writeQueue--;
			pthread_mutex_unlock(&connections[argForThread->ID].writeQueue_mutex);
			
			//printf(" WRITE THREAD - Connection %d : Waiting on interaction CV \n",argForThread->ID);
			pthread_cond_wait(&connections[argForThread->ID].writeThread_cond,&connections[argForThread->ID].writeThread_mutex);
			//printf(" WRITE THREAD - Connection %d : Waking up \n",argForThread->ID);
			//printf(" WRITE THREAD - Connection %d : Signalling interaction CV \n",argForThread->ID);
			pthread_cond_signal(&connections[argForThread->ID].writeThread_cond);
			//printf(" WRITE THREAD - Connection %d : Waiting on interaction CV \n",argForThread->ID);
			pthread_cond_wait(&connections[argForThread->ID].writeThread_cond,&connections[argForThread->ID].writeThread_mutex);
			//printf(" WRITE THREAD - Connection %d : Waking up \n",argForThread->ID);
			//printf(" WRITE THREAD - Connection %d : Data to be sent is received\n",argForThread->ID);
			
			memcpy(buf,&connections[argForThread->ID].cHeader,sizeof(connections[argForThread->ID].cHeader));
			if(send(connections[argForThread->ID].socket,buf,sizeof(connections[argForThread->ID].cHeader),0) == EPIPE)
			{
				pthread_cond_signal(&connections[argForThread->ID].writeThread_cond);
				pthread_mutex_unlock(&connections[argForThread->ID].writeThread_mutex);
				close(connections[argForThread->ID].socket);
				break;
			}
			//printf(" WRITE THREAD - Connection %d Sent cHeader : \n",argForThread->ID);
			//connections[argForThread->ID].cHeader.print();
			//printf(" WRITE THREAD - Connection %d Sent data : \n",argForThread->ID);
			if(connections[argForThread->ID].cHeader.msgType == (unsigned char)HLLO){
				memcpy(buf,&connections[argForThread->ID].helloMsg,connections[argForThread->ID].cHeader.dataLength);
				//connections[argForThread->ID].helloMsg.print();
				msgEntry.helloMsg = connections[argForThread->ID].helloMsg;
			}
			else if(connections[argForThread->ID].cHeader.msgType == (unsigned char)JNRQ){
				memcpy(buf,&connections[argForThread->ID].joinRequestMsg,connections[argForThread->ID].cHeader.dataLength);
				//connections[argForThread->ID].joinRequestMsg.print();
				msgEntry.joinTimeOut = joinTimeOut;
				msgEntry.joinRequestMsg = connections[argForThread->ID].joinRequestMsg;
			}
			else if(connections[argForThread->ID].cHeader.msgType == (unsigned char)JNRS){
				memcpy(buf,&connections[argForThread->ID].joinResponseMsg,connections[argForThread->ID].cHeader.dataLength);
				//connections[argForThread->ID].joinResponseMsg.print();
				msgEntry.joinResponseMsg = connections[argForThread->ID].joinResponseMsg;
			}
			else if(connections[argForThread->ID].cHeader.msgType == (unsigned char)STRQ){
				memcpy(buf,&connections[argForThread->ID].statusRequestMsg,connections[argForThread->ID].cHeader.dataLength);
				//connections[argForThread->ID].statusRequestMsg.print();
				msgEntry.statusRequestMsg = connections[argForThread->ID].statusRequestMsg;
				pthread_mutex_lock(&msgTable_mutex);
				if(thePacketIsDuplicate(connections[argForThread->ID].cHeader))
					flag = 0;
				pthread_mutex_unlock(&msgTable_mutex);
			}
			else if(connections[argForThread->ID].cHeader.msgType == (unsigned char)STRS){
				memcpy(buf,&connections[argForThread->ID].statusResponseMsg,connections[argForThread->ID].cHeader.dataLength);
				//connections[argForThread->ID].statusResponseMsg.print();
				msgEntry.statusResponseMsg = connections[argForThread->ID].statusResponseMsg;
			}
			else if(connections[argForThread->ID].cHeader.msgType == (unsigned char)KPAV){
				//printf(" WRITE THREAD %d : Keepalive message sent \n",argForThread->ID);
				
			}
			else if(connections[argForThread->ID].cHeader.msgType == (unsigned char)CKRQ){
				msgEntry.joinTimeOut = joinTimeOut;
				
				//printf(" WRITE THREAD %d : check request message sent \n",argForThread->ID);
			}
			else if(connections[argForThread->ID].cHeader.msgType == (unsigned char)CKRS){
				memcpy(buf,&connections[argForThread->ID].checkResponseMsg,connections[argForThread->ID].cHeader.dataLength);
				//connections[argForThread->ID].statusResponseMsg.print();
				//printf(" WRITE THREAD %d : check response message sent \n",argForThread->ID);
				msgEntry.checkResponseMsg = connections[argForThread->ID].checkResponseMsg;
				
			}
			sprintf(msgEntry.hostname,"%s",hostname);
			msgEntry.port = port;
			if(connections[argForThread->ID].cHeader.dataLength > 0)
				if(send(connections[argForThread->ID].socket,buf,connections[argForThread->ID].cHeader.dataLength,0) == EPIPE)
				{
					pthread_cond_signal(&connections[argForThread->ID].writeThread_cond);
					pthread_mutex_unlock(&connections[argForThread->ID].writeThread_mutex);
					close(connections[argForThread->ID].socket);
					break;
				}
			
			memcpy(&msgEntry.cHeader,&connections[argForThread->ID].cHeader,sizeof(connections[argForThread->ID].cHeader));
			msgEntry.s_r_f =  connections[argForThread->ID].s_r_f;
			msgEntry.msgLifeTime = msgLifeTime;
					
			if(flag){
				pthread_mutex_lock(&msgTable_mutex);
				msgTable.push_back(msgEntry);				logFile<<msgEntry;
				pthread_mutex_unlock(&msgTable_mutex);
			}
			
				
			pthread_cond_signal(&connections[argForThread->ID].writeThread_cond);
		}
		else{
			//printf(" WRITE THREAD - Connection %d : No one is waiting in the queue for me \n",argForThread->ID);
			//printf(" WRITE THREAD - Connection %d : Setting status to 0 \n",argForThread->ID);
			connections[argForThread->ID].writeThreadStatus = 0;
			pthread_mutex_unlock(&connections[argForThread->ID].writeQueue_mutex);
		}
		//printf(" WRITE THREAD - Connection %d : Going for wait \n",argForThread->ID);
		pthread_cond_wait(&connections[argForThread->ID].writeThread_cond,&connections[argForThread->ID].writeThread_mutex);
		pthread_mutex_unlock(&connections[argForThread->ID].writeThread_mutex);
		//printf(" WRITE THREAD - Connection %d : Waking up \n",argForThread->ID);
		if(timeIsUp()){
			//printf(" WRITE THREAD - Connection %d : Terminating \n",argForThread->ID);
			break;
		}
		//printf(" WRITE THREAD - Connection %d : waiting for thread lock \n",argForThread->ID);
		pthread_mutex_lock(&connections[argForThread->ID].writeThread_mutex);
		//printf(" WRITE THREAD - Connection %d : waiting for queue lock \n",argForThread->ID);
		pthread_mutex_lock(&connections[argForThread->ID].writeQueue_mutex);
	}
	return NULL;
}
