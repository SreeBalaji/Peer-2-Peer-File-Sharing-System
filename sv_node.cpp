#include "sv_node.h"


bool distanceCompare(const joinResponse_struct& first, const joinResponse_struct& second)
{
  if (first.distance < second.distance)
    return true;
  else
    return false;
}

void shutdown(){
	unsigned int i = 0;
	pthread_mutex_lock(&shutDown_mutex);
	shutDown = 1;
	pthread_mutex_unlock(&shutDown_mutex);
	//printf(" SHUTOWN : Begins\n");
	//printf(" SHUTOWN : Thread List\n");
	pthread_mutex_trylock(&connection_mutex);
	for(i=0;i<noOfConnections;i++){
		//printf(" %d \n",connections[i].initiator);
		
		pthread_mutex_lock(&connections[i].writeThread_mutex);
		pthread_cond_signal(&connections[i].writeThread_cond);
		pthread_mutex_unlock(&connections[i].writeThread_mutex);
	}
	pthread_mutex_unlock(&connection_mutex);
	if(initNeighborListFileAvailable && noOfNeighbors >= initNeighbors){
		for(i=0;i<noOfBeacons;i++){
			if(port != beacon_nodes[i].port && !client_killed[i]){
				//printf(" SHUTDOWN : Client %d (Thread ID : %d) still alive, sending SIGALRM\n",i,client[i]);
				pthread_kill(client[i],SIGALRM);
			}
			//pthread_cond_signal(&clientConnect_cond[i]);
		}
	}
	//printf(" SHUTDOWN : Sending SIGUSR1 to server thread\n");
	pthread_kill(server,SIGUSR2);
	return;
}

