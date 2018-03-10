#include <stdio.h>
#include <stdbool.h>

/* ******************************************************************
 ALTERNATING BIT AND GO-BACK-N NETWORK EMULATOR: VERSION 1.1  J.F.Kurose

   This code should be used for PA2, unidirectional or bidirectional
   data transfer protocols (from A to B. Bidirectional transfer of data
   is for extra credit and is not required).  Network properties:
   - one way network delay averages five time units (longer if there
     are other messages in the channel for GBN), but can be larger
   - packets can be corrupted (either the header or the data portion)
     or lost, according to user-defined probabilities
   - packets will be delivered in the order in which they were sent
     (although some can be lost).
**********************************************************************/

#define BIDIRECTIONAL 0    /* change to 1 if you're doing extra credit */
                           /* and write a routine called B_output */

/* a "msg" is the data unit passed from layer 5 (teachers code) to layer  */
/* 4 (students' code).  It contains the data (characters) to be delivered */
/* to layer 5 via the students transport level protocol entities.         */
struct msg {
  char data[20];
  };

/* a packet is the data unit passed from layer 4 (students code) to layer */
/* 3 (teachers code).  Note the pre-defined packet structure, which all   */
/* students must follow. */
struct pkt {
   int seqnum;
   int acknum;
   int checksum;
   char payload[20];
    };

/********* STUDENTS WRITE THE NEXT SEVEN ROUTINES *********/

float TIMER_INCREMENT = 20;

/*** GLOBAL VARIABLES FOR A ***/
enum HostState{WaitingForData, WaitingForACK};
enum HostState A_State; // will keep track of which state the sender is in
int A_NextPacketNumber; // will keep track of what packet number to send next i.e. 0 or 1
int A_NextACKNumber; // will keep track of what ACK number to expect next from B
int A_LastSentPacketNumber; 
struct pkt A_LastSentPacket; // will keep track of the last sent packet, in case of retransmission

/*** GLOBAL VARIABLES FOR B ***/
enum HostState B_State;
int B_NextPacketNumber; // will keep track of what packer number to receive next from the sender
int B_NextACKNumber; // will keep track of what ACK number to expect next from A
int B_LastACKedPacketNumber; // will keep track of what was the pktnum of the last acked packet to avoid duplicates
int B_LastDataCheckSum; 
int B_NumMssgsACKED; // keep tracks of how many messages have been ACKed
char B_allMessagesReceived[100][20];
int B_numMessagesReceived=0;

/********* MY OWN HELPER METHODS START *********/
void printPkt(packet)
struct pkt packet;
{
  int i;
  printf("\tpacket.seqnum = %d\n",packet.seqnum);
  printf("\tpacket.acknum = %d\n",packet.acknum);
  printf("\tpacket.checksum = %d\n",packet.checksum);
  printf("\tpacket.payload = ");
  for (i = 0; i<20;i++)
    printf("%c",packet.payload[i]);
  printf("\n");
}

void printData(data)
char* data;
{
  int i;
  for (i = 0; i<20; i++) 
    printf("%c",data[i]);
  printf("\n");
}

// function to calculate the checksum of the packet
int calculateCheckSum(seqnum,acknum,data)
int seqnum;
int acknum;
char* data;
{
  int dataSum = 0;
  int i, e;
  int sumSeqAck = seqnum+acknum;
  for (i = 0; i<20; i++) {
    e = data[i];
    dataSum = dataSum + e;
  }
  // printf("The dataSum is %d and sumSeqAck is %d and the checksum is %d\n",dataSum,sumSeqAck,(dataSum+sumSeqAck));
  return sumSeqAck+dataSum;
} 

int calculateDataCheckSum(data)
char* data;
{
  int dataSum = 0;
  int i, e;
  for (i = 0; i<20; i++) {
    e = data[i];
    dataSum = dataSum + e;
  }
  return dataSum;
}

