/**********************
*
* Progam Name: MP1. Membership Protocol
* 
* Code authors: harish2
*
* Current file: mp1_node.c
* About this file: Member Node Implementation
* 
***********************/

#include "mp1_node.h"
#include "emulnet.h"
#include "MPtemplate.h"
#include "log.h"


/*
*
*Initialize the Membership data for the node
*Dynamically allocate memory for the nodes depending
*on the MAX_NNB variable.
*
*/
void Initialize(member *node){

    node->membershipList = malloc(sizeof(address)*MAX_NNB);
    node->lastAlive = malloc(sizeof(int) * MAX_NNB);
    node->timeStamp = malloc(sizeof(int)*MAX_NNB);
    node->initialized  = 1;
    node->totalMembers = 0;
    node->currentTime = getcurrtime();
    node->heartbeatCount = 0;
}

//add the node to it's own membership list
//The node is always the 0th element in it's own List
void selfAdd(member *node){

    memcpy(&node->membershipList[0], &node->addr, sizeof(address));
    node->lastAlive[0] = getcurrtime();
    node->timeStamp[0] = 0;
    logNodeAdd(&node->addr, &node->addr);
    node->totalMembers++;
}
/*
 *
 * Routines for introducer and current time.
 *
 */

char NULLADDR[] = {0,0,0,0,0,0};
int isnulladdr( address *addr){
    return (memcmp(addr, NULLADDR, 6)==0?1:0);
}

/* 
Return the address of the introducer member. 
*/
address getjoinaddr(void){

    address joinaddr;

    memset(&joinaddr, 0, sizeof(address));
    *(int *)(&joinaddr.addr)=1;
    *(short *)(&joinaddr.addr[4])=0;

    return joinaddr;
}

/*
 *
 * Message Processing routines.
 *
 */

/* 
*Received a Gossip message message.
*This function will parse the message and update
*its membership list accordingly.
*/
void Process_recvgossip(void *env, char *data, int size)
{
    member *node = (member *) env;
    int currTime = getcurrtime();


    //Get the total number of members received in the list
    int totalCountReceived = size / (sizeof(int) + sizeof(address));
    LOG(&node->addr, "Gossip received Count: %d", totalCountReceived);

    int timeStampOffset = (sizeof(address)*totalCountReceived);

    //Get pointers to the appripriate locations in the memory.
    address *receivedAddr = (address *) data;
    int *receivedStamps = (int *) (data + timeStampOffset);

    int iterator, count;
    int notFound;

    //iterate over and addresses just received
    for(count =0 ; count < totalCountReceived; count++){

        notFound = 1;

        //Iterate over the current Membership List to find a match 
        //So that an update can be performed. 
        for(iterator = 0; iterator< node->totalMembers; iterator++){

            //An entry found in our list, Update it
            //if the local time stamp is less than what we received
            if( memcmp( &(node->membershipList[iterator]), &(receivedAddr[count]), sizeof(address)) == 0 ){

                //check if the we received an updated time stamp
                if( receivedStamps[count] > node->timeStamp[iterator]){
                    node->timeStamp[iterator] = receivedStamps[count];
                    node->lastAlive[iterator] = currTime;
                }

                notFound = 0;
                break;

            }
        }

        //It is a new node which I haven't seen so far.
        //Must call logNodeAdd to add this node
        if(notFound == 1){

            memcpy( &(node->membershipList[node->totalMembers]), &(receivedAddr[count]), sizeof(address) );
            node->lastAlive[node->totalMembers] = currTime;
            node->timeStamp[node->totalMembers] = receivedStamps[count];
            logNodeAdd(&node->addr, &(node->membershipList[node->totalMembers]));
            node->totalMembers++;


            

        }

    }

    return;
}

/* 
*Received a JOINREQ (joinrequest) message.
*Add the node which requested join and reply it back with your address
*/
void Process_joinreq(void *env, char *data, int size)
{

    member *node = (member *) env;
    int currTime = getcurrtime();

    int iterator;
    int needToAdd = 1;
    
    //Add the newly added node to your membership list
    memcpy(&(node->membershipList[node->totalMembers]), data, sizeof(address));
    node->lastAlive[node->totalMembers] = currTime;
    node->timeStamp[node->totalMembers] = 1;

    logNodeAdd(&node->addr, &(node->membershipList[node->totalMembers]));
    node->totalMembers++;


    //Send a response message back to the node which
    //was just added in the group

    messagehdr *msg;
    size_t msgsize = sizeof(messagehdr) + sizeof(address);
    msg=malloc(msgsize);

    msg->msgtype = JOINREP;
    memcpy((char *) (msg+1), &node->addr, sizeof(address));

    MPp2psend(&node->addr, (address *) data , (char *)msg, msgsize);

    free(msg);
    
    return;
}