void *timerThread(void *s){

	struct commonHeader_struct cHeader;
	static sigset_t mask;
	sigemptyset (&mask);
    
	sigaddset (&mask, SIGPIPE);
    sigaddset (&mask, SIGUSR2);
    sigaddset (&mask, SIGUSR1);
    sigaddset (&mask, SIGINT);	
    sigaddset (&mask, SIGALRM);
	if(pthread_sigmask(SIG_BLOCK, &mask, NULL) != 0)
		printf(" Error blocking signal from main thread\n");	
	
	while(1){
		pthread_mutex_lock(&msgTable_mutex);
		for(it1 = msgTable.begin();it1 != msgTable.end();++it1){
			it1->msgLifeTime--;
			if(it1->cHeader.msgType == (unsigned char)JNRQ){
				it1->joinTimeOut--;
				//printf(" TIMER THREAD : Join Timeout : %d\n",it1->joinTimeOut);
				if(it1->joinTimeOut == 0){
					if(it1->s_r_f == 's'){
						//printf(" TIMER THREAD : Join Timeout : %d, found a join request \n",it1->joinTimeOut);
						list<struct joinResponse_struct> sortedNeighbors;
						list<struct joinResponse_struct>::iterator it3;
						for(it2 = msgTable.begin();it2 != msgTable.end();++it2){
							if(it2->cHeader.msgType == (unsigned char)JNRS){ 
								if(!memcmp((char *)it2->joinResponseMsg.UOID,(char *)it1->cHeader.UOID,20)){
									//printf(" TIMER THREAD : Found neighbors %s:%d:%d \n",it2->joinResponseMsg.hostname,it2->joinResponseMsg.port,it2->joinResponseMsg.distance);
									sortedNeighbors.push_back(it2->joinResponseMsg);
								}
							}
						}
						sortedNeighbors.sort(distanceCompare);
						unsigned int i,totalNeighbors = (unsigned int)sortedNeighbors.size();
						if((unsigned int)sortedNeighbors.size() >= initNeighbors){
							ofstream newInitNeighborListFile;
							newInitNeighborListFile.open(initNeighborListFileName);
							
							if(newInitNeighborListFile.is_open()){
								//printf(" TIMER THREAD : %s %s \n",initNeighborListFileName,homeDir);
								for(it3 = sortedNeighbors.begin(),i=0;i < min(totalNeighbors,initNeighbors);++it3,i++)
									newInitNeighborListFile<<it3->hostname<<":"<<it3->port<<endl;
								noOfNeighbors = min(totalNeighbors,initNeighbors);
								newInitNeighborListFile.close();
								initNeighborListFileAvailable = true;
								pthread_kill(tempReadT,SIGUSR1);
								
								tempWriteDie = true;
								
								pthread_cond_signal(&connections[999].writeThread_cond);
								pthread_mutex_unlock(&connections[999].writeThread_mutex);
								
							}
						}
						else{
							noOfNeighbors = min(totalNeighbors,initNeighbors);
														
							tempWriteDie = true;
								
							pthread_cond_signal(&connections[999].writeThread_cond);
							pthread_mutex_unlock(&connections[999].writeThread_mutex);
							pthread_mutex_lock(&shutDown_mutex);
							shutDown = 1;
							pthread_mutex_unlock(&shutDown_mutex);
							pthread_kill(tempReadT,SIGUSR1);
							break;
						}
						
					}
					// erase the message	
					msgTable.erase(it1);
				}				
			}			
			else if(it1->cHeader.msgType == (unsigned char)CKRQ){
				it1->joinTimeOut--;
				if(it1->joinTimeOut == 0){
					if(it1->s_r_f == 's'){
						bool flag = 0;
						for(it2 = msgTable.begin();it2 != msgTable.end();++it2){
							if(it2->cHeader.msgType == (unsigned char)CKRS){ 
								if(!memcmp((char *)it2->checkResponseMsg.UOID,(char *)it1->cHeader.UOID,20)){
									//printf(" TIMER THREAD : I am still connected to the SERVANT network \n");
									flag = 1;
									break;									
								}	
							}
						}
						if(!flag){
							softRestart = 1;
							break;
						}
					}
				}
			}
			if(it1->msgLifeTime <= 0){
				if(it1->cHeader.msgType == (unsigned char)STRQ ){
					if(it1->s_r_f == 's'){
						ofstream statusFile;
						struct node_struct addednodes[100];
						int j = 0;
						remove(statusFileName);
						statusFile.open(statusFileName,ios::out );
						neighborMap.clear();
						if(statusFile.is_open()){
							statusFile<<"V -t * -v 1.a05"<<endl;
							statusFile<<"n -t * -s "<<port<<" -c red -i black"<<endl;
							
							addednodes[0].port = port;
							j++;
							
							for(it2 = msgTable.begin();it2 != msgTable.end();++it2){
								if(it2->cHeader.msgType == (unsigned char)STRS && !memcmp((char *)it2->statusResponseMsg.UOID,(char *)it1->cHeader.UOID,20)){
									
									bool flag = 0;
									for(int k = 0; k < j;k++){
										if(addednodes[k].port == it2->statusResponseMsg.port)
											flag = 1;
									}
									if(flag == 0)
										statusFile<<"n -t * -s "<<it2->statusResponseMsg.port<<" -c red -i black"<<endl;
									
									char hname[100][24];
									int recordLength = -1;
									unsigned short p[100];
									int h = 0,i = 0;
									
									addednodes[j].port =  it2->statusResponseMsg.port;
									j++;
									
									
									memcpy(&recordLength,it2->statusResponseMsg.msgData,4);
									//printf(" Record Length : %d \n",recordLength);
									
									for(i = 0;recordLength!= 0;i++){
										
										memset(&p[i],0,sizeof(p[i]));
										memset(hname[i],0,sizeof(hname[i]));
										
										h +=4;
										
										memcpy(&p[i],(it2->statusResponseMsg.msgData+h),sizeof(p[i]));
										h += sizeof(p[i]);
										
										flag = 0;
										for(int k = 0; k < j;k++){
											if(addednodes[k].port == p[i])
												flag = 1;
										}
										if(flag == 0){
											statusFile<<"n -t * -s "<<p[i]<<" -c red -i black"<<endl;
											addednodes[j].port = p[i];
											j++;
										}
										if(!nodesAreNeighbors(p[i],it2->statusResponseMsg.port)){
											statusFile<<"l -t * -s "<<it2->statusResponseMsg.port<<" -d "<<p[i]<<" -c blue"<<endl;
										}
										
										memcpy(hname[i],(it2->statusResponseMsg.msgData+h),recordLength - sizeof(p[i]));
										h += recordLength - sizeof(p[i]);										
										memcpy(&recordLength,(it2->statusResponseMsg.msgData+h),4);			
									}
									h += 4;
									memcpy(&p[i],(it2->statusResponseMsg.msgData+h),sizeof(p[i]));
									h += sizeof(p[i]);
									//printf(" Port : %d \n",p[i]);
									
									flag = 0;
									for(int k = 0; k < j;k++){
										if(addednodes[k].port == p[i])
											flag = 1;
									}
									if(flag == 0){
										statusFile<<"n -t * -s "<<p[i]<<" -c red -i black"<<endl;
										addednodes[j].port = p[i];
										j++;
									}
									if(!nodesAreNeighbors(p[i],it2->statusResponseMsg.port))
										statusFile<<"l -t * -s "<<it2->statusResponseMsg.port<<" -d "<<p[i]<<" -c blue"<<endl;
																		
									memcpy(hname[i],(it2->statusResponseMsg.msgData+h),sizeof(it2->statusResponseMsg.msgData) - h);
									
									msgTable.erase(it2);
								}
							}
						}
						if(statusFile.is_open())
							statusFile.close();
						pthread_mutex_lock(&UI_mutex);
						pthread_cond_signal(&UI_cond);
						pthread_mutex_unlock(&UI_mutex);
					}
				}
				msgTable.erase(it1);
			}
		}
		pthread_mutex_unlock(&msgTable_mutex);
		if(softRestart || timeIsUp())
			break;
		struct timeval tv;
		gettimeofday(&tv,NULL);
		
		pthread_mutex_lock(&connection_mutex);
		for(unsigned int i=0;i<noOfConnections;i++){
			connections[i].keepAliveTimeOut++;
			//printf(" READ THREAD - Connection %d : timeout %d \n",i,connections[i].keepAliveTimeOut);
			if(connections[i].keepAliveTimeOut == keepAliveTimeOut/2){
				
				getUOID((unsigned char *)getNodeInstanceID(),(unsigned char *)"msg",cHeader.UOID,20);
								
				cHeader.msgType = (unsigned char)KPAV;
				cHeader.TTL = (char)1;
				cHeader.dataLength = 0;
				cHeader.dataLength = htonl(cHeader.dataLength);
				
				sendProcedure(i);
				
				connections[i].cHeader = cHeader;
				connections[i].s_r_f = 's';
				
				pthread_cond_signal(&connections[i].writeThread_cond);
				//printf(" USER INPUT THREAD : Data to be sent is sent to write thread\n");
				pthread_cond_wait(&connections[i].writeThread_cond,&connections[i].writeThread_mutex);
				pthread_cond_signal(&connections[i].writeThread_cond);
				pthread_mutex_unlock(&connections[i].writeThread_mutex);	
			}
			else if(connections[i].keepAliveTimeOut == keepAliveTimeOut){
				//printf(" READ THREAD - Connection %d : Connection down \n",i);
				unsigned int j = 0;
				
				for(j = i;j<(noOfConnections-1);j++){
					connections[j] = connections[j+1];
				}
				noOfConnections--;
				
				if(!iAmABeacon){
					getUOID((unsigned char *)getNodeInstanceID(),(unsigned char *)"msg",cHeader.UOID,20);
									
					cHeader.msgType = (unsigned char)CKRQ;
					cHeader.TTL = (char)TTL;
					cHeader.dataLength = 0;
					//printf(" TIMER THREAD : Number of available connections : %d\n",noOfConnections);
					for(j = 0;j<noOfConnections;j++){
						sendProcedure(j);
						
						connections[j].cHeader = cHeader;
						connections[j].s_r_f = 's';
						
						pthread_cond_signal(&connections[j].writeThread_cond);
						pthread_cond_wait(&connections[j].writeThread_cond,&connections[j].writeThread_mutex);
						pthread_cond_signal(&connections[j].writeThread_cond);
						pthread_mutex_unlock(&connections[j].writeThread_mutex);
					}
				}
			}
		}
		pthread_mutex_unlock(&connection_mutex);
		//printf("\n Autoshutdown : %d",autoShutDown);
		if(--autoShutDown <= 0){
			break;
			//printf("\n Autoshutdown : %d",autoShutDown);
		}
		sleep(1);
		
	}
	//printf(" TIMER THREAD : Calling shutdown\n");
	shutdown();
	//printf(" TIMER THREAD : Terminating \n");
	return NULL;
}