// function for A to make a packet. will use A's global variabls and functions
struct pkt A_makePacket(message)
struct msg message;
{
  struct pkt newPkt;
  int i;
  newPkt.seqnum = A_NextPacketNumber;
  newPkt.acknum = A_NextACKNumber;
  for (i=0; i<20; i++)  
    newPkt.payload[i] = message.data[i];
  newPkt.checksum = calculateCheckSum(newPkt.seqnum,newPkt.acknum,newPkt.payload);
  return newPkt;
}

// will be used mainly to make an ACK message to send back when it received the packet properly
struct pkt B_make_ACK_Packet(packet)
struct pkt packet;
{
  struct pkt newPkt;
  int i;
  newPkt.seqnum = packet.seqnum;
  newPkt.acknum = packet.seqnum;
  // Writing in ACK in the payload
  char* ACKStr = "ACK";
  for (i=0;i<3;i++)
    newPkt.payload[i] = ACKStr[i];
  // remaining 17 chars can be the same as the old payload
  for (i=3;i<20;i++)
    newPkt.payload[i] = packet.payload[i];
  newPkt.checksum = calculateCheckSum(newPkt.seqnum,newPkt.acknum,newPkt.payload);
  return newPkt;
}

struct pkt B_make_NACK_Packet(packet)
struct pkt packet;
{
  struct pkt newPkt;
  int i;
  newPkt.seqnum = packet.seqnum;
  newPkt.acknum = packet.seqnum; //this is a NACK for this packet number
  char* NACKStr = "NACK";
  for (i=0;i<4;i++)
    newPkt.payload[i] = NACKStr[i];
  for (i=4;i<20;i++)
    newPkt.payload[i] = packet.payload[i];
  newPkt.checksum = calculateCheckSum(newPkt.seqnum,newPkt.acknum,newPkt.payload);
  return newPkt;
}

// function to check if a packet is corrupt or not based on the checksum in that packet
bool isCorrupt(packet)
struct pkt packet;
{
  int checksum = calculateCheckSum(packet.seqnum,packet.acknum,packet.payload);
  return checksum != packet.checksum;
}

bool B_hasCorrectSeqNum(packet)
struct pkt packet;
{
  return packet.seqnum == B_NextACKNumber;
}

bool A_hasCorrectAckNum(packet) 
struct pkt packet;
{
  return packet.acknum == A_LastSentPacketNumber;
}

bool isACK(packet)
struct pkt packet;
{
  int i;
  char* ACKStr = "ACK";
  for (i=0;i<3;i++)
    if (packet.payload[i] != ACKStr[i])
      return false;
  return true;
}

bool isNACK(packet)
struct pkt packet;
{
  int i; char* NACKStr = "NACK";
  for (i=0;i<4;i++)
    if (packet.payload[i] != NACKStr[i])
      return false;
  return true;
}

void B_printAllDataReceived()
{
  int i,j;
  for (i=0;i<B_numMessagesReceived;i++) {
    for (j=0;j<20;j++)
      printf("%c",B_allMessagesReceived[i][j]);
    printf("\n");
  }
}

/********* MY OWN HELPER METHODS END *********/

/* called from layer 5, passed the data to be sent to other side */
A_output(message)
struct msg message;
{  
  if (A_State == WaitingForData) {
    struct pkt sndpkt = A_makePacket(message);

    printf("[EVENT: Packet Sent] Following packet being passed to layer3 from Host A:\n");
    printPkt(sndpkt);

    tolayer3(0,sndpkt);
    A_LastSentPacket = sndpkt;
    A_LastSentPacketNumber = sndpkt.seqnum;
    starttimer(0,TIMER_INCREMENT);
    A_State = WaitingForACK;
  } else if (A_State == WaitingForACK) { // if there is an an outgoing packet already in the link, do not accept new data
    return;
  }
}

B_output(message)  /* need be completed only for extra credit */
  struct msg message;
{

}