/* 
*Received a JOINREP (joinreply) message. 
*Add yourself to the group, add yourself to the membership list
*then add the introducer to the membership list
*/
void Process_joinrep(void *env, char *data, int size)
{
    member *node = (member *) env;
    int currTime = getcurrtime();

    //Add self to the group
    node->ingroup=1;
    Initialize(node);
    selfAdd(node);


    //Add the introducer to the Membership List
    memcpy(&(node->membershipList[node->totalMembers]),(address *)data, sizeof(address));
    node->lastAlive[node->totalMembers] = currTime;
    node->timeStamp[node->totalMembers] = 1;

    logNodeAdd(&(node->addr), data);


    node->totalMembers++;

    return;
}


/* 
Array of Message handlers. 
*/
void ( ( * MsgHandler [20] ) STDCLLBKARGS )={
/* Message processing operations at the P2P layer. */
    Process_joinreq, 
    Process_joinrep,
    Process_recvgossip
};

/* 
Called from nodeloop() on each received packet dequeue()-ed from node->inmsgq. 
Parse the packet, extract information and process. 
env is member *node, data is 'messagehdr'. 
*/
int recv_callback(void *env, char *data, int size){

    member *node = (member *) env;
    messagehdr *msghdr = (messagehdr *)data;
    char *pktdata = (char *)(msghdr+1);

    if(size < sizeof(messagehdr)){
#ifdef DEBUGLOG
        LOG(&((member *)env)->addr, "Faulty packet received - ignoring");
#endif
        return -1;
    }

#ifdef DEBUGLOG
    LOG(&((member *)env)->addr, "Received msg type %d with %d B payload", msghdr->msgtype, size - sizeof(messagehdr));
#endif

    if((node->ingroup && msghdr->msgtype >= 0 && msghdr->msgtype <= DUMMYLASTMSGTYPE)
        || (!node->ingroup && msghdr->msgtype==JOINREP))            
            /* if not yet in group, accept only JOINREPs */
        MsgHandler[msghdr->msgtype](env, pktdata, size-sizeof(messagehdr));
    /* else ignore (garbled message) */
    free(data);

    return 0;

}

/*
 *
 * Initialization and cleanup routines.
 *
 */

/* 
Find out who I am, and start up. 
*/
int init_thisnode(member *thisnode, address *joinaddr){
    
    if(MPinit(&thisnode->addr, PORTNUM, (char *)joinaddr)== NULL){ /* Calls ENInit */
#ifdef DEBUGLOG
        LOG(&thisnode->addr, "MPInit failed");
#endif
        exit(1);
    }
#ifdef DEBUGLOG
    else LOG(&thisnode->addr, "MPInit succeeded. Hello.");
#endif

    thisnode->bfailed=0;
    thisnode->inited=1;
    thisnode->ingroup=0;
    /* node is up! */

    return 0;
}


/* 
*Clean up this node. 
*/
int finishup_thisnode(member *node){

    //if this node was initialized and memory was allocated
    //dynamically, then free the allocated memory
    if(node->initialized){
        free(node->membershipList);
        free(node->lastAlive);
        free(node->timeStamp);
        node->ingroup =0;
    }


    return 0;
}


/* 
 *
 * Main code for a node 
 *
 */

/* 
Introduce self to group. 
*/
int introduceselftogroup(member *node, address *joinaddr){
    
    messagehdr *msg;
#ifdef DEBUGLOG
    static char s[1024];
#endif

    if(memcmp(&node->addr, joinaddr, 4*sizeof(char)) == 0){
        /* I am the group booter (first process to join the group). Boot up the group. */
#ifdef DEBUGLOG
        LOG(&node->addr, "Starting up group...");
#endif

        //Add the introducer in the group, initialize it and then 
        //add it to it's own membership list.
        node->ingroup = 1;
        Initialize(node);
        selfAdd(node);

    }
    else{
        size_t msgsize = sizeof(messagehdr) + sizeof(address);
        msg=malloc(msgsize);

    /* create JOINREQ message: format of data is {struct address myaddr} */
        msg->msgtype=JOINREQ;
        memcpy((char *)(msg+1), &node->addr, sizeof(address));

#ifdef DEBUGLOG
        sprintf(s, "Trying to join...");
        LOG(&node->addr, s);
#endif

    /* send JOINREQ message to introducer member. */
        MPp2psend(&node->addr, joinaddr, (char *)msg, msgsize);
        
        free(msg);
    }

    return 1;

}

/* 
Called from nodeloop(). 
*/
void checkmsgs(member *node){
    void *data;
    int size;

    /* Dequeue waiting messages from node->inmsgq and process them. */
	
    while((data = dequeue(&node->inmsgq, &size)) != NULL) {
        recv_callback((void *)node, data, size); 
    }
    return;
}