void sig_server(int signum){
	
	//printf(" SERVER THREAD : Signal received %d\n",signum);
	return;
}

void *serverThread(void *arg){
	
	struct sockaddr_in server_addr,client_addr;
    socklen_t sin_size;
	int connected;
	
	int sock,yes = 1;
	unsigned int i;
	signal(SIGUSR2,sig_server);
	
	static sigset_t unmask;
	sigemptyset (&unmask);
    sigaddset (&unmask, SIGUSR2);
	if(pthread_sigmask(SIG_UNBLOCK, &unmask, NULL) != 0)
		printf(" Error blocking signal from main thread\n");
		
	static sigset_t mask;
	sigemptyset (&mask);
    sigaddset (&mask, SIGUSR1);
	sigaddset (&mask, SIGINT);
	if(pthread_sigmask(SIG_BLOCK, &mask, NULL) != 0)
		printf(" Error blocking signal from server thread\n");
	
	
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(port);
	server_addr.sin_addr.s_addr = INADDR_ANY;
	
	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("Socket");
        exit(1);
    }
	
	if (setsockopt(sock,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(int)) == -1) {
		perror("Setsockopt");
		exit(1);
	}	
	
	if (bind(sock, (struct sockaddr *)&server_addr, sizeof(struct sockaddr))
																   == -1) {
		perror("Unable to bind");
		exit(1);
	}

	if (listen(sock, 5) == -1) {
		perror("Listen");
		exit(1);
	}
	
	sin_size = sizeof(struct sockaddr_in);
	//printf(" SERVER THREAD : Port I am accepting on : %d\n",port);
	i = 0;
    while(1){
		if(timeIsUp()){
			close(sock);
			break;
		}
		connected = accept(sock, (struct sockaddr *)&client_addr,&sin_size);
		if(connected < 0){
			if(errno == EINTR){
				//printf(" SERVER THREAD : Terminating\n");
				close(sock);
				break;
			}
			else
				continue;
		}
		
		if(timeIsUp()){
			//printf(" SERVER THREAD : Terminating\n");
			close(connected);
			close(sock);
			break;
		}
			
		pthread_mutex_lock(&connection_mutex);
		//printf(" SERVER THREAD : Accepted a connection, Connection ID : %d \n",noOfConnections);	
		argForThread[noOfConnections].ID = noOfConnections;
		connections[noOfConnections].ID = noOfConnections;
		connections[noOfConnections].socket = connected;
		connections[noOfConnections].initiator = pthread_self();
		connections[noOfConnections].keepAliveTimeOut = 0;

		if (pthread_mutex_init(&connections[noOfConnections].writeQueue_mutex, NULL) != 0){
			perror(" ERROR AT INITIALIZING MUTEX\n");
			return NULL;
		}
		if (pthread_cond_init(&connections[noOfConnections].writeQueue_cond, NULL) != 0){
			perror(" ERROR AT INITIALIZING COND \n");
			return NULL;
		}
		if (pthread_mutex_init(&connections[noOfConnections].writeThread_mutex, NULL) != 0){
			perror(" ERROR AT INITIALIZING MUTEX\n");
			return NULL;
		}
		if (pthread_cond_init(&connections[noOfConnections].writeThread_cond, NULL) != 0){
			perror(" ERROR AT INITIALIZING COND\n");
			return NULL;
		}
		connections[noOfConnections].writeQueue = 0;
		if(pthread_create(&readT[noOfConnections],NULL,&readThread,&argForThread[noOfConnections]) < 0){
			printf("\n\tPthread creation failed");
			return 0;
		}
		if(pthread_create(&writeT[noOfConnections],NULL,&writeThread,&argForThread[noOfConnections]) < 0){
			printf("\n\tPthread creation failed");
			return 0;
		}
		noOfConnections++;
		i++;
		pthread_mutex_unlock(&connection_mutex);
		if(timeIsUp()){
			break;
		}
	}
	return NULL;
}

