/* 
 * File:   sender_main.cpp
 * Modified Date: 2022.2.28
 * Author: Guanshujie Fu
 * 
 * 2022.2.28: Add definition for class TCPSegment and TCPSender
 * 2022.3.1:  Implement TCPSender::file2segments()
 * 
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/stat.h>
#include <signal.h>
#include <string.h>
#include <sys/time.h>

// Added Library
#include <deque>
#include <netdb.h>
#include <iostream>


// Added Definition
#define DEBUG 1
#define MSS 1024               // byte
#define SEG_HEADRER_SIZE 10  // byte
#define MAX_SEGMENT_SIZE MSS+SEG_HEADRER_SIZE
#define WINDOW_SIZE 5           // set 1 for easy test
#define TIME_OUT_MICRO 20000    // micro second
#define MAGIC_LARGE 1000000000

#define SEND_BUF_SIZE MAX_SEGMENT_SIZE*1
#define RECV_BUF_SIZE MAX_SEGMENT_SIZE*1
#define RTT 1               
using namespace::std;

//static void *send_thread(void *arg);
static void *receive_thread(void *arg);
void diep(string s);
enum CongestStates {SlowStart_, CongestAvoid_, FastRecover_};
enum event_t {NEW_ACK, DUP_ACK, TIME_OUT};
CongestStates CongStat;
event_t tcpevent;

static pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;

/**********************************************************************/
/* ********** Here goes the definitions of TCP Segment Class **********/
/**********************************************************************/

/*
 * TCPSegment Class
 *      fin: special segment to close
 *      id_: Sequence Number (Not implement as typical TCP sequence number, just {0, 1, 2...})
 *      ack_ Acknowledge Number 
 *          it set ack_ as the seqnumber expected as acknowledgement
 *          (ack_ - 1) is the sequence number of the segment that has been received
 *      len_: Segment Data Size
 *      data_: Segment Data
 * 
 */
typedef struct TCPSegment {
    char        sent;
    bool        fin;
    int         id_;
    int         len_;
    char        data[MSS+3];
} TCPSegment_t;



/**********************************************************************/
/* ********** Here goes the definitions of TCP Sender Class ***********/
/**********************************************************************/

/*
 * TCPSender Class (rdt sender)
 * Description: 
 * 
 * 
 */
class TCPSender
{
public:
    TCPSender(const int socket, addrinfo* rcvinfo, FILE* fileptr, unsigned long long int filesize);
    int file2segments();
    int sendSegment(TCPSegment* seg);
    int sendNewSegments();
    void receiveACK();
    void CongestControl();
    void ResendOldSegs();
    int TimeACK();
    void sendClose();
    // Functions for congestion control

    int socket_id;      // socket id of the established socket connection
    volatile int sendBase;          // seq number of the oldest unacked segment. Changed only when ack received. initialize as 0
    int NextSeqNum;                 // seq number of the next sended segment. Changed only when segment sent. initialize as 0
    volatile int lastRCVack;        // the ack_ lastly received. Used to compare with the newly arrived ack_. initialize as -1
    long long int totalBytes;
    long long int fileBytes;        // byte size of the file loaded. Keep updating.

    // reliable data transfer
    struct timeval TimeoutVal;      // Self-explained
    volatile int DupACK_flag;       // Self-explained
    int Resend_flag;
    double cwnd;                // self-updating window size
    double ssthresh;           // threshold value of window size for congestion control


    FILE* filedata;
    addrinfo* rcvaddr;            // info about receiver
    deque<TCPSegment_t> wait2sendSegs;     // segments not send yet. initialize as empty
};

// constructor
TCPSender::TCPSender(const int socket, addrinfo* rcvinfo, FILE* fileptr, unsigned long long int filesize)
{
    socket_id = socket; 
    rcvaddr = rcvinfo;
    filedata = fileptr; totalBytes = fileBytes = (long long int) filesize;
    sendBase = 0; DupACK_flag = Resend_flag = 0; lastRCVack = -1;
    TimeoutVal.tv_sec = 0;
    TimeoutVal.tv_usec = TIME_OUT_MICRO;
    cwnd = WINDOW_SIZE;
    ssthresh = MAGIC_LARGE;     // initialize to a large number at first
    return;
}


/**********************************************************************/
/* ********** Here goes the definitions of member functions ***********/
/**********************************************************************/


/*
 * file2segments() - implemented and tested
 * @param none
 *      This function takes the file stream and 
 *      creat segments to store in a waiting queue
 */

