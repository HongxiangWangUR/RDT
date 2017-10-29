/*
 * FILE: rdt_sender.cc
 * DESCRIPTION: Reliable data transfer sender.
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
#include "rdt_sender.h"

#include <math.h>
/*
 * Initial parameter is here
 */
#define WIN_SIZE 10     //window size
#define MAX_BUFFER_SIZE 20000 //the maximum buffer size
#define TIMEOUT_INTERVAL 0.3 //the timeout interval
#define TRUE 1
#define FALSE 0
#define CHECK 65535   //the true checksum value
#define MAX_SEQ 256   //the size of the sequnce number
#define HEADER_LEN 4 // the size of header

int timeout_count=0;

struct packet buffer[MAX_BUFFER_SIZE]; //the buffer used to store the packet
int next_buffer_position=0; //the next buffer position to store the data, range [ 0 ~ max_buffer_size-1 ]
int next_seq=0; //the next sequence number in the sequence number field, range [ 0 ~ MAX_SEQ-1 ]
int base=0; //point to the base of the window, range [ 0 ~ max_buffer_size-1 ]
int num_of_packet=0; //the number of packet in the buffer, range [ 0 ~ max_buffer_size ]

/* added function declerations are here */
unsigned short get_checksum(packet pkt, int size);
unsigned short check_sum(unsigned short *a, int len);
int check_packet(struct packet* pkt);
int sender_check(struct packet* pkt);


/* sender initialization, called once at the very beginning */
void Sender_Init()
{
    fprintf(stdout, "At %.2fs: sender initializing ...\n", GetSimulationTime());
}

/* sender finalization, called once at the very end.
   you may find that you don't need it, in which case you can leave it blank.
   in certain cases, you might want to take this opportunity to release some 
   memory you allocated in Sender_init(). */
void Sender_Final()
{
    fprintf(stdout, "At %.2fs: sender finalizing ...\n", GetSimulationTime());
}

/* event handler, called when a message is passed from the upper layer at the 
   sender */
void Sender_FromUpperLayer(struct message *msg)
{
  /* if the output buffer is full, then we just throw this packet */
  if(num_of_packet>=MAX_BUFFER_SIZE){
    printf("the buffer is full!\n");
    return;
  }

  int header_size=HEADER_LEN;
  int maxpayload_size=RDT_PKTSIZE-header_size;
  packet pkt={0};
  int cursor=0;

  printf("the base is %d, the number of packet is %d\n",base,num_of_packet);
  /*
   * make the packet and store it into the buffer
   */
  while( msg->size-cursor > maxpayload_size ){
    /* if the output buffer is full, then we just throw this packet */
    if(num_of_packet>=MAX_BUFFER_SIZE)
      return;
    /* set the sequnce number */
    pkt.data[0]=next_seq;
    /* set the next sequnce number */
    next_seq++;
    next_seq=next_seq % MAX_SEQ;
     /* set the number of data field */
    pkt.data[1]=maxpayload_size;
    /* initially, set the checksum field to be 0 */
    pkt.data[2]=0;
    pkt.data[3]=0;
    /* copy the data to the packet */
    memcpy(pkt.data+header_size,msg->data+cursor,pkt.data[1]);
    /* move the cursor */
    cursor+=maxpayload_size;
    /* get the checksum and set it in the field */
    unsigned short check=get_checksum(pkt,RDT_PKTSIZE);
    pkt.data[2]=(check>>8);
    pkt.data[3]=(check&0xff);
    /* store the packet into the buffer */
    buffer[next_buffer_position]=pkt;
    /* if this packet is within the window then send it */
    if(next_buffer_position<((base+WIN_SIZE)%MAX_BUFFER_SIZE)){
      Sender_ToLowerLayer(&buffer[next_buffer_position]);
      //test
      printf("sending sequence=%d\n",buffer[next_buffer_position].data[0]);
      printf("sending packet:");
      for(int i=0;i<(buffer[next_buffer_position].data[1]+4);i++){
        printf("%d",buffer[next_buffer_position].data[i]);
      }
      printf("\n");
      //end
      /* if this is the first packet then we start timer */
      if(base == next_buffer_position){
        Sender_StartTimer(TIMEOUT_INTERVAL);
      }
    }
    /* set the next buffer position to store data */
    next_buffer_position++;
    next_buffer_position=next_buffer_position % MAX_BUFFER_SIZE;
    /* increment the number of packet */
    num_of_packet++;
  }

  /* deal with the last packet */
  if(msg->size > cursor){
    /* if the output buffer is full, then we just throw this packet */
    if(num_of_packet>=MAX_BUFFER_SIZE)
      return;
    /* set the sequnce number */
    pkt.data[0]=next_seq;
    /* set the next sequnce number */
    next_seq++;
    next_seq=next_seq%MAX_SEQ;
    /* set the number of data field */
    pkt.data[1]=msg->size-cursor;
    /* initially, set the checksum field to be 0 */
    pkt.data[2]=0;
    pkt.data[3]=0;
    /* copy the data to the packet */
    memcpy(pkt.data+header_size, msg->data+cursor, pkt.data[1]);
    /* get the checksum and set it in the field */
    unsigned short check=get_checksum(pkt,pkt.data[1]+header_size);
    pkt.data[2]=(check>>8);
    pkt.data[3]=(check&0xff);
    /* store the packet into the buffer */
    buffer[next_buffer_position]=pkt;
    /* if this packet is within the window then send it */
    if(next_buffer_position<((base+WIN_SIZE)%MAX_BUFFER_SIZE)){
      Sender_ToLowerLayer(&buffer[next_buffer_position]);
       //test
      printf("sending sequence=%d\n",buffer[next_buffer_position].data[0]);
      printf("sending packet:");
      for(int i=0;i<(buffer[next_buffer_position].data[1]+4);i++){
        printf("%d",buffer[next_buffer_position].data[i]);
      }
      printf("\n");
      //end
      /* if this is the first packet then we start timer */
      if(base == next_buffer_position){
        Sender_StartTimer(TIMEOUT_INTERVAL);
      }
    }
    /* set the next buffer position to store data */
    next_buffer_position++;
    next_buffer_position=next_buffer_position % MAX_BUFFER_SIZE;
    /* increment the number of packet */
    num_of_packet++;
    printf("next buffer position to store data=%d\n",next_buffer_position);
  }

  printf("function sender from upper layer finished\n");
}

