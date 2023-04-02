/* Project #2; Nagarur, Sai Sanjay*/
#include "csim.h"
#include <stdio.h>

#define SIMTIME 1000.0
#define NUM_NODES 5
#define TIME_OUT 5.0
#define T_DELAY 0.5
#define TRANS_TIME 0.1
#define REQUEST 1L
#define REPLY 2L
#define IATM 5.0	/*mean of inter-arrival time distribution */

typedef struct msg *msg_t;

struct msg{
	long type;
	long from;
	long to;
	TIME start_time;
	msg_t link; 
};
msg_t msg_queue;
struct nde{
	FACILITY cpu;
	MBOX input;
};
struct nde node[NUM_NODES];
FACILITY network[NUM_NODES][NUM_NODES];
TABLE resp_tm;
FILE *fp;

//count of the number of messages sent by each node
int messages_sent[NUM_NODES];

//count of the number of acknowledgements received by each node
int ack_recevied[NUM_NODES];

//stores the value of packet loss probability
double plp=0.5;

void init();
void myreport();
void proc();
void send_message();
void form_reply();
void decode_msg();
void return_msg();
msg_t new_msg();
double getRandom();
void sim(){
	int i;
	printf("-----------------------Program started-----------------------\n");
	create("sim");
	init();
	hold(SIMTIME);
	myreport();
}

void init(){
	long i,j;
	char str[24];
	
	fp=fopen("opfile.out","w");
	set_output_file(fp);
	max_facilities(NUM_NODES*NUM_NODES+NUM_NODES);
	max_servers(NUM_NODES*NUM_NODES+NUM_NODES);
	max_mailboxes(NUM_NODES);
	max_events(2*NUM_NODES*NUM_NODES);
	resp_tm = table("msg rsp tm");
	msg_queue=NIL;
	

	for(i=0;i<NUM_NODES;i++){
		printf(str, "cpu. %d", i);
		node[i].cpu = facility(str);
		printf(str, "input. %d", i);
		node[i].input = mailbox(str);
		ack_recevied[i]=0;
		messages_sent[i]=0;
	}
	for(i=0;i<NUM_NODES;i++){
		for(j=0;j<NUM_NODES;j++){
			network[i][j] = facility(str);
		}
	}
	for(i=0;i<NUM_NODES;i++){
		proc(i);
	}
}
void proc(n)
long n;
{
	msg_t m;
	long s, t;
	
	create("proc");
	
	while(clock<SIMTIME){
		s = timed_receive(node[n].input, &m, TIME_OUT);
		switch(s){
			case TIMED_OUT: 
				m = new_msg(n);
				#ifdef TRACE
					decode_msg("timed out, send new msg", m, n);
					//printf("node.%ld sends a Hello_Ack to node.%ld at",m->from, m->to);
				#endif
					send_msg(m);
					break;
			case EVENT_OCCURRED:
				#ifdef TRACE
					decode_msg("received msg",m ,n);
					//printf("node.%ld sends a Hello to node.%ld at",m->from, m->to);

				#endif
					t=m->type;
					switch(t){
						case REQUEST:
							form_reply(m);		
							#ifdef TRACE
								decode_msg("return request",m,n);
							#endif
								// we maintain an IATM of 0.5 before a send message operation is taken place.
								hold(expntl(IATM));	
								send_msg(m);
								break;
						case REPLY:
							#ifdef TRACE
								decode_msg("receive reply", m, n);
							#endif
								record(clock- m->start_time, resp_tm);
								return_msg(m);
								break;
						default:
							decode_msg("***unexpected type",m,n);
							break;						
					}
				break;
		}
	}
}

void send_msg(m)
msg_t m;
{
	//cts - clear stores the randomly generated probability
	double cts = uniform(0,1);
	if(cts<plp){
		//clear to send if and only if its greater than the packet loss probability chosen
		printf("Stopping Hello transmission as plp not met\n");
		return;
	}
	printf("node.%ld sends a Hello to node.%ld at %ld time units\n",m->from, m->to, clock);
	long from, to;
	
	from=m->from;
	//update the number of messages sent by a node
	messages_sent[from]=messages_sent[from]+1;
	to=m->to;
	use(node[from].cpu, T_DELAY);
	reserve(network[from][to]);
	hold(TRANS_TIME);
	send(node[to].input, m);
	release(network[from][to]);
}

msg_t new_msg(from)
long from;
{
	msg_t m;
	long i;
	
	if(msg_queue==NIL){
		m=(msg_t)do_malloc(sizeof(struct msg));
	}	
	else{
		m=msg_queue;
		msg_queue=msg_queue->link;
	}
	do{
		i = random(01, NUM_NODES-1);
	}while(i==from);
	m->to=i;
	m->from=from;
	m->type=REQUEST;
	m->start_time=clock;
	return(m);
}

void return_msg(m)
msg_t m;
{
	//cts - clear stores the randomly generated probability
	double cts = uniform(0,1);
	if(cts<plp){
		//clear to send if and only if its greater than the packet loss probability chosen
		printf("Stopping Hello_Ack transmission as plp not met\n");
		return;
	}
	printf("node.%ld sends a HelloAck to node.%ld at %4ld time units\n",m->from, m->to, clock);
	m->link = msg_queue;
	msg_queue =m;
	//update the count of ack recieved at the reciever end
	ack_recevied[(int)m->to]=ack_recevied[(int)m->to]+1;
}

void form_reply(m)
msg_t m;
{
	long from, to;
	from = m-> from;
	to = m->to;
	m->from=to;
	m->to=from;
	m->type=REPLY;
}

void decode_msg(str, m, n)
char *str;msg_t m; long n;
{
	//printf("This is decode message fn");
	printf("%f node %2ld: %s - msg: type = %s, from = %ld, to = %ld\n", clock,n, str, (m->type==REQUEST)?"req":"rep",m->from, m->to);
}

void myreport(){
	printf("The simulation ended at %ld simtime\nThe number of messages and acknowledges received by each node are:\n", SIMTIME);
	int i;
	int sucessful=0, failure=0;
	for(i=0;i<NUM_NODES;i++){
		printf("Node.%d has sent %d messages and got %d acknowledges\n",i,messages_sent[i],ack_recevied[i]); 
		int diff = messages_sent[i]-ack_recevied[i];
		if(diff>0){
			//we count the number of successful transmissions with the messages who have got an acknowledgement and the difference belongs to failed category
			sucessful+=ack_recevied[i];
			failure+=diff;
		}else{
			// when the difference is zero all messages have their corresponding acknowledgement
			sucessful+=messages_sent[i];
		}
	}
	int total = sucessful+failure;
	printf("\n\nNumber of succesful Transmissions are %d and average = %f\n",sucessful, (sucessful/(1.0*total)));
	printf("Number of failed Transmissions are %d and average = %f\n",failure, (failure/(1.0*total)));
	printf("Packet loss probability chosen was %f\n",plp);
}