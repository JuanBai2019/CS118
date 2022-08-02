#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <fcntl.h>
#include <netdb.h> 

// =====================================

#define RTO 500000 /* timeout in microseconds */
#define HDR_SIZE 12 /* header size*/
#define PKT_SIZE 524 /* total packet size */
#define PAYLOAD_SIZE 512 /* PKT_SIZE - HDR_SIZE */
#define WND_SIZE 10 /* window size*/
#define MAX_SEQN 25601 /* number of sequence numbers [0-25600] */
#define FIN_WAIT 2 /* seconds to wait after receiving FIN*/

// Packet Structure: Described in Section 2.1.1 of the spec. DO NOT CHANGE!
struct packet {
    unsigned short seqnum;
    unsigned short acknum;
    char syn;
    char fin;
    char ack;
    char dupack;
    unsigned int length;
    char payload[PAYLOAD_SIZE];
};

// Printing Functions: Call them on receiving/sending/packet timeout according
// Section 2.6 of the spec. The content is already conformant with the spec,
// no need to change. Only call them at correct times.
void printRecv(struct packet* pkt) {
    printf("RECV %d %d%s%s%s\n", pkt->seqnum, pkt->acknum, pkt->syn ? " SYN": "", pkt->fin ? " FIN": "", (pkt->ack || pkt->dupack) ? " ACK": "");
}

void printSend(struct packet* pkt, int resend) {
    if (resend)
        printf("RESEND %d %d%s%s%s\n", pkt->seqnum, pkt->acknum, pkt->syn ? " SYN": "", pkt->fin ? " FIN": "", pkt->ack ? " ACK": "");
    else
        printf("SEND %d %d%s%s%s%s\n", pkt->seqnum, pkt->acknum, pkt->syn ? " SYN": "", pkt->fin ? " FIN": "", pkt->ack ? " ACK": "", pkt->dupack ? " DUP-ACK": "");
}

void printTimeout(struct packet* pkt) {
    printf("TIMEOUT %d\n", pkt->seqnum);
}

// Building a packet by filling the header and contents.
// This function is provided to you and you can use it directly
void buildPkt(struct packet* pkt, unsigned short seqnum, unsigned short acknum, char syn, char fin, char ack, char dupack, unsigned int length, const char* payload) {
    pkt->seqnum = seqnum;
    pkt->acknum = acknum;
    pkt->syn = syn;
    pkt->fin = fin;
    pkt->ack = ack;
    pkt->dupack = dupack;
    pkt->length = length;
    memcpy(pkt->payload, payload, length);
}

// =====================================

double setTimer() {
    struct timeval e;
    gettimeofday(&e, NULL);
    return (double) e.tv_sec + (double) e.tv_usec/1000000 + (double) RTO/1000000;
}

double setFinTimer() {
    struct timeval e;
    gettimeofday(&e, NULL);
    return (double) e.tv_sec + (double) e.tv_usec/1000000 + (double) FIN_WAIT;
}

int isTimeout(double end) {
    struct timeval s;
    gettimeofday(&s, NULL);
    double start = (double) s.tv_sec + (double) s.tv_usec/1000000;
    return ((end - start) < 0.0);
}

// =====================================

