/**********************
*
* Progam Name: MP1. Membership Protocol.
* 
* Code authors: harish2
*
* Current file: mp2_node.h
* About this file: Header file.
* 
***********************/

#ifndef _NODE_H_
#define _NODE_H_

#include "stdincludes.h"
#include "params.h"
#include "queue.h"
#include "requests.h"
#include "emulnet.h"

/* Configuration Parameters */
char JOINADDR[30];                    /* address for introduction into the group. */
extern char *DEF_SERVADDR;            /* server address. */
extern short PORTNUM;                /* standard portnum of server to contact. */

/* Miscellaneous Parameters */
extern char *STDSTRING;

/* globals for the Tfail and Tcleanup 
THRESHOLD is for failure detection and CLEANUP is for deletion*/

#define THRESHOLD MAX_NNB
#define CLEANUP (MAX_NNB*2)

typedef struct member{            
        struct address addr;            // my address
        int inited;                     // boolean indicating if this member is up
        int ingroup;                    // boolean indiciating if this member is in the group

        queue inmsgq;                   // queue for incoming messages

        int bfailed;                    // boolean indicating if this member has failed

        int totalMembers;               //Total number of entries in the membership list
        int heartbeatCount;             //Current heartbeat Counter at this node
        int currentTime;                //Current time at the node
        int initialized;                //Flag whether any dynamic memory had been allocated for this ndoe

        address* membershipList;        //An array of addresses in the Membership List
        int* lastAlive;                 //An array to hold when was the last time we updated the entry for a node
        int* timeStamp;                 //The heartbeat counter for a particular node

} member;

/* Message types */
/* Meaning of different message types
  JOINREQ - request to join the group
  JOINREP - replyto JOINREQ
  RECVGOSSIP - to receive gossips from other peers
*/
enum Msgtypes{
		JOINREQ,			
		JOINREP,
    RECVGOSSIP,
		DUMMYLASTMSGTYPE
};

/* Generic message template. */
typedef struct messagehdr{ 	
	enum Msgtypes msgtype;
} messagehdr;


/* Functions in mp2_node.c */

/* Message processing routines. */
STDCLLBKRET Process_joinreq STDCLLBKARGS;
STDCLLBKRET Process_joinrep STDCLLBKARGS;
STDCLLBKRET Process_recvgossip STDCLLBKARGS;
/*
int recv_callback(void *env, char *data, int size);
int init_thisnode(member *thisnode, address *joinaddr);
*/

/*
*
*Functions I defined
*
*/

//A function to allocate memory for the node's
//Membership list
void Initialize(member *node);
//A function to add self to the 
//Membership List
void selfAdd(member *node);


/*
Other routines.
*/

void nodestart(member *node, char *servaddrstr, short servport);
void nodeloop(member *node);
int recvloop(member *node);
int finishup_thisnode(member *node);

#endif /* _NODE_H_ */

