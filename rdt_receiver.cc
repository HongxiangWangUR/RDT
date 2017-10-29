/*
 * FILE: rdt_receiver.cc
 * DESCRIPTION: Reliable data transfer receiver.
 * VERSION: 0.2
 * AUTHOR: Kai Shen (kshen@cs.rochester.edu)
 * NOTE: This implementation assumes there is no packet loss, corruption, or 
 *       reordering.  You will need to enhance it to deal with all these 
 *       situations.  In this implementation, the packet format is laid out as 
 *       the following:
 *       
 *       |<-  1 byte  ->|<-             the rest            ->|
 *       | payload size |<-             payload             ->|
 *
 *       The first byte of each packet indicates the size of the payload
 *       (excluding this single-byte header)
 */

/*
 * My packet format:
 *
 * |<- 1 byte seq number ->|<- 1 byte data size ->|<- 2 byte checksum ->|<- 60 or less bytes data ->|
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "rdt_struct.h"
#include "rdt_receiver.h"

#define TRUE 1
#define FALSE 0
#define HEADER_LEN 4 // the size of header
#define MAX_SEQ 256   //the size of the sequnce number
#define CHECK 65535   //the true checksum value

int expected_seq=0; //the expected sequence number

packet tosender; //the packet to sender

/* function declerations are here */
int receiver_check(struct packet* pkt);
unsigned short receiver_get_checksum(packet pkt, int size);
unsigned short receiver_check_sum(unsigned short *a, int len);

/* receiver initialization, called once at the very beginning */
void Receiver_Init()
{
    tosender.data[0]=-1; //initial the packet sequence number to -1
    fprintf(stdout, "At %.2fs: receiver initializing ...\n", GetSimulationTime());
}

/* receiver finalization, called once at the very end.
   you may find that you don't need it, in which case you can leave it blank.
   in certain cases, you might want to use this opportunity to release some 
   memory you allocated in Receiver_init(). */
void Receiver_Final()
{
    fprintf(stdout, "At %.2fs: receiver finalizing ...\n", GetSimulationTime());
}

/* event handler, called when a packet is passed from the lower layer at the 
   receiver */
void Receiver_FromLowerLayer(struct packet *pkt)
{
  printf("function receiver from lower layer begin\n");
    /* first we find if the packet is corrupted */
    int corrupted=receiver_check(pkt);
    printf("receiver corrupted=%d\n",corrupted);
    /* if it is not corrupted */
    if(corrupted != TRUE){
      //test
      printf("receive packet:");
      for(int i=0;i<pkt->data[1]+4;i++){
        printf("%d",pkt->data[i]);
      }
      printf("\n");
      //end
        /* get the sequence number in the packet */
        int seq_num=(unsigned char)pkt->data[0];
        printf("receiver sequence=%d, expected sequence=%d\n",seq_num,expected_seq);

        /* if the packet is as expected */
        if(seq_num==expected_seq){
            /* set the acked sequence number */
            tosender.data[0]=seq_num;
            /* set the next sequence number */
            expected_seq++;
            expected_seq=expected_seq%MAX_SEQ;
            /* tranmit the message to upper layer */
            struct message *msg = (struct message*) malloc(sizeof(struct message));
            msg->size=pkt->data[1];
            msg->data=(char*)malloc(msg->size);
            memcpy(msg->data,pkt->data+HEADER_LEN,msg->size);
            Receiver_ToUpperLayer(msg);
            /* free memory space */
            free(msg->data);
            free(msg);
        }
        /* set data length */
        tosender.data[1]=0;
        /* set initial checksum to 0 */
        tosender.data[2]=0;
        tosender.data[3]=0;
        unsigned short check=receiver_get_checksum(tosender,HEADER_LEN);
        tosender.data[2]=(check>>8);
        tosender.data[3]=(check&0xff);
        Receiver_ToLowerLayer(&tosender);
        printf("receiver send acked sequence number=%d\n",tosender.data[0]);
    }else{
        printf("the packet is corrupted so it was ignored\n");
    }
    /* if the packet is corrupted we just ignore it */
    printf("function receiver from lower layer finished\n");
}

/*
 * check if this packet is corrupted
 * If it is corrupted then return TRUE and if not return FALSE
 */
int receiver_check(struct packet* pkt){
  /* if the number of data is less than zero, then it must be corrupted */
  printf("receiver check packet begin\n");
  if(pkt->data[1]<0){
    return TRUE;
  }
  /* number of the bytes in whole packet */
  int bsize=pkt->data[1]+HEADER_LEN;
  /* size of the 16-bit word buffer */
  int pool_size=bsize/2;
  if( ( bsize % 2 ) == 1 )
    pool_size+=1;
  /* 16-bit word buffer */
  unsigned short pool[pool_size];
  /* count the number of byte in the packet */
  int count=0;
  /* count the current position of 16-bit word */
  int pool_count=0;

  while(bsize>1){
    /* initialize every word to zero */
    pool[pool_count]&=0;
    /* put the first byte in the higher 8 bit position */
    pool[pool_count]+=(((unsigned short)pkt->data[count])<<8);
    count++;
    /* put the second byte in the lower 8 bit position */
    pool[pool_count]+=(((unsigned short)pkt->data[count])&0xff);
    /* move the pointers */
    count++;
    bsize-=2;
    pool_count++;
  }

  if(bsize){
    pool[pool_count]&=0;
    pool[pool_count]+=(((unsigned short)pkt->data[count])<<8);
  }
  /* add all of the 16-bit word */
  unsigned short sum=0;
  for(int i=0;i<pool_size;i++){
    sum+=pool[i];
  }
  if(sum==CHECK){
    return FALSE;
  }else{
    return TRUE;
  }
}

/*
 * this function is used to transform original packet into unsigned short buffer, the size is the number of bytes of 
 * the whole packet data
 */
unsigned short receiver_get_checksum(packet pkt, int size){
  /* the size of the 16 bit word array */
  int pool_size=size/2;
  pool_size+=(size%2==0?0:1);
  /* 16 bit word is stored at here */
  unsigned short pool[pool_size];
  /* the current position to store transformed data */
  int pool_count=0;
  /* the current position in packet data */
  int count=0;

  while(size>1){
    /* initialize every word to zero */
    pool[pool_count]&=0;
    /* put the first byte in the higher 8 bit position */
    pool[pool_count]+=(((unsigned short)pkt.data[count])<<8);
    count++;
    /* put the second byte in the lower 8 bit position */
    pool[pool_count]+=(((unsigned short)pkt.data[count])&0xff);
    /* move the pointers */
    count++;
    size-=2;
    pool_count++;
  }

  if(size){
    pool[pool_count]&=0;
    pool[pool_count]+=(((unsigned short)pkt.data[count])<<8);
  }

  return receiver_check_sum(pool,pool_size);
}

/*
 * Transform 16 bit word array into checksum, the len is the number of 16-bit word
 */
unsigned short receiver_check_sum(unsigned short *a, int len)
{
    unsigned int sum = 0; //this is the checksum
    int count=0; // count the current 16-bit word

    /* every time we add the two 16-bit word */
    while (len > 1) {
        sum += a[count];count++;
        sum += a[count];count++;
        len -= 2;
    }
    if(len){
      sum+= a[count];
    }

    return (unsigned short)(~sum);
}