int TCPSender::file2segments()
{
    if (!filedata || fileBytes == 0)  return -1;

    int newSeqid = 0;
    while (fileBytes > 0 && !feof(filedata)) {
        TCPSegment_t newSeg;
        int sendByte = (totalBytes >= MSS) ? MSS : (int)totalBytes;
        sendByte = (fileBytes >= sendByte) ? sendByte : fileBytes;
        
        size_t datalen = fread((char*)newSeg.data, sizeof(char), sendByte, filedata);
        cout << endl;
        newSeg.id_ = newSeqid++;
        newSeg.sent = 0;
        newSeg.len_ = (int) sendByte;
        newSeg.fin =  false;

        fileBytes = fileBytes >= (int) datalen ? fileBytes - (int) datalen : 0;
        wait2sendSegs.push_back(newSeg);
        #if DEBUG
            // cout << "\n\n##### file2segments ####\n";
            // cout <<  "Seq id: " << newSeqid << endl;
            // cout << "Remaining size: " << fileBytes << endl;
            // cout << "Segment data size: " << datalen << endl;
            // cout << "Queue Size: " << wait2sendSegs.size() << endl;
            // for (int i = 0; i < datalen; i++) {
            //     cout << newSeg.data[i];
            // }
        #endif
    } return 0;
}


/*
 * sendSegment()
 *      This function takes a segment and send
 *      to send buffer -> network connection
 */
int TCPSender::sendSegment(TCPSegment_t* seg)
{
    int sendBytes;
    // send segment
    if ((sendBytes = sendto(socket_id, (char*)seg, MAX_SEGMENT_SIZE+3, 0, rcvaddr->ai_addr, rcvaddr->ai_addrlen)) == -1)
        perror("Fail to send packet");
    seg->sent = 1;
    
    return sendBytes-MAX_SEGMENT_SIZE;
}



/*
 * sendNewSegments()
 *      This function takes segments from queue and send
 * 
*/
int TCPSender::sendNewSegments()
{
    for (int i = sendBase; i < sendBase + cwnd && !wait2sendSegs.empty(); i++) {
        // push packet into waiting queues
        if(i < wait2sendSegs.size()) {
            if (wait2sendSegs[i].sent == 0) {
                sendSegment(&(wait2sendSegs[i]));
                #if DEBUG
                    // cout << "### sendNewSegments() ###\n";
                    // cout << "SendBase: " << sendBase << endl;
                    // cout << "Segment ID: " << wait2sendSegs[i].get_id() << endl;
                    // cout << "Queue ID: " << i << endl;
                #endif
            }
        }
    } 
    return 0;
}



/*
 * ResendOldSegs()
 *      This function is called if 3 duplicate ACK is received or Time out.
 *      Fast Retransmit the lost segment
 */
void TCPSender::ResendOldSegs()
{
    #if DEBUG
        cout << "\n\n**** Resend ****\n";
        cout << "Send Base: " << sendBase << endl;
        cout << "Last ACK: " << lastRCVack << endl;
    #endif
    Resend_flag = 1;
    // for (int i = sendBase; i < sendBase + cwnd && !wait2sendSegs.empty(); i++) {
    //     if(i < wait2sendSegs.size()){
    //         wait2sendSegs[i].sent = 0;    // set sent back to 0
    //         if (sendto(socket_id, (char*)&wait2sendSegs[i], MAX_SEGMENT_SIZE, 0, rcvaddr->ai_addr, rcvaddr->ai_addrlen) == -1)
    //             perror("Fail to send packet");
    //         wait2sendSegs[i].sent = 1;
    //     }
    // }
    if (sendto(socket_id, (char*)&wait2sendSegs[sendBase], MAX_SEGMENT_SIZE+3, 0, rcvaddr->ai_addr, rcvaddr->ai_addrlen) == -1)
            perror("Fail to send packet");
    Resend_flag = 0;
}



/*
 * receiveACK()
 *      receive ACK from the server
 * 
 */