/* event handler, called when a packet is passed from the lower layer at the 
   sender */
void Sender_FromLowerLayer(struct packet *pkt)
{
  int corrupted=check_packet(pkt);
  /* first judge if this packet is corrupted */
  printf("sender corrupted=%d\n",corrupted);
  if(corrupted != TRUE){
    int seq_num=pkt->data[0];  //seq number of acked packet
    int base_seq=buffer[base].data[0];  //window base sequnce number
    /* interval is the distance between the window base and acked packet */
    int interval=(seq_num-base_seq+MAX_SEQ)%MAX_SEQ;
    //test
    printf("acked sequnce number=%d, base sequence number=%d\n",seq_num,base_seq);
    if( ( seq_num-base_seq < 0 && abs(seq_num-base_seq)< WIN_SIZE ) || seq_num == -1 ){
      printf("ignore the acked packet, function Sender_FromLowerLayer finished\n");
      return;
    }else if( interval < WIN_SIZE ){
      /* move is the number of step that the base need to move */
      int move=interval+1;
      /* update base and number of packet and send new packet */
      printf("window base move=%d\n",move);
      for(int i=0;i<move;i++){
        base=(base+1)%MAX_BUFFER_SIZE;
        num_of_packet--;
        if(num_of_packet>=10){

          Sender_ToLowerLayer(&buffer[(base+WIN_SIZE-1)%MAX_BUFFER_SIZE]);
        }
      }
      printf("the number of packet now is: %d\n",num_of_packet);
      printf("sender start a new timer\n");
      /* start a new timer */
      Sender_StartTimer(TIMEOUT_INTERVAL);
    }
  }
  printf("the base now at %d\n",base);
  printf("function Sender_FromLowerLayer finished\n");
}

/* event handler, called when the timer expires */
void Sender_Timeout()
{
  /* calculate the number of packet to be sent */
  printf("timeout\n");
  if(num_of_packet==0){
    printf("there are not packet in the buffer, stop timer and function Sender_Timeout finished\n");
    Sender_StopTimer();
    return;
  }
  int send_num=(num_of_packet>WIN_SIZE?WIN_SIZE:num_of_packet);
  printf("send num=%d\n",send_num);
  printf("num of packet=%d\n",num_of_packet);
  for(int i=0;i<send_num;i++){
    int index=(base+i)%MAX_BUFFER_SIZE;
    Sender_ToLowerLayer(&buffer[index]);
  }
  Sender_StartTimer(TIMEOUT_INTERVAL);

  printf("function sender timeout finished\n");
}

/*
 * this function is used to transform original packet into unsigned short buffer, the size is the number of bytes of 
 * the whole packet data
 */
unsigned short get_checksum(packet pkt, int size){
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

  return check_sum(pool,pool_size);
}

/*
 * Transform 16 bit word array into checksum, the len is the number of 16-bit word
 */
unsigned short check_sum(unsigned short *a, int len)
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
/*
 * check whether this packet is corrupted
 * If it is corrupted then return TRUE and if not return FALSE
 */
int check_packet(struct packet* pkt){
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