void printSigset(FILE *of,char *prefix,sigset_t *sigset)
{
    int sig, cnt;

    cnt = 0;
    for (sig = 1; sig < NSIG; sig++) {
        if (sigismember(sigset, sig)) {
            cnt++;
            fprintf(of," %s%d (%s)\n", prefix, sig, strsignal(sig));
        }
    }

    if (cnt == 0)
        fprintf(of," %s<empty signal set> \n", prefix);
}

void sig_client(int signum){
	//printf(" CLIENT THREAD : Signal received %d \n",signum);
}
void *clientThread(void *argS){
	struct arg_struct *arg;
	arg = (struct arg_struct *)argS;
	
	struct hostent *host;
    struct sockaddr_in server_addr;
	
	struct commonHeader_struct cHeader;
	
	struct timeval tv;
	struct timespec ts;
	
	//signal(SIGPIPE,SIG_IGN);
	signal(SIGUSR1,sig_client);
	signal(SIGALRM,sig_client);
	
		
	/*static sigset_t unmask;
	sigemptyset (&unmask);
    //sigaddset (&unmask, SIGUSR2);
	sigaddset (&unmask, SIGUSR1);
	if(pthread_sigmask(SIG_UNBLOCK, &unmask, NULL) != 0)
		printf(" Error blocking signal from main thread \n");
	*/
	static sigset_t mask;
	sigemptyset (&mask);
    sigaddset (&mask, SIGUSR1);
    sigaddset (&mask, SIGUSR2);
    sigaddset (&mask, SIGINT);
	if(pthread_sigmask(SIG_BLOCK, &mask, NULL) != 0)
		printf(" Error blocking signal from main thread \n");
		
	
	//printSigset(stdout,c,&mask);
	
	char *nodeInstanceID = new char[30];
	unsigned char *objectType = new unsigned char[10];
	
	client_killed[arg->ID] = false;
	
	memcpy(objectType,"msg",3);
	//printf(" CLIENT THREAD %d : Port I am connecting to : %d\n",arg->ID,beacon_nodes[arg->ID].port);
	
	int yes = 1;
	int sock;
	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("Socket");
        exit(1);
    }
	
	if (setsockopt(sock,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(int)) == -1) {
		perror("\nSetsockopt");
		exit(1);
	}	
	server_addr.sin_family = AF_INET;
	if(initNeighborListFileAvailable){
		server_addr.sin_port = htons(neighbor_nodes[arg->ID].port);
		host = gethostbyname(neighbor_nodes[arg->ID].hostname);
		//printf(" CLIENT THREAD %d : Connection to %d \n",arg->ID,neighbor_nodes[arg->ID].port);
	}
	else{
		server_addr.sin_port = htons(beacon_nodes[arg->ID].port);
		host = gethostbyname(beacon_nodes[arg->ID].hostname);
		//printf(" CLIENT THREAD %d : Connection to %d \n",arg->ID,beacon_nodes[arg->ID].port);
	}
    server_addr.sin_addr = *((struct in_addr *)host->h_addr);
	
	
	while(connect(sock,(struct sockaddr *)&server_addr,sizeof(struct sockaddr)) == -1)
	{
		if(!(iAmABeacon || initNeighborListFileAvailable))
			return NULL;
		if(timeIsUp() || client_killed[arg->ID] == true){
			close(sock);
			//printf(" CLIENT THREAD %d : Terminating for port %d\n",arg->ID,beacon_nodes[arg->ID].port);
			return NULL;
		}
		
       // printf(" CLIENT THREAD %d : Connection  refused for port %d. Going to Sleep for 30 seconds\n",arg->ID,beacon_nodes[arg->ID].port);
		gettimeofday(&tv,NULL);
		ts.tv_sec = tv.tv_sec + 30;
		ts.tv_nsec = 0;
		if(sleep(retry)>0){
			close(sock);
			client_killed[arg->ID] = true;
			//printf(" CLIENT THREAD %d : Terminating for port %d\n",arg->ID,beacon_nodes[arg->ID].port);
			return NULL;
		}
		
		if(timeIsUp() || client_killed[arg->ID] == true){
			close(sock);
			//printf(" CLIENT THREAD %d : Terminating for port %d\n",arg->ID,beacon_nodes[arg->ID].port);
			return NULL;
		}
		
		close(sock);
		if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
			perror("Socket");
			exit(1);
		}
		
		if (setsockopt(sock,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(int)) == -1) {
			perror("\nSetsockopt");
			exit(1);
		}	
		server_addr.sin_family = AF_INET;
		if(initNeighborListFileAvailable)
			server_addr.sin_port = htons(neighbor_nodes[arg->ID].port);
		else
			server_addr.sin_port = htons(beacon_nodes[arg->ID].port);
		server_addr.sin_addr = *((struct in_addr *)host->h_addr);
		
    }
	
	//printf(" CLIENT THREAD %d :Connected\n",arg->ID);
	
	pthread_mutex_lock(&connection_mutex);
	int myConnection;
	
	if(initNeighborListFileAvailable){
		connections[myConnection].port = neighbor_nodes[arg->ID].port;
		strcpy(connections[myConnection].hostname,neighbor_nodes[arg->ID].hostname);
	}
	
	if(!(iAmABeacon || initNeighborListFileAvailable)){
		myConnection = 999;
		connections[myConnection].port = beacon_nodes[arg->ID].port;
		strcpy(connections[myConnection].hostname,beacon_nodes[arg->ID].hostname);
	}
	else{
		myConnection = noOfConnections;
		noOfConnections++;
		connections[myConnection].port = beacon_nodes[arg->ID].port;
		strcpy(connections[myConnection].hostname,beacon_nodes[arg->ID].hostname);
	}
	
	pthread_mutex_unlock(&connection_mutex);
	
	connections[myConnection].ID = myConnection;
	connections[myConnection].socket = sock;
	argForThread[myConnection].ID = myConnection;
	connections[myConnection].initiator = pthread_self();
	connections[myConnection].keepAliveTimeOut = 0;
	
	if (pthread_mutex_init(&connections[myConnection].writeQueue_mutex, NULL) != 0)
    {
        perror(" ERROR AT INITIALIZING MUTEX\n");
        return NULL;
    }
	if (pthread_cond_init(&connections[myConnection].writeQueue_cond, NULL) != 0)
    {
        perror(" ERROR AT INITIALIZING COND\n");
        return NULL;
    }
	if (pthread_mutex_init(&connections[myConnection].writeThread_mutex, NULL) != 0){
		perror(" ERROR AT INITIALIZING MUTEX\n");
		return NULL;
	}
	if (pthread_cond_init(&connections[myConnection].writeThread_cond, NULL) != 0){
		perror(" ERROR AT INITIALIZING COND \n");
		return NULL;
	}
	
	
	sprintf(nodeInstanceID,"%s",getNodeInstanceID());
	//printf(" CLIENT THREAD %d : Node Instance ID : %s \n",arg->ID,nodeInstanceID);
	
	
	
	if(iAmABeacon || initNeighborListFileAvailable){   //  || you are starting from init_neighbor_list file
		
		if(pthread_create(&writeT[myConnection],NULL,&writeThread,&argForThread[myConnection]) < 0){
			printf("\n\tPthread creation failed");
			return 0;
		}
		if(pthread_create(&readT[myConnection],NULL,&readThread,&argForThread[myConnection]) < 0){
			printf("\n\tPthread creation failed");
			return 0;
		}
		
		sendProcedure(myConnection);
		
		getUOID((unsigned char *)nodeInstanceID,objectType,(unsigned char *)connections[myConnection].cHeader.UOID,20);
		
		connections[myConnection].cHeader.msgType = (unsigned char)HLLO;
		connections[myConnection].cHeader.TTL = (char)TTL;
		connections[myConnection].s_r_f = 's';
		connections[myConnection].cHeader.dataLength = (sizeof(port)+strlen(hostname));
		connections[myConnection].cHeader.dataLength = htonl(connections[myConnection].cHeader.dataLength);
		connections[myConnection].helloMsg.port = htons(port);
		sprintf(connections[myConnection].helloMsg.hostname,"%s",hostname);
		
		pthread_cond_signal(&connections[myConnection].writeThread_cond);
		pthread_cond_wait(&connections[myConnection].writeThread_cond,&connections[myConnection].writeThread_mutex);
		pthread_cond_signal(&connections[myConnection].writeThread_cond);
		pthread_mutex_unlock(&connections[myConnection].writeThread_mutex);
	}
	else{				// You dont have init_neighbor_list file and you are not a beacon, send join request
		
		
		if(pthread_create(&tempWriteT,NULL,&tempWriteThread,&argForThread[myConnection]) < 0){
		printf("\n\tPthread creation failed");
		return 0;
		}
		if(pthread_create(&tempReadT,NULL,&tempReadThread,&argForThread[myConnection]) < 0){
			printf("\n\tPthread creation failed");
			return 0;
		}
		
		sendProcedure(myConnection);
		
		//printf(" CLIENT THREAD %d : I am not a beacon and I dont have init_neighbor_list file\n",arg->ID);
		getUOID((unsigned char *)nodeInstanceID,objectType,(unsigned char *)cHeader.UOID,20);
		
		cHeader.msgType = (unsigned char)JNRQ;
		cHeader.TTL = (char)TTL;
		connections[myConnection].cHeader = cHeader;
		connections[myConnection].s_r_f = 's';
		
		connections[myConnection].joinRequestMsg.port = htons(port);
		connections[myConnection].joinRequestMsg.location = htonl(location);
		sprintf(connections[myConnection].joinRequestMsg.hostname,"%s",hostname);
		connections[myConnection].cHeader.dataLength = sizeof(connections[myConnection].joinRequestMsg);	
		connections[myConnection].cHeader.dataLength = htonl(connections[myConnection].cHeader.dataLength);
		
		pthread_cond_signal(&connections[myConnection].writeThread_cond);
		pthread_cond_wait(&connections[myConnection].writeThread_cond,&connections[myConnection].writeThread_mutex);
		pthread_cond_signal(&connections[myConnection].writeThread_cond);
		pthread_mutex_unlock(&connections[myConnection].writeThread_mutex);
		
		pthread_join(tempReadT,NULL);
		pthread_join(tempWriteT,NULL);
		
	}
	
	client_killed[arg->ID] = true;
	//pthread_join(writeT[temp],NULL);
	return NULL;
}