int main (int argc, char *argv[])
{
    printf("*********** START ************\n");
    if (argc != 4) {
        perror("ERROR: incorrect number of arguments\n");
        exit(1);
    }

    struct in_addr servIP;
    if (inet_aton(argv[1], &servIP) == 0) {
        struct hostent* host_entry; 
        host_entry = gethostbyname(argv[1]); 
        if (host_entry == NULL) {
            perror("ERROR: IP address not in standard dot notation\n");
            exit(1);
        }
        servIP = *((struct in_addr*) host_entry->h_addr_list[0]);
    }

    unsigned int servPort = atoi(argv[2]);

    FILE* fp = fopen(argv[3], "r");
    if (fp == NULL) {
        perror("ERROR: File not found\n");
        exit(1);
    }

    // =====================================
    // Socket Setup

    int sockfd;
    struct sockaddr_in servaddr;
    
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);

    servaddr.sin_family = AF_INET;
    servaddr.sin_addr = servIP;
    servaddr.sin_port = htons(servPort);
    memset(servaddr.sin_zero, '\0', sizeof(servaddr.sin_zero));

    int servaddrlen = sizeof(servaddr);

    // NOTE: We set the socket as non-blocking so that we can poll it until
    //       timeout instead of getting stuck. This way is not particularly
    //       efficient in real programs but considered acceptable in this
    //       project.
    //       Optionally, you could also consider adding a timeout to the socket
    //       using setsockopt with SO_RCVTIMEO instead.
    fcntl(sockfd, F_SETFL, O_NONBLOCK);

    // =====================================
    // Establish Connection: This procedure is provided to you directly and is
    // already working.
    // Note: The third step (ACK) in three way handshake is sent along with the
    // first piece of along file data thus is further below

    struct packet synpkt, synackpkt;

    unsigned short seqNum = rand() % MAX_SEQN;
    //unsigned short seqNum = 25000;
    buildPkt(&synpkt, seqNum, 0, 1, 0, 0, 0, 0, NULL);

    printSend(&synpkt, 0);
    //send a syn packet to server to note that client wants to establish a connection
    sendto(sockfd, &synpkt, PKT_SIZE, 0, (struct sockaddr*) &servaddr, servaddrlen);
    double timer = setTimer();
    int n;

    while (1) {
        while (1) {
            //try to receive a synack packet from server
            n = recvfrom(sockfd, &synackpkt, PKT_SIZE, 0, (struct sockaddr *) &servaddr, (socklen_t *) &servaddrlen);

            if (n > 0)
                break;
            else if (isTimeout(timer)) {
                printTimeout(&synpkt);
                printSend(&synpkt, 1);
                //RESEND SYN packet if the last syn packet is timed out.
                sendto(sockfd, &synpkt, PKT_SIZE, 0, (struct sockaddr*) &servaddr, servaddrlen);
                timer = setTimer();
            }
        }

        printRecv(&synackpkt);
        if ((synackpkt.ack || synackpkt.dupack) && synackpkt.syn && synackpkt.acknum == (seqNum + 1) % MAX_SEQN) {
            printf("Exit first part 1, synackpkt.acknum:%d, seqNum:%d \n", synackpkt.acknum   ,seqNum);
            seqNum = synackpkt.acknum;
            break;
        }
        //ADDED CODE TO HANDLE DATA LOSS, when synack/ack from server is lost
        if ((synackpkt.ack || synackpkt.dupack) && synackpkt.acknum == (seqNum + 1 + synackpkt.length)%MAX_SEQN )
        {
            
            printf("Exit second part 2, synack lost, but exit upon receiving next ack,synackpkt.acknum:%d, seqNum:%d \n", synackpkt.acknum   ,seqNum);
            seqNum = synackpkt.acknum;
            break;
        }
    }

    // =====================================
    // FILE READING VARIABLES
    
    char buf[PAYLOAD_SIZE]; //PAYLOAD_SIZE = 512
    size_t m;

    // =====================================
    // CIRCULAR BUFFER VARIABLES

    struct packet ackpkt;
    struct packet pkts[WND_SIZE]; //WND_SIZE = 10
    int s = 0; //starting window
    int e = 0; // end/current window
    int full = 0;
    
    int wait_to_read = 0; //number of packets that still need to be processed.

    // =====================================
    // Send First Packet (ACK containing payload)

    m = fread(buf, 1, PAYLOAD_SIZE, fp); //how many bytes read

    buildPkt(&pkts[0], seqNum, (synackpkt.seqnum + 1) % MAX_SEQN, 0, 0, 1, 0, m, buf);
    printSend(&pkts[0], 0);
    sendto(sockfd, &pkts[0], PKT_SIZE, 0, (struct sockaddr*) &servaddr, servaddrlen);
    timer = setTimer();
