#ifndef SNMP_H
#define SNMP_H

#ifdef __cplusplus
extern "C"{
#endif

#include <netdb.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>

#define SNMP_V1 0x00
#define SNMP_V2 0x01
#define SNMP_V3 0x02

#define SNMP_Integer 0x02
#define SNMP_OctetString 0x04
#define SNMP_Null 0x05
#define SNMP_ObjID 0x06
#define SNMP_Sequence 0x30
#define SNMP_GetReq 0xa0
#define SNMP_GetRsp 0xa2
#define SNMP_SetReq 0xa3

#define SNMP_ResponseMax 512
#define SNMP_SendMax 255

typedef unsigned char byte;

int openSNMP(const char *host, struct addrinfo **serv_addr);
int sendSNMP(int sockfd, struct addrinfo *serv_addr, byte *msg);
int recvSNMP(int sockfd, struct addrinfo *serv_addr, byte *repsonse, size_t len);
void closeSNMP(int sockfd, struct addrinfo *serv_addr);

byte* createOIDStr(const char *oid);
byte* createGetRequestVBList(const char **oids, size_t num);
byte* createPDU(byte type, byte *vblist);
byte* createGetRequestMessage(byte version, byte* community, size_t cLen, const char **oids, size_t num);

size_t encodeLen(size_t len, byte **encodedLen);
size_t getEncodedLenLen(size_t len);
size_t decodeLen(byte *b);
size_t encodeOID(int oid, byte **encodedLen);
int decodeOID(byte *b);
void dumpSNMPMsg(FILE *f, byte *msg);
void dumpBytes(FILE *f, byte *b, size_t len);

#ifdef __cplusplus
}
#endif

#endif // SNMP_H