void sig_usr(int signum){
	printf(" Process Interrupted \n");
	pthread_mutex_lock(&msgTable_mutex);
	for(it1 = msgTable.begin();it1 != msgTable.end();++it1){
		if(it1->cHeader.msgType == (unsigned char)STRQ && it1->s_r_f == 's'){
			//printf(" USER INPUT THREAD :  Making the status request lifetime 0 \n");
			it1->msgLifeTime = 0;
		}
	}
	pthread_mutex_unlock(&msgTable_mutex);
}
void *userInputThread(void *s){
	char *uInput = new char[50];
	char *tempInput = new char[50];
	char *temp = new char[50];
	struct commonHeader_struct cHeader;
	struct statusRequest_struct statusRequestMsg;
	unsigned int i = 0;
	
	signal(SIGUSR1,sig_usr);
	signal(SIGUSR2,sig_usr);
	signal(SIGINT,sig_usr);
	
	static sigset_t mask;
	sigemptyset (&mask);
    
	sigaddset (&mask, SIGPIPE);
    sigaddset (&mask, SIGUSR2);
    sigaddset (&mask, SIGUSR1);
	if(pthread_sigmask(SIG_BLOCK, &mask, NULL) != 0)
		printf("\n Error blocking signal from main thread");	
	
	static sigset_t unmask;
	sigemptyset (&unmask);
	sigaddset (&unmask, SIGINT);
	if(pthread_sigmask(SIG_UNBLOCK, &mask, NULL) != 0)
		printf("\n Error blocking signal from main thread");
	
	fd_set set;
    
    FD_ZERO (&set);
    FD_SET (STDIN_FILENO, &set);
		
	while(1){
		memset(uInput,0,sizeof(uInput));
		memset(&cHeader,0,sizeof(struct commonHeader_struct));
		
		printf(" servant:%d >  ",port);
		fflush(stdout);
		int rc = select (FD_SETSIZE,&set, NULL, NULL,NULL);
		if(rc == EINTR)
			break;
		gets(uInput);
		strcpy(tempInput,uInput);
		temp = strtok(uInput," ");
		if(temp != NULL){
			if(!strcmp(temp,"shutdown")){
				shutdown();
				break;
			}
			else if(!strcmp(temp,"status")){
				temp = strtok(NULL," ");
				if(!strcmp(temp,"neighbors")){
					//printf(" Temp : %s \n",temp);
					temp = strtok(NULL," ");
					cHeader.TTL =(unsigned  char)(atoi(temp));
					temp = strtok(NULL,"\0");
					strcpy(statusFileName,temp);
					//printf(" USER INPUT THREAD : Read file name : %s \n",statusFileName);
					cHeader.msgType = (char)STRQ;
					//printf(" read TTL : %d \n",cHeader.TTL);
					getUOID((unsigned char *)getNodeInstanceID(),(unsigned char *)"msg",(unsigned char *)cHeader.UOID,20);
					cHeader.dataLength = sizeof(struct statusRequest_struct);
					cHeader.dataLength = ntohl(cHeader.dataLength);
					statusRequestMsg.statusType = (char)STRQN;
					
					pthread_mutex_lock(&connection_mutex);
					for(i = 0;i<noOfConnections;i++){
						sendProcedure(i);
						
						connections[i].cHeader = cHeader;
						connections[i].statusRequestMsg = statusRequestMsg;
						connections[i].s_r_f = 's';
						
						pthread_cond_signal(&connections[i].writeThread_cond);
						//printf(" USER INPUT THREAD : Data to be sent is sent to write thread\n");
						pthread_cond_wait(&connections[i].writeThread_cond,&connections[i].writeThread_mutex);
						pthread_cond_signal(&connections[i].writeThread_cond);
						pthread_mutex_unlock(&connections[i].writeThread_mutex);
					}
					pthread_mutex_unlock(&connection_mutex);
					
					if(noOfConnections >= 1){
						pthread_mutex_lock(&UI_mutex);
						pthread_cond_wait(&UI_cond,&UI_mutex);
						pthread_mutex_unlock(&UI_mutex);
					}
				}
				else if(!strcmp(temp,"files")){
				}
			}
		}
		else
			printf(" servant:%d > Invalid command \n",port);
	}
	return NULL;
			
}