void TCPSender::receiveACK()
{
    clock_t s = clock();
    sendNewSegments();

    int dup_ack = 0;
    while(sendBase < wait2sendSegs.back().id_+1) {
        // Wait for ACK
        int acknum = TimeACK();

        // Time Out detected
        if (acknum == -1) {
            dup_ack = 0;
            tcpevent = TIME_OUT;
            ResendOldSegs();
            #if DEBUG
                cout << "\n*** TimeOut ***\n";
                cout << "expect ACK: " << lastRCVack+1 << endl;
            #endif
            continue;
        }

        // // Duplicate ACK detected
        // else if (acknum == lastRCVack) {
        //     dup_ack += 1;
        //     tcpevent = DUP_ACK;

        //     if(dup_ack == 5) {
        //         dup_ack = 0;
        //         ResendOldSegs();
        //         pthread_mutex_lock(&mtx);
        //         DupACK_flag = 1;
        //         pthread_mutex_unlock(&mtx);
        //         #if DEBUG
        //             cout << "\n*** Dup ACK ***\n";
        //             cout << "expect ACK: " << lastRCVack+1 << endl;
        //         #endif
        //     }
        // }


        // ACK receives successfully
        if (acknum >= sendBase) {
            dup_ack = 0;
            sendBase = acknum + 1;
            lastRCVack = acknum;
            tcpevent = NEW_ACK;
            #if DEBUG
                cout << "\n*** ACK Received ***\n";
                cout << "ack: " << acknum << endl;
            #endif
        }


        //Check if TCP should be terminated
        if(fileBytes == 0 && sendBase == wait2sendSegs.back().id_+1) {
            sendClose();
            break;
        }      

        sendNewSegments();

        // Congestion Control
        // switch (CongStat) {

        // case SlowStart_:
        //     if (tcpevent == TIME_OUT) {
        //         ssthresh = cwnd/1.5;
        //         cwnd = WINDOW_SIZE;
        //         dup_ack = 0;
        //         CongStat = SlowStart_;
        //     } else if (DupACK_flag == 1) {
        //         ssthresh = cwnd/1.5;
        //         cwnd = ssthresh + 3;
        //         CongStat = FastRecover_;
        //     } else if (tcpevent == NEW_ACK){
        //         cwnd += 1;
        //         CongStat = SlowStart_;
        //         if (cwnd >= ssthresh) 
        //             CongStat = CongestAvoid_;
        //     }
        //     break;

        // case CongestAvoid_:
        //     if (tcpevent == TIME_OUT) {
        //         ssthresh = cwnd/1.5;
        //         cwnd = WINDOW_SIZE;
        //         dup_ack = 0;
        //         CongStat = SlowStart_;
        //     } else if (DupACK_flag == 1) {
        //         ssthresh = cwnd/1.5;
        //         cwnd = ssthresh + 3;
        //         CongStat = FastRecover_;
        //     } else if (tcpevent == NEW_ACK){
        //         cwnd += (1.0/cwnd);
        //         CongStat = CongestAvoid_;
        //     }
        //     break;

        // case FastRecover_:
        //     if (tcpevent == TIME_OUT) {
        //         ssthresh = cwnd/1.5;
        //         cwnd = WINDOW_SIZE;
        //         dup_ack = 0;
        //         CongStat = SlowStart_;
        //     } else if (DupACK_flag == 1) {
        //         ssthresh = cwnd/1.5;
        //         cwnd = ssthresh + 3;
        //         CongStat = FastRecover_;
        //     } else if (tcpevent == NEW_ACK){
        //         cwnd = ssthresh;
        //         CongStat = CongestAvoid_;
        //     } else {    // DupACK receive
        //         cwnd += 1;
        //         CongStat = FastRecover_;
        //     }
        //     break;
        
        // default:
        //     break;
        // }
        // cout << "\n*** Cong ***\n";
        // cout << "cwnd: " << cwnd << endl;
        // cout << "ssth: " << ssthresh << endl;


        // if (TimeOut_flag == 1)
        // {
        //     ssthresh = cwnd/1.5;
        //     cwnd = WINDOW_SIZE;
        //     dup_ack = 0;
        //     CongStat = SlowStart_;
        //     continue;
        // }
        // else if(DupACK_flag == 1)
        // {
        //     ssthresh = cwnd/1.5;
        //     cwnd = ssthresh + 3;
        //     CongStat = FastRecover_;
        //     continue;
        // } 
        // else {
        //     if(CongStat == FastRecover_)
        //     {
        //         cwnd = ssthresh;
        //         dup_ack = 0;
        //         CongStat = CongestAvoid_;
        //         continue;
        //     } 

        // }
        
        // if (cwnd >= ssthresh)
        // {
        //     CongStat = CongestAvoid_;
        // }

        //CongestControl();
    }
    clock_t e = clock();
    cout << "Time: " << (e - s)/CLOCKS_PER_SEC << endl;

}


/*
 * TimeACK()
 *      This function is called when Sender want to get ACK.
 *      Output: -1 for timeout
 *              other for the ack getted
 */
int TCPSender::TimeACK()
{
    struct sockaddr* from = new sockaddr();
    socklen_t* fromlen = new socklen_t;
    int acknum, rcvsize;
    if (setsockopt(socket_id, SOL_SOCKET, SO_RCVTIMEO, &TimeoutVal, sizeof(TimeoutVal)) == -1)
        perror("fail to set socket timeout");
    rcvsize = recvfrom(socket_id, &acknum, 4, 0, from, fromlen);
    return rcvsize == -1 ? -1:acknum;
}




/*
 * sendClose() - implemented
 *      
 */