/* called from layer 3, when a packet arrives for layer 4 */
A_input(packet)
  struct pkt packet;
{
  // if the incoming packet is corrupt, do nothing
  if (isCorrupt(packet)) {
    printf("[EVENT: Corruption] Host A received a corrupted packet from Host B. \n");
    return;
  }

  // if the incoming packet is a NACK, retransmit the last set packet and do not change the next acknum and packetnum
  if (isNACK(packet)) {
    printf("[EVENT: NACK] Host A received a NACK from Host B. Retransmitting the last unacked packet. \n");
    stoptimer(0);
    tolayer3(0,A_LastSentPacket);
    starttimer(0,TIMER_INCREMENT);
    return;
  }

  if (isACK(packet)) {
    // if the incoming packet is an ACK but ACKs a packet the sender is not waiting for, do nothing
    bool packetHasCorrectAckNum = A_hasCorrectAckNum(packet);
    if (!packetHasCorrectAckNum) {
      printf("[EVENT: Wrong ACK] Host A received an ACK for packet %d but is waiting for an ACK for packet %d.\n",packet.acknum,A_LastSentPacketNumber);
      return;
    }
    printf("[EVENT: ACK] Host A received an ACK for packet %d.\n",packet.acknum);

    // if the incoming packet is an ACK and matches the next expected ACK number for this host, stop the timer,change the state, next acknum and next packetnum
    stoptimer(0);
    A_State = WaitingForData;
    if (A_NextACKNumber==0)
      A_NextACKNumber = 1;
    else
      A_NextACKNumber = 0;

    if (A_NextPacketNumber == 0)
      A_NextPacketNumber = 1;
    else 
      A_NextPacketNumber = 0;
    return;
  }
}

/* called when A's timer goes off */
A_timerinterrupt()
{
  // if the state of the sender is still in waiting for ACK, retransmit the last sent ACK and restart timer
  // stoptimer(0);
  if (A_State == WaitingForACK) {
    printf("[EVENT: Timeout] Host A timer timedout for packet %d carrying data ",A_LastSentPacketNumber);
    printData(A_LastSentPacket.payload);
    printf(" Retransmitting this packet now.\n");
    tolayer3(0,A_LastSentPacket);
    starttimer(0,TIMER_INCREMENT);
  }
}

/* the following routine will be called once (only) before any other */
/* entity A routines are called. You can use it to do any initialization */
A_init()
{
  // initially, the sender (A) is in waiting for new data and is waiting for data0 and the ACK it is not expecting any ACK
  A_State = WaitingForData;
  A_NextPacketNumber = 0;
  A_NextACKNumber = 0;
  A_LastSentPacketNumber = -1;
}

/* Note that with simplex transfer from a-to-B, there is no B_output() */