/* 
*Executed periodically for each member. 
*Performs necessary periodic operations. 
*Called by nodeloop(). 
*/
void nodeloopops(member *node){

    //increment the value of our heartbeat counter
    node->heartbeatCount++;
    int currTime = getcurrtime();

    //Update our heartbeat counter in the membership list
    node->lastAlive[0] = currTime;
    node->timeStamp[0]++;

    //Iterate over the loop and delete the entries that you dont need
    int iterator, count;

    for(iterator = 0; iterator <  node->totalMembers; iterator++){

        //delete this node if it hasn't been updated for more than
        //the Tcleanup
        if( (currTime - node->lastAlive[iterator]) > CLEANUP){

            //Log the removal 
            logNodeRemove(&node->addr, &(node->membershipList[iterator]));

            for(count = iterator; count< (node->totalMembers -1 ); count++){

                memcpy(&(node->membershipList[count]), &(node->membershipList[count+1]), sizeof(address));
                memcpy(&(node->lastAlive[count]), &(node->lastAlive[count+1]), sizeof(int));
                memcpy(&(node->timeStamp[count]), &(node->timeStamp[count+1]), sizeof(int));

            }

            //decrease the number of elements in the Membership List
            node->totalMembers--;
        }
    }


    //Allocate space for the Membership list to actually send.
    //This list does not include those that have been marked for failure
    //This is required so that we don't send the nodes that have been marked failed
    //But have not yet been deleted form the MembershipList

    int totalCountToSend = 0;

    address * nonFailed = malloc(sizeof(address)* node->totalMembers);
    int * timeVector = malloc(sizeof(int) * node->totalMembers);

    for(iterator = 0; iterator < node->totalMembers; iterator++){

        if((currTime - node->lastAlive[iterator]) < THRESHOLD){
            memcpy(&(nonFailed[totalCountToSend]) , &(node->membershipList[iterator]), sizeof(address));
            memcpy(&(timeVector[totalCountToSend]), &(node->timeStamp[iterator]), sizeof(int));
            totalCountToSend++;
        }
    }


    //create a message to to gossip to the peers
    messagehdr *msg;

    //Find the size of the new message, allocate memory for it
    size_t msgsize = sizeof(messagehdr) + ((sizeof(int) + sizeof(address)) * totalCountToSend);
    msg = malloc(msgsize);

    msg->msgtype=RECVGOSSIP;
    int addrSize = sizeof(address)*totalCountToSend;
    int vectorSize = sizeof(int)*totalCountToSend;

    //Copy over the two arrays into a single char* to send to the peers
    char *temp = msg+1;

    memcpy((char *) (temp), nonFailed, addrSize);
    memcpy((char *) (temp + addrSize), timeVector , vectorSize );



    //Implementing gossip protocol where random nodes are selected and are gossiped to.
    //In this scenario, I am choosing to send the gossip to 1/4th the number of peers
    //In the network
    int numNodesToSend = (node->totalMembers) / 2;
    int random;
    for(iterator = 0; iterator < numNodesToSend; iterator++){
        random = rand();
        random = (random % node->totalMembers) + 1;
        MPp2psend(&node->addr, &(node->membershipList[random]), (char *)msg, msgsize);
    }

    //Free all the dynamically allocated space in the function
    free(timeVector);
    free(nonFailed);
    free(msg);

    return;
}

/* 
Executed periodically at each member. Called from app.c.
*/
void nodeloop(member *node){
    if (node->bfailed) return;

    checkmsgs(node);

    /* Wait until you're in the group... */
    if(!node->ingroup) return ;

    /* ...then jump in and share your responsibilites! */
    nodeloopops(node);
    
    return;
}

/* 
All initialization routines for a member. Called by app.c. 
*/
void nodestart(member *node, char *servaddrstr, short servport){

    address joinaddr=getjoinaddr();

    /* Self booting routines */
    if(init_thisnode(node, &joinaddr) == -1){

#ifdef DEBUGLOG
        LOG(&node->addr, "init_thisnode failed. Exit.");
#endif
        exit(1);
    }

    if(!introduceselftogroup(node, &joinaddr)){
        finishup_thisnode(node);
#ifdef DEBUGLOG
        LOG(&node->addr, "Unable to join self to group. Exiting.");
#endif
        exit(1);
    }

    return;
}

/* 
Enqueue a message (buff) onto the queue env. 
*/
int enqueue_wrppr(void *env, char *buff, int size){    return enqueue((queue *)env, buff, size);}

/* 
Called by a member to receive messages currently waiting for it. 
*/
int recvloop(member *node){
    if (node->bfailed) return -1;
    else return MPrecv(&(node->addr), enqueue_wrppr, NULL, 1, &node->inmsgq); 
    /* Fourth parameter specifies number of times to 'loop'. */
}

