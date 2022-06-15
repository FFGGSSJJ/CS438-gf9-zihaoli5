/* 
 * File:   receiver_main.c
 * Author: 
 *
 * Created on
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
#include <list>

using namespace::std;


#define RCV_BUFFER_SIZE 512  // this value should be larger than the sender's window size
#define MSG_BUFFER_SIZE 1500  // this value should be larger than the sender's MAX_SEGMENT_SIZE

// Added Definition
#define DEBUG 0
#define MSS 1024               // byte
#define SEG_HEADRER_SIZE 10  // byte
#define MAX_SEGMENT_SIZE MSS+SEG_HEADRER_SIZE
#define WINDOW_SIZE 70           // set 1 for easy test
#define TIME_OUT_MICRO 20000    // micro second
#define MAGIC_LARGE 1000000000


struct sockaddr_in si_me, si_other;
int s, slen;


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



int file_write(char content[], FILE* file, int size)
{
    size_t written = 0;
    // fwrite(what to write, size of each element that is going to be written, numelements, file)
    written = fwrite(content, 1, size, file);
    return written;
}



void diep(string s) {
    perror(s.c_str());
    exit(1);
}



void reliablyReceive(unsigned short int myUDPport, char* destinationFile) {
    
    // variables
    int rcvsize;
    int last_ack = -1;
    char data[MSS];
    struct sockaddr_storage from;
    // this line is very important
    socklen_t fromlen = sizeof from;


    slen = sizeof (si_other);


    if ((s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
        diep("socket");

    memset((char *) &si_me, 0, sizeof (si_me));
    si_me.sin_family = AF_INET;
    si_me.sin_port = htons(myUDPport);
    si_me.sin_addr.s_addr = htonl(INADDR_ANY);
    printf("Now binding\n");
    if (bind(s, (struct sockaddr*) &si_me, sizeof (si_me)) == -1)
        diep("bind");


    // Initialize the rcv buffer
    list<TCPSegment_t> rcv_buf = list<TCPSegment_t>();
    TCPSegment_t empty;
    empty.fin = false;
    empty.id_ = -1;
    empty.len_ = 0;
    memset(empty.data, 0, MSS);
    for (int i = 0; i < RCV_BUFFER_SIZE;  i++) 
        rcv_buf.emplace_back(empty);
    list<TCPSegment_t>::iterator rcv_buf_begin = rcv_buf.begin();

    FILE* file = fopen(destinationFile, "wb");
    if (file == NULL)   diep("open file");


    // Receive Loop Begins Here
    while(1)
    {
        int sendBytes;
        // received info
        bool fin;
        int id;
        
        TCPSegment_t rcvseg;
        // cout << "wait....\n";
        if ((rcvsize = recvfrom(s, (char*)&rcvseg, MAX_SEGMENT_SIZE+3, 0, (struct sockaddr*)&from, &fromlen)) == -1)
            diep("receive error");
        fin = rcvseg.fin;
        id = rcvseg.id_;
        memcpy(data, rcvseg.data, rcvseg.len_);


        #if DEBUG
            cout << "\n\n### Receive ###\n";
            cout << "size:" << rcvsize << endl;
            cout << "fin: " << rcvseg.fin << endl;
            cout << "id: " << rcvseg.id_ << endl;
            cout << "len: " << rcvseg.len_ << endl;
            cout << "data: ";
            for (int i = 0; i < rcvseg.len_; i ++) {
                //cout << i << ":";
                cout << data[i];
            }
            cout << endl;
        #endif


        // FIN receives
        if (fin == 1) {
            int close = -1;
            sendto(s, &close, sizeof(int), 0, (struct sockaddr*)&from, fromlen);
            break;
        } 
        
        // New Expected ACK received
        else if (id == last_ack + 1) {
            *rcv_buf_begin = rcvseg;

            while ((*rcv_buf_begin).id_ != -1) {
                last_ack = (*rcv_buf_begin).id_;
                file_write((*rcv_buf_begin).data, file, (*rcv_buf_begin).len_);
                (*rcv_buf_begin) = empty;
                rcv_buf_begin++;

                // skip last element
                if (rcv_buf_begin == rcv_buf.end())
                    rcv_buf_begin++;
            }

            // send back ack
            if ((sendBytes = sendto(s, &last_ack, sizeof(int), 0, (struct sockaddr*)&from, fromlen)) == -1)
                perror("Fail to send ack number packet back");

            #if DEBUG
                cout << "\n\n**** NO DUP ****\n";
                cout << "send back ack: " << last_ack << endl;
            #endif
        } 
        
        // Ahead Segment received
        else if (id > last_ack + 1) {
            int offset = id - last_ack - 1;
            bool full_flag = false;

            list<TCPSegment>::iterator rcv_buf_ptr = rcv_buf_begin;
            while (offset > 0) {
                rcv_buf_ptr++;
                if (rcv_buf_ptr == rcv_buf.end())
                    rcv_buf_ptr++;
                if (rcv_buf_ptr == rcv_buf_begin) {
                    full_flag = true;
                    break;
                }
                offset--;
            }

            if (full_flag)  continue;
            if ((*rcv_buf_ptr).id_ == -1)    *rcv_buf_ptr = rcvseg;
            // send back last ack
            if ((sendBytes = sendto(s, &last_ack, sizeof(int), 0, (struct sockaddr*)&from, fromlen)) == -1)
                perror("Fail to send ack number packet back");
            
        } 
        
        // Previous ack received
        else {
            if ((sendBytes = sendto(s, &last_ack, sizeof(int), 0, (struct sockaddr*)&from, fromlen)) == -1)
                perror("Fail to send ack number packet back");
            continue;
        }
    }

    fclose(file);
    close(s);
	printf("%s received.", destinationFile);
    return;
}

/*
 * 
 */
int main(int argc, char** argv) {

    unsigned short int udpPort;

    if (argc != 3) {
        fprintf(stderr, "usage: %s UDP_port filename_to_write\n\n", argv[0]);
        exit(1);
    }

    udpPort = (unsigned short int) atoi(argv[1]);

    reliablyReceive(udpPort, argv[2]);
}