/* called from layer 3, when a packet arrives for layer 4 at B*/
B_input(packet)
  struct pkt packet;
{
  // make sure that the receiver was indeed in the waiting state
  if (B_State == WaitingForACK) {
    // check if the received packet is corrupt
    bool packetIsCorrupt = isCorrupt(packet);
    // if the packet received is corrupt, send a NACK
    if (packetIsCorrupt) {
      printf("[EVENT: Corruption] Host B received a corrupted packet from Host A. Sending a NACK. \n");
      struct pkt nackpkt = B_make_NACK_Packet(packet);
      tolayer3(1,nackpkt);
      return;
    }

    // if the packet is not corrupt but the seqnum of this packet is the same as the last ACKed pkt => duplicate => just send ack for this
    if (packet.seqnum == B_LastACKedPacketNumber) {
      printf("[EVENT: Duplicate Packet] Host B received a duplicate packet with seqnum %d carrying data ",packet.seqnum);
      printData(packet.payload);
      printf(" Sending an ACK for this packet to Host A.\n");

      struct pkt ackpkt = B_make_ACK_Packet(packet);
      tolayer3(1,ackpkt);
      // do not hand the data to the upper layer
      return;
    }
    // if the seqnum of this packet is different => new data. pass it above and send ack for this data
    tolayer5(1,packet.payload);
    strncpy(B_allMessagesReceived[B_numMessagesReceived++], packet.payload, 20);
    printf("[EVENT: Data Delivery] Delivered the following data to layer5 of Host B: ");
    printData(packet.payload);
    struct pkt ackpkt = B_make_ACK_Packet(packet);
    B_NumMssgsACKED++;
    printf("[EVENT: ACK Send] Sending an ACK for this data to Host A. Number of messages ACKed would now be %d.\n",B_NumMssgsACKED);
    tolayer3(1,ackpkt);
    B_LastACKedPacketNumber = packet.seqnum;
    if (B_NumMssgsACKED == 10) {
      printf("\n\n-----  NUMBER OF MESSAGES CORRECTLY ACKed AT THE RECEIVER HAS REACHED %d -------- \n",B_NumMssgsACKED);
      printf("The data received by B is:\n");
      B_printAllDataReceived();
    }
    return;
  }
}

/* called when B's timer goes off */
B_timerinterrupt()
{
  // printf("[B_timerinterrupt()] Hi. I was called!\n");
}

/* the following rouytine will be called once (only) before any other */
/* entity B routines are called. You can use it to do any initialization */
B_init()
{
  B_State = WaitingForACK;
  B_NextACKNumber = 0; //initially the receiver is waiting for ack number 0
  B_LastACKedPacketNumber = -1; // initially B has not ACKed anything
  B_LastDataCheckSum = -1;
}


/*****************************************************************
***************** NETWORK EMULATION CODE STARTS BELOW ***********
The code below emulates the layer 3 and below network environment:
  - emulates the tranmission and delivery (possibly with bit-level corruption
    and packet loss) of packets across the layer 3/4 interface
  - handles the starting/stopping of a timer, and generates timer
    interrupts (resulting in calling students timer handler).
  - generates message to be sent (passed from later 5 to 4)

THERE IS NOT REASON THAT ANY STUDENT SHOULD HAVE TO READ OR UNDERSTAND
THE CODE BELOW.  YOU SHOLD NOT TOUCH, OR REFERENCE (in your code) ANY
OF THE DATA STRUCTURES BELOW.  If you're interested in how I designed
the emulator, you're welcome to look at the code - but again, you should have
to, and you defeinitely should not have to modify
******************************************************************/

struct event {
   float evtime;           /* event time */
   int evtype;             /* event type code */
   int eventity;           /* entity where event occurs */
   struct pkt *pktptr;     /* ptr to packet (if any) assoc w/ this event */
   struct event *prev;
   struct event *next;
 };
struct event *evlist = NULL;   /* the event list */

/* possible events: */
#define  TIMER_INTERRUPT 0  
#define  FROM_LAYER5     1
#define  FROM_LAYER3     2

#define  OFF             0
#define  ON              1
#define   A    0
#define   B    1



int TRACE = 1;             /* for my debugging */
int nsim = 0;              /* number of messages from 5 to 4 so far */ 
int nsimmax = 0;           /* number of msgs to generate, then stop */
float time = 0.000;
float lossprob;            /* probability that a packet is dropped  */
float corruptprob;         /* probability that one bit is packet is flipped */
float lambda;              /* arrival rate of messages from layer 5 */   
int   ntolayer3;           /* number sent into layer 3 */
int   nlost;               /* number lost in media */
int ncorrupt;              /* number corrupted by media*/