void sig_main(int signum){
	//printf("\n MAIN THREAD : Signal received %d",signum);
	logFile.close();
}

void sigmain(int signum){
	if(signum == SIGINT)
		printf("\n Main : Signal received SIGINT");
	if(signum == SIGUSR1)
		printf("\n Main : Signal received SIGUSR1");
	if(signum == SIGUSR2)
		printf("\n Main : Signal received SIGUSR2");
	return;
}
int main(unsigned int argc,char **argv){
	unsigned int i,j;
	
	char *line = new char[100];
	char *ini_filename = new char[32],*temp = new char[32];
	
	struct arg_struct argForClientThread[100];
	
	initNeighborListFileName = new char[80];
	statusFileName = new char[80];
	homeDir = new char[100];
	nodeID = new char[50];
	hostname = new char[50];
	logFileName = new char[50];
	
	ID = (unsigned int)pthread_self();
	
	
	signal(SIGUSR1,sig_main);
	signal(SIGUSR2,sig_main);
	//signal(SIGINT,sig_main);
	
	static sigset_t mask;
	sigemptyset (&mask);
    
	sigaddset (&mask, SIGPIPE);
    sigaddset (&mask, SIGUSR2);
    sigaddset (&mask, SIGUSR1);
	//sigaddset (&mask, SIGINT);
	if(pthread_sigmask(SIG_BLOCK, &mask, NULL) != 0)
		printf("\n Error blocking signal from main thread");	
	
	for(i=0;i<argc;i++){
		if(!strcmp(argv[i],"-reset"))
			reset = true;
		else if(strcmp(argv[i],"./sv_node"))
			strcpy(ini_filename,argv[i]);
	}
	
	ifstream fp;
	fp.open(ini_filename,ios::in|ios::binary);
	
	noOfBeacons=0;
	
	while(1){
		
		reset = false;
		iAmABeacon = false;
		shutDown = false;
		initNeighborListFileAvailable = false;
		tempWriteDie = false; 
		softRestart = false;
		
		port = 0;
		location = 0;
		autoShutDown = 900;
		retry = 30;
		noOfBeacons = 0;
		TTL = 30;
		msgLifeTime = 30;
		noOfConnections = 0;
		initNeighbors = 3;
		minNeighbors = 2;
		joinTimeOut = 15;
		keepAliveTimeOut = 60;
		noOfNeighbors = 0;
		
		if(timeIsUp()){
			break;
		}
		
		if (pthread_mutex_init(&shutDown_mutex, NULL) != 0){
			perror("\tERROR AT INITIALIZING MUTEX");
			return 1;
		}
		
		if (pthread_mutex_init(&msgTable_mutex, NULL) != 0){
			perror("\tERROR AT INITIALIZING MUTEX");
			return 1;
		}
		if (pthread_mutex_init(&connection_mutex, NULL) != 0){
			perror("\tERROR AT INITIALIZING MUTEX");
			return 1;
		}		
		
		while(1){
			fp.getline(line,100);
			if(line[0] == ';')
				continue;
			else
				break;
		}
		
		if(!strcmp(line,"[init]")){
			while(fp.good()){
				fp.getline(line,100);
				if(line[0] == ';')
					continue;
				//printf("\n Line : %s",line);
				temp = strchr(line,'=');
				if(temp == NULL){
					fp.getline(line,100);
					if(line[0] == ';')
						continue;
					if(!strcmp(line,"[beacons]"))
						break;
					//printf("\n\tInvalid File \n");
					return 1;
				}
				*temp++ = '\0';
				//printf("\nLine after strchr : %s",line);
				//printf("\nTemp : %s",temp);
				if(!strcmp(strtok(line,"="),"Port"))
					port = atoi((temp));
				else if(!strcmp(strtok(line,"="),"Location"))
					location = atoi((temp));
				else if(!strcmp(strtok(line,"="),"HomeDir"))
					strcpy(homeDir,temp);
				else if(!strcmp(strtok(line,"="),"AutoShutdown"))
					autoShutDown = atoi(temp);		
				else if(!strcmp(strtok(line,"="),"TTL"))
					TTL = atoi(temp);
				else if(!strcmp(strtok(line,"="),"MsgLifetime"))
					msgLifeTime = atoi(temp);
				else if(!strcmp(strtok(line,"="),"LogFileName"))
					sprintf(logFileName,"%s/%s",homeDir,temp);
				else if(!strcmp(strtok(line,"="),"InitNeighbors"))
					initNeighbors = atoi(temp);
				else if(!strcmp(strtok(line,"="),"MinNeighbors"))
					minNeighbors = atoi(temp);
				else if(!strcmp(strtok(line,"="),"JoinTimeout"))
					joinTimeOut = atoi(temp);
				else if(!strcmp(strtok(line,"="),"KeepAliveTimeout"))
					keepAliveTimeOut = atoi(temp);
			}
		}
		if(!strcmp(line,"[beacons]")){
			while(fp.good()){
				fp.getline(line,100);
				//printf("\n Line at the end : %s",line);
				if(line[0] == ';' || line[0] == '\0')
					continue;
				temp = strtok(line,"=");
				if(!strcmp(temp,"Retry")){
					temp = strtok(NULL,"=");
					retry = atoi(temp);
				}
				else{
					char *temp1 = strtok(temp,":");
					strcpy(beacon_nodes[noOfBeacons].hostname,temp1);
					char *temp2 = strtok(NULL,":");
					beacon_nodes[noOfBeacons].port = atoi(temp2);
					//printf("\n Port : %d \t Hostname : %s\n",beacon_nodes[noOfBeacons].port,beacon_nodes[noOfBeacons].hostname);
					noOfBeacons++;
				}
			}
		}
		
		sprintf(logFileName,"%s/servant.log",homeDir);
		logFile.open(logFileName);
		if(!logFile.is_open()){
			printf("\n\tINVALID FILE\n");
			exit(1);
		}
		
		//printf(" Port : %d \t Location : %d \t HomeDir : %s MsgLifetime : %d JoinTimeOut : %d InitNeighbors : %d\n",port,location,homeDir,msgLifeTime,joinTimeOut,initNeighbors);
		for(j=0;j<noOfBeacons;j++){
			//printf(" Port : %d \t Hostname : %s\n",beacon_nodes[j].port,beacon_nodes[j].hostname);
			if(port == beacon_nodes[j].port)
				iAmABeacon = true;
		}
		
		if(pthread_create(&userInput,NULL,&userInputThread,NULL) < 0){
			printf("\n\tPthread creation failed");
			return 0;
		}
		if(gethostname(hostname,255)<0){
			perror("\n\tGETHOSTNAME : ");
			return 0;
		}
		sprintf(nodeID,"%s_%d",hostname,port);
		//printf("\n Node ID : %s",nodeID);
		
		sprintf(initNeighborListFileName,"%s/ini_neighbor_list",homeDir);
		//printf(" MAIN THREAD : Init neighbor list file with path : %s \n",initNeighborListFileName);
		initNeighborListFile.open(initNeighborListFileName);
		
		
		if(!initNeighborListFile.is_open()){
			//printf(" MAIN THREAD : No initNeighborListFile present \n");
		}
		else{
			initNeighborListFileAvailable = true;
			readInitFile();
			
			//printf(" MAIN THREAD : available neighbors \n");
			//for(i = 0;i<noOfNeighbors;i++)
			//	printf("%s : %d \n",neighbor_nodes[i].hostname,neighbor_nodes[i].port);
			if(softRestart){
				cleanUp();
				continue;
			}				
		}
		
		if(timeIsUp()){
			break;
		}
		
		if(pthread_create(&timer,NULL,&timerThread,NULL) < 0){
			printf("\n\tPthread creation failed");
			return 0;
		}
		if(!iAmABeacon){
			if(!(initNeighborListFileAvailable)){    // && noOfNeighbors >= minNeighbors
				for(i = 0;i<noOfBeacons;i++){
					argForClientThread[i].ID = i;
					if(pthread_create(&client[i],NULL,&clientThread,(void *)&argForClientThread[i]) < 0){
						printf("\n\tPthread creation failed");
						return 0;
					}
					pthread_join(client[i],NULL);
					
					if(initNeighborListFileAvailable && noOfNeighbors >= initNeighbors)
						break;
				}
				
				if(timeIsUp() || i == noOfBeacons )
					break;
				initNeighborListFile.open(initNeighborListFileName);
				
				if(initNeighborListFile.is_open())
					readInitFile();
			}
			//printf(" MAIN THREAD : no of neighbors : %d \n",noOfNeighbors);
			
			if(softRestart){
				cleanUp();
				softRestart=0;
				continue;
			}
			
			for(i = 0;i<noOfNeighbors;i++){
				argForClientThread[i].ID = i;
				//printf(" %d %d : %d \n",i,argForClientThread[i].ID,neighbor_nodes[i].port);
				if (pthread_cond_init(&clientConnect_cond[i], NULL) != 0)
				{
					perror("\tERROR AT INITIALIZING MUTEX");
					return 1;
				}						
				if(pthread_create(&client[i],NULL,&clientThread,(void *)&argForClientThread[i]) < 0){
					printf("\n\tPthread creation failed");
					return 0;
				}
				//printf(" MAIN THREAD : Created client thread ID : %d \n",client[i]);
			}
			
		}
		else{											// || Or I have init_neighbor_list file 
			for(i=0;i<noOfBeacons;i++){
				if(port != beacon_nodes[i].port){
					argForClientThread[i].ID = i;
					//printf(" %d %d : %d \n",i,argForClientThread[i].ID,beacon_nodes[i].port);
					if (pthread_cond_init(&clientConnect_cond[i], NULL) != 0)
					{
						perror("\tERROR AT INITIALIZING MUTEX");
						return 1;
					}						
					if(pthread_create(&client[i],NULL,&clientThread,(void *)&argForClientThread[i]) < 0){
						printf("\n\tPthread creation failed");
						return 0;
					}
					//printf(" MAIN THREAD : Created client thread ID : %d \n",client[i]);
				}
			}
		}
		
		if(timeIsUp()){
			break;
		}
		
		if(pthread_create(&server,NULL,&serverThread,NULL) < 0){
			printf("\n\tPthread creation failed");
			return 0;
		}
		//printf(" MAIN THREAD : Created server thread ID : %d \n",server);
		pthread_join(server,NULL);
		pthread_join(timer,NULL);
		/*for(i=0;i<max(noOfBeacons,noOfNeighbors);i++)
			if(port != beacon_nodes[i].port)
				pthread_join(client[i],NULL);*/
		if(timeIsUp())
			break;
	}
	//printf("\n MAIN THREAD : All the threads terminated \n");
	printf(" \n");
	logFile.close();
	
}