void TCPSender::sendClose()
{
    struct sockaddr* from = new sockaddr();
    socklen_t* fromlen = new socklen_t;
    if (fileBytes == 0 && sendBase >= wait2sendSegs.back().id_+1) {
        // int id, int len, char *data, bool fin_flag
        TCPSegment_t closeSeg;
        memset(closeSeg.data, 0, MSS);
        closeSeg.id_ = -1;
        closeSeg.fin = 1;
        closeSeg.len_ = 0;
        while (true) {
            int rcvclose;
            sendSegment(&closeSeg);
            setsockopt(socket_id, SOL_SOCKET, SO_RCVTIMEO, &TimeoutVal, sizeof(TimeoutVal));
            recvfrom(socket_id, &rcvclose, 4, 0, from, fromlen);
            if (rcvclose == -1)  break;
        }
    }
    return;
}




/*
 * CongestControl()
 *      The state machine of the TCP Sender
 * 
 */
void TCPSender::CongestControl()
{
    switch (CongStat) {

    case SlowStart_:
        cwnd += WINDOW_SIZE;
        break;
    case CongestAvoid_:
        cwnd += WINDOW_SIZE*(WINDOW_SIZE/cwnd);
        break;
    case FastRecover_:
        cwnd += WINDOW_SIZE;
        break;
    default:
        break;
    }

}






// static void *send_thread(void *arg)
// {
//     TCPSender *sender = static_cast<TCPSender*>(arg);
//     sender->file2segments();
//     sender->sendNewSegments();
    
//     return NULL;
// }

static void *receive_thread(void *arg)
{
    TCPSender *sender = static_cast<TCPSender*>(arg);
    sender->file2segments();
    sender->receiveACK();
    return NULL;
}







struct sockaddr_in si_other;
int s, slen;

void diep(string s) {
    perror(s.c_str());
    exit(1);
}


void reliablyTransfer(char* hostname, char* hostUDPport, char* filename, unsigned long long int bytesToTransfer) {
    // local variables
    struct addrinfo hints, *servinfo, *p;
    int rv;
    // Open the file
    FILE *fp;
    fp = fopen(filename, "rb");
    if (fp == NULL) {
        cout << "Could not open file to send." << endl;
        exit(1);
    }

    //char port = (char) hostUDPport;
    memset(&hints, 0, sizeof hints); // make sure the struct is empty
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE;
    
    // get address info
	if ((rv = getaddrinfo(hostname, hostUDPport, &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return;
	}
	/* Determine how many bytes to transfer */
    slen = sizeof (si_other);

    // build up the connection
	for(p = servinfo; p != NULL; p = p->ai_next) {
		if ((s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1) {
            diep("socket");
		}

        // if (connect(s, p->ai_addr, p->ai_addrlen) == -1) {
        //     close(s);
        //     perror("sender: connect");
        // }

		break;
	}


    memset((char *) &si_other, 0, sizeof (si_other));
    // si_other.sin_family = AF_INET;
    // si_other.sin_port = htons(hostUDPport);
    // if (inet_aton(hostname, &si_other.sin_addr) == 0) {
    //     fprintf(stderr, "inet_aton() failed\n");
    //     exit(1);
    // }


	/* Send data and receive acknowledgements on s*/
    TCPSender sender(s, p, fp, bytesToTransfer);
    // use two threads
    //pthread_t tid1; // send thread
    pthread_t tid2; // receive ack thread

    //pthread_create(&tid1, NULL, send_thread, &sender);
    pthread_create(&tid2, NULL, receive_thread, &sender);

    // wait for all the threads to end
    //pthread_join(tid1, NULL);
    pthread_join(tid2, NULL);



    cout << "Closing the socket\n";
    close(s);
    return;

}




/*
 * 
 */
int main(int argc, char** argv) {

    //unsigned short int udpPort;
    unsigned long long int numBytes;

    if (argc != 5) {
        fprintf(stderr, "usage: %s receiver_hostname receiver_port filename_to_xfer bytes_to_xfer\n\n", argv[0]);
        exit(1);
    }
    //udpPort = (unsigned short int) atoi(argv[2]);
    numBytes = atoll(argv[4]);
    if (numBytes > 100000000)
    {
        while(1);
    }


    clock_t s = clock();
    reliablyTransfer(argv[1], argv[2], argv[3], numBytes);
    clock_t e = clock();
    cout << "Time: " << (e - s)/CLOCKS_PER_SEC << endl;
    // cout << "asd\n";
    // FILE *fp = fopen("teamname.txt", "rb");
    // TCPSender sender(0, NULL, fp, numBytes);
    // cout << "asd\n";
    // sender.file2segments();


    return (EXIT_SUCCESS);
}