main()
{
   struct event *eventptr;
   struct msg  msg2give;
   struct pkt  pkt2give;
   
   int i,j;
   char c; 
  
   init();
   A_init();
   B_init();
   
   while (1) {
        eventptr = evlist;            /* get next event to simulate */
        if (eventptr==NULL)
           goto terminate;
        evlist = evlist->next;        /* remove this event from event list */
        if (evlist!=NULL)
           evlist->prev=NULL;
        if (TRACE>=2) {
           printf("\nEVENT time: %f,",eventptr->evtime);
           printf("  type: %d",eventptr->evtype);
           if (eventptr->evtype==0)
	       printf(", timerinterrupt  ");
             else if (eventptr->evtype==1)
               printf(", fromlayer5 ");
             else
	     printf(", fromlayer3 ");
           printf(" entity: %d\n",eventptr->eventity);
           }
        time = eventptr->evtime;        /* update time to next event time */
        if (nsim==nsimmax)
	  break;                        /* all done with simulation */
        if (eventptr->evtype == FROM_LAYER5 ) {
            generate_next_arrival();   /* set up future arrival */
            /* fill in msg to give with string of same letter */    
            j = nsim % 26;
             for (i=0; i<20; i++)  
               msg2give.data[i] = 97 + j;
            if (TRACE>2) {
               printf("          MAINLOOP: data given to student: ");
                 for (i=0; i<20; i++) 
                  printf("%c", msg2give.data[i]);
               printf("\n");
	     }
            nsim++;
            if (eventptr->eventity == A) 
               A_output(msg2give);  
             else
               B_output(msg2give);  
            }
          else if (eventptr->evtype ==  FROM_LAYER3) {
            pkt2give.seqnum = eventptr->pktptr->seqnum;
            pkt2give.acknum = eventptr->pktptr->acknum;
            pkt2give.checksum = eventptr->pktptr->checksum;
            for (i=0; i<20; i++)  
                pkt2give.payload[i] = eventptr->pktptr->payload[i];
	    if (eventptr->eventity ==A)      /* deliver packet by calling */
   	       A_input(pkt2give);            /* appropriate entity */
            else
   	       B_input(pkt2give);
	    free(eventptr->pktptr);          /* free the memory for packet */
            }
          else if (eventptr->evtype ==  TIMER_INTERRUPT) {
            if (eventptr->eventity == A) 
	       A_timerinterrupt();
             else
	       B_timerinterrupt();
             }
          else  {
	     printf("INTERNAL PANIC: unknown event type \n");
             }
        free(eventptr);
        }

terminate:
   printf(" Simulator terminated at time %f\n after sending %d msgs from layer5\n",time,nsim);
}



init()                         /* initialize the simulator */
{
  int i;
  float sum, avg;
  float jimsrand();
  
  
   printf("-----  Stop and Wait Network Simulator Version 1.1 -------- \n\n");
   printf("Enter the number of messages to simulate: ");
   scanf("%d",&nsimmax);
   printf("Enter  packet loss probability [enter 0.0 for no loss]:");
   scanf("%f",&lossprob);
   printf("Enter packet corruption probability [0.0 for no corruption]:");
   scanf("%f",&corruptprob);
   printf("Enter average time between messages from sender's layer5 [ > 0.0]:");
   scanf("%f",&lambda);
   printf("Enter TRACE:");
   scanf("%d",&TRACE);

   srand(9999);              /* init random number generator */
   sum = 0.0;                /* test random number generator for students */
   for (i=0; i<1000; i++)
      sum=sum+jimsrand();    /* jimsrand() should be uniform in [0,1] */
   avg = sum/1000.0;
   if (avg < 0.25 || avg > 0.75) {
    printf("It is likely that random number generation on your machine\n" ); 
    printf("is different from what this emulator expects.  Please take\n");
    printf("a look at the routine jimsrand() in the emulator code. Sorry. \n");
    exit(0);
    }

   ntolayer3 = 0;
   nlost = 0;
   ncorrupt = 0;

   time=0.0;                    /* initialize time to 0.0 */
   generate_next_arrival();     /* initialize event list */
}