//ADDED 1 LINE OF CODE HERE
    int timer_switch = 1;
    buildPkt(&pkts[0], seqNum, (synackpkt.seqnum + 1) % MAX_SEQN, 0, 0, 0, 1, m, buf);

    e = 1;
    wait_to_read ++;
    // =====================================
    // *** TODO: Implement the rest of reliable transfer in the client ***
    // Implement GBN for basic requirement or Selective Repeat to receive bonus

    // Note: the following code is not the complete logic. It only sends a
    //       single data packet, and then tears down the connection without
    //       handling data loss.
    //       Only for demo purpose. DO NOT USE IT in your final submission
    printf("----------------------------------------------------------------\n");
    int prev_pkt_num = -1;
    int recv_counter = 0; //increase by 1 when server receives a new ack

    
    
    while (1) {
  
        prev_pkt_num = (e==0 && s>=e)?  9 : e - 1; //calculate index for previous packet
        n = recvfrom(sockfd, &ackpkt, PKT_SIZE, 0, (struct sockaddr *) &servaddr, (socklen_t *) &servaddrlen);
        //if client receives something, check if the message is the next expected ACK/DUPACK
        if(n > 0){
            printRecv(&ackpkt);
            //if receive a new ack, due to the potential of data loss(loss ack from server)
            //now the condition is simpler
            if (ackpkt.ack) {
 
                //algorithm below calculates the "offset"
                //offset is how many packets is skipped due to data ack loss.
                //And the window should move up by this amount of offset
                int offset= -1;
                int temp_result = -1;
                int floor = -1;
                int ceiling = -1;
                
                if(ackpkt.acknum  ==  pkts[recv_counter].seqnum){
                    offset = 0;
                }
                else if(ackpkt.acknum < pkts[recv_counter].seqnum){
                    //case when acknum is small because it just overflowed
                    //check if ack + 25601 < seq + 512*9
                    if(ackpkt.acknum + 25601 < pkts[recv_counter].seqnum + 512*9){
                        temp_result = (ackpkt.acknum + 25601) - pkts[recv_counter].seqnum;
                        // numerator is divisble by 512
                        floor   = temp_result / 512;
                        //not divisible by 512, therefore add 1, there is 1 more packet.
                        ceiling = 1 + temp_result / 512;
                        offset =  (temp_result % 512 == 0) ? floor : ceiling;
                    }
                    else{ //case when it is a normal less than
                        offset = 0;
                    }
                }
                else if(ackpkt.acknum > pkts[recv_counter].seqnum){
                    //maximum valid offset is 9?
                    if(ackpkt.acknum > pkts[recv_counter].seqnum + 512*9){
                        //case when "pkts[recv_counter].seqnum" overflows
                        offset = 0;
                    }
                    else{ //case when it is a normal greater than
                        temp_result = ackpkt.acknum - pkts[recv_counter].seqnum;
                        floor   = temp_result / 512;
                        //not divisible by 512, therefore add 1, there is 1 more packet.
                        ceiling = 1 + temp_result / 512;
                        offset =  (temp_result % 512 == 0) ? floor : ceiling;
                    }
                }
                
                s = (s + offset) % 10;
                wait_to_read -= offset;
                
                full = (offset == 0)? 1 : 0 ;
                recv_counter = (recv_counter + offset) % 10;
            
                //timer part
                if(wait_to_read == 0) //everything received on time, so stop timer
                {
                    timer_switch = 0;
                }
                else
                {
                    timer = setTimer();
                    timer_switch = 1;
                }
//                printf("ackpkt.acknum: %d, pkts[recv_counter].seqnum: %d,  offset is %d, s is %d,  e is %d ", ackpkt.acknum,pkts[recv_counter].seqnum, offset,s,e);
//                printf("     time: %d, wait_to_read is %d\n", timer_switch,wait_to_read);
                continue; //go to next iteration, because client send upon server ack for GBN?
            }
        }
        
        //keep sending packets if window is not filled
        //start of the iteration, s=0, e=1, sent 1 packets
        //when s == e, then windows are filled
        //also bytes read must be > 0
        //ADD PKT TO WINDOW, IF window NOT FULL AND there may be(from last iteration, there is things to read) still things to read.
        if(wait_to_read != 10 && m > 0 ){
          //  printf("READ&SEND\n");
            
            m = fread(buf, 1, PAYLOAD_SIZE, fp);
            if(m==0) continue;
                        
            //ack is false, ack number = 0
            buildPkt(&pkts[e], (pkts[prev_pkt_num].seqnum + pkts[prev_pkt_num].length) % MAX_SEQN ,0, 0, 0, 0, 0, m, buf);
            printSend(&pkts[e], 0);
            sendto(sockfd, &pkts[e], PKT_SIZE, 0, (struct sockaddr*) &servaddr, servaddrlen);
//            printf("s is %d, e is %d, wait_to_read is %d \n", s,e,wait_to_read);
            if(timer_switch == 0) //only start timer when it is running.
                timer = setTimer();
            
 //           buildPkt(&pkts[e],pkts[prev_pkt_num].seqnum + pkts[prev_pkt_num].length ,(synackpkt.seqnum + 1) % MAX_SEQN, 0, 0, 0, 0, m, buf);
            
//printf("Prev_pkt:%d,seq:%d,ack:%d\n",prev_pkt_num,pkts[prev_pkt_num].seqnum,pkts[prev_pkt_num].acknum);
 // printf("START: s= %d , e=%d, m is %d ----",s,e,m);
//printf("packet sent:%d,seq:%d,ack:%d\n",e, pkts[e].seqnum, pkts[e].acknum);
            e = (e + 1) % 10;
            wait_to_read += 1;
            //check if the window is full, 1 is full, 0 is not full
            full = (s == e) ? 1:0;
//printf("full is %d, s is %d, e is %d\n",full, s ,e);
        }
     
        
        
        //case when there is a timeout
        if(isTimeout(timer) && timer_switch == 1) {
            printTimeout(&pkts[s]); //there is a timeout at index = s,(least recent unacked packet)
            int resend_index = s;
            //resend from start(s) of the window, to end of window(e)
//            printf("In time out? GONNA RESEND, s = %d, e = %d, e - 1 = %d,full = %d,wait_to_read is %d \n", s,e,prev_pkt_num,full,wait_to_read );
            //prev_pkt_num is basically e - 1
            if(wait_to_read == 10)
            {
                int count = 0; //resend 10 packets
                for( ; count<10; count++, resend_index = (resend_index + 1)% 10 )
                {
                    printSend(&pkts[resend_index], 1);
                    sendto(sockfd, &pkts[resend_index], PKT_SIZE, 0, (struct sockaddr*) &servaddr, servaddrlen);
                }
            }
            else{
                for(resend_index = s; resend_index != e; resend_index = (resend_index + 1)% 10 )
                {
                    printSend(&pkts[resend_index], 1);
                    sendto(sockfd, &pkts[resend_index], PKT_SIZE, 0, (struct sockaddr*) &servaddr, servaddrlen);
                }
            }
            //set timer again
            timer = setTimer();
            timer_switch = 1;
        }

//       printf("server: seq: %d ack: %d  -------    ",ackpkt.seqnum, ackpkt.acknum);
 //      printf("prev_pkt_num is: num:%d, seq:%d, ack:%d   length:%d\n",prev_pkt_num, pkts[prev_pkt_num].seqnum,  pkts[prev_pkt_num].acknum, pkts[prev_pkt_num].length );

        if(m == 0 &&  ackpkt.acknum == (pkts[prev_pkt_num].seqnum + pkts[prev_pkt_num].length) % MAX_SEQN)
        {
 //           printf("DEBUG: SERVER SEND: %d %d --- CLIENT: %d %d\n",ackpkt.seqnum, ackpkt.acknum,(pkts[prev_pkt_num].seqnum + pkts[prev_pkt_num].length)% MAX_SEQN, pkts[prev_pkt_num].acknum) ;
            break;
        }
        
  
        if(ackpkt.fin) //exit when receives a FIN
        {
            break;
        }
    }
    printf("-----------------------------------------------------------------\n");
    
    
    // *** End of your client implementation ***
    fclose(fp);

    // =====================================
    // Connection Teardown: This procedure is provided to you directly and is
    // already working.


    struct packet finpkt, recvpkt;
    buildPkt(&finpkt, ackpkt.acknum, 0, 0, 1, 0, 0, 0, NULL);
    buildPkt(&ackpkt, (ackpkt.acknum + 1) % MAX_SEQN, (ackpkt.seqnum + 1) % MAX_SEQN, 0, 0, 1, 0, 0, NULL);

    printSend(&finpkt, 0);
    
    //send a fin packet to server to denote close a connection
    sendto(sockfd, &finpkt, PKT_SIZE, 0, (struct sockaddr*) &servaddr, servaddrlen);
    timer = setTimer();
    int timerOn = 1;

    double finTimer;
    int finTimerOn = 0;

    while (1) {
        while (1) {
            //try to receive ONLY FIN packets from server
            n = recvfrom(sockfd, &recvpkt, PKT_SIZE, 0, (struct sockaddr *) &servaddr, (socklen_t *) &servaddrlen);

            if (n > 0)
                break;
            if (timerOn && isTimeout(timer)) {
                printTimeout(&finpkt);
                printSend(&finpkt, 1);
                if (finTimerOn)
                    timerOn = 0;
                else
                    sendto(sockfd, &finpkt, PKT_SIZE, 0, (struct sockaddr*) &servaddr, servaddrlen);
                timer = setTimer();
            }
            if (finTimerOn && isTimeout(finTimer)) {
                close(sockfd);
                if (! timerOn)
                {
                    printf("*********** END ************\n\n");
                    exit(0);
                }
            }
        }
        printRecv(&recvpkt);
        if ((recvpkt.ack || recvpkt.dupack) && recvpkt.acknum == (finpkt.seqnum + 1) % MAX_SEQN) {
            //check if received ack/dupack packet from server
            timerOn = 0;
        }
        else if (recvpkt.fin && (recvpkt.seqnum + 1) % MAX_SEQN == ackpkt.acknum) {
            //check if received packet is FIN pkt
            //if yes, client send an ACK packet back to server.
            printSend(&ackpkt, 0);
            sendto(sockfd, &ackpkt, PKT_SIZE, 0, (struct sockaddr*) &servaddr, servaddrlen);
            finTimer = setFinTimer();
            finTimerOn = 1;
            buildPkt(&ackpkt, ackpkt.seqnum, ackpkt.acknum, 0, 0, 0, 1, 0, NULL);
        }
    }
}