/****************************************************************************/
/* jimsrand(): return a float in range [0,1].  The routine below is used to */
/* isolate all random number generation in one location.  We assume that the*/
/* system-supplied rand() function return an int in therange [0,mmm]        */
/****************************************************************************/
float jimsrand() 
{
  double mmm = 2147483647;   /* largest int  - MACHINE DEPENDENT!!!!!!!!   */
  float x;                   /* individual students may need to change mmm */ 
  x = rand()/mmm;            /* x should be uniform in [0,1] */
  return(x);
}  

/********************* EVENT HANDLINE ROUTINES *******/
/*  The next set of routines handle the event list   */
/*****************************************************/
 
generate_next_arrival()
{
   double x,log(),ceil();
   struct event *evptr;
    char *malloc();
   float ttime;
   int tempint;

   if (TRACE>2)
       printf("          GENERATE NEXT ARRIVAL: creating new arrival\n");
 
   x = lambda*jimsrand()*2;  /* x is uniform on [0,2*lambda] */
                             /* having mean of lambda        */
   evptr = (struct event *)malloc(sizeof(struct event));
   evptr->evtime =  time + x;
   evptr->evtype =  FROM_LAYER5;
   if (BIDIRECTIONAL && (jimsrand()>0.5) )
      evptr->eventity = B;
    else
      evptr->eventity = A;
   insertevent(evptr);
} 


insertevent(p)
   struct event *p;
{
   struct event *q,*qold;

   if (TRACE>2) {
      printf("            INSERTEVENT: time is %lf\n",time);
      printf("            INSERTEVENT: future time will be %lf\n",p->evtime); 
      }
   q = evlist;     /* q points to header of list in which p struct inserted */
   if (q==NULL) {   /* list is empty */
        evlist=p;
        p->next=NULL;
        p->prev=NULL;
        }
     else {
        for (qold = q; q !=NULL && p->evtime > q->evtime; q=q->next)
              qold=q; 
        if (q==NULL) {   /* end of list */
             qold->next = p;
             p->prev = qold;
             p->next = NULL;
             }
           else if (q==evlist) { /* front of list */
             p->next=evlist;
             p->prev=NULL;
             p->next->prev=p;
             evlist = p;
             }
           else {     /* middle of list */
             p->next=q;
             p->prev=q->prev;
             q->prev->next=p;
             q->prev=p;
             }
         }
}

printevlist()
{
  struct event *q;
  int i;
  printf("--------------\nEvent List Follows:\n");
  for(q = evlist; q!=NULL; q=q->next) {
    printf("Event time: %f, type: %d entity: %d\n",q->evtime,q->evtype,q->eventity);
    }
  printf("--------------\n");
}



/********************** Student-callable ROUTINES ***********************/

/* called by students routine to cancel a previously-started timer */
stoptimer(AorB)
int AorB;  /* A or B is trying to stop timer */
{
 struct event *q,*qold;

 if (TRACE>2)
    printf("          STOP TIMER: stopping timer at %f\n",time);
/* for (q=evlist; q!=NULL && q->next!=NULL; q = q->next)  */
 for (q=evlist; q!=NULL ; q = q->next) 
    if ( (q->evtype==TIMER_INTERRUPT  && q->eventity==AorB) ) { 
       /* remove this event */
       if (q->next==NULL && q->prev==NULL)
             evlist=NULL;         /* remove first and only event on list */
          else if (q->next==NULL) /* end of list - there is one in front */
             q->prev->next = NULL;
          else if (q==evlist) { /* front of list - there must be event after */
             q->next->prev=NULL;
             evlist = q->next;
             }
           else {     /* middle of list */
             q->next->prev = q->prev;
             q->prev->next =  q->next;
             }
       free(q);
       return;
     }
  printf("Warning: unable to cancel your timer. It wasn't running.\n");
}


starttimer(AorB,increment)
int AorB;  /* A or B is trying to stop timer */
float increment;
{
 struct event *q;
 struct event *evptr;
 char *malloc();

 if (TRACE>2)
    printf("          START TIMER: starting timer at %f\n",time);
 /* be nice: check to see if timer is already started, if so, then  warn */
/* for (q=evlist; q!=NULL && q->next!=NULL; q = q->next)  */
   for (q=evlist; q!=NULL ; q = q->next)  
    if ( (q->evtype==TIMER_INTERRUPT  && q->eventity==AorB) ) { 
      printf("Warning: attempt to start a timer that is already started\n");
      return;
      }
 
/* create future event for when timer goes off */
   evptr = (struct event *)malloc(sizeof(struct event));
   evptr->evtime =  time + increment;
   evptr->evtype =  TIMER_INTERRUPT;
   evptr->eventity = AorB;
   insertevent(evptr);
} 


/************************** TOLAYER3 ***************/
tolayer3(AorB,packet)
int AorB;  /* A or B is trying to stop timer */
struct pkt packet;
{
 struct pkt *mypktptr;
 struct event *evptr,*q;
 char *malloc();
 float lastime, x, jimsrand();
 int i;


 ntolayer3++;

 /* simulate losses: */
 if (jimsrand() < lossprob)  {
      nlost++;
      if (TRACE>0)    
	printf("          TOLAYER3: packet being lost\n");
      return;
    }  

/* make a copy of the packet student just gave me since he/she may decide */
/* to do something with the packet after we return back to him/her */ 
 mypktptr = (struct pkt *)malloc(sizeof(struct pkt));
 mypktptr->seqnum = packet.seqnum;
 mypktptr->acknum = packet.acknum;
 mypktptr->checksum = packet.checksum;
 for (i=0; i<20; i++)
    mypktptr->payload[i] = packet.payload[i];
 if (TRACE>2)  {
   printf("          TOLAYER3: seq: %d, ack %d, check: %d ", mypktptr->seqnum,
	  mypktptr->acknum,  mypktptr->checksum);
    for (i=0; i<20; i++)
        printf("%c",mypktptr->payload[i]);
    printf("\n");
   }

/* create future event for arrival of packet at the other side */
  evptr = (struct event *)malloc(sizeof(struct event));
  evptr->evtype =  FROM_LAYER3;   /* packet will pop out from layer3 */
  evptr->eventity = (AorB+1) % 2; /* event occurs at other entity */
  evptr->pktptr = mypktptr;       /* save ptr to my copy of packet */
/* finally, compute the arrival time of packet at the other end.
   medium can not reorder, so make sure packet arrives between 1 and 10
   time units after the latest arrival time of packets
   currently in the medium on their way to the destination */
 lastime = time;
/* for (q=evlist; q!=NULL && q->next!=NULL; q = q->next) */
 for (q=evlist; q!=NULL ; q = q->next) 
    if ( (q->evtype==FROM_LAYER3  && q->eventity==evptr->eventity) ) 
      lastime = q->evtime;
 evptr->evtime =  lastime + 1 + 9*jimsrand();
 


 /* simulate corruption: */
 if (jimsrand() < corruptprob)  {
    ncorrupt++;
    if ( (x = jimsrand()) < .75)
       mypktptr->payload[0]='Z';   /* corrupt payload */
      else if (x < .875)
       mypktptr->seqnum = 999999;
      else
       mypktptr->acknum = 999999;
    if (TRACE>0)    
	printf("          TOLAYER3: packet being corrupted\n");
    }  

  if (TRACE>2)  
     printf("          TOLAYER3: scheduling arrival on other side\n");
  insertevent(evptr);
} 

tolayer5(AorB,datasent)
  int AorB;
  char datasent[20];
{
  int i;  
  if (TRACE>2) {
     printf("          TOLAYER5: data received: ");
     for (i=0; i<20; i++)  
        printf("%c",datasent[i]);
     printf("\n");
   }
  
}
