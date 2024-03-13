#include "snmp.h"

#ifdef __cplusplus
extern "C"{
#endif

// SNMP Resources
// https://www.ranecommercial.com/legacy/note161.html
// https://intronetworks.cs.luc.edu/current1/html/netmgmt.html

// Connection Interface
int openSNMP(const char *host, struct addrinfo **serv_addr){
  int sockfd = -1;
  char *service = "snmp";
  struct addrinfo hints;
  struct addrinfo *rp;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_DGRAM;
  hints.ai_protocol = 0;
  hints.ai_canonname = NULL;
  hints.ai_addr = NULL;
  hints.ai_next = NULL; 
  
  int r = getaddrinfo(host, service, &hints, serv_addr);
  if(r){
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(r));
    return -1;
  }
  for(rp = *serv_addr; rp != NULL; rp = rp->ai_next){
    sockfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
    if(sockfd != -1) break;
  }
  *serv_addr = rp;
  if(sockfd == -1){
    fprintf(stderr, "Could not open socket\n");
  }
  return sockfd;
}

int sendSNMP(int sockfd, struct addrinfo *serv_addr, byte *msg){
  size_t msgLen = decodeLen(&msg[1]);
  int lenBytes = 1;
  int c = msgLen;
  while(c >= 128){
    lenBytes += 1;
    c /= 128;
  }
  msgLen += lenBytes + 1;
  ssize_t r = sendto(sockfd, msg, msgLen, 0, serv_addr->ai_addr, serv_addr->ai_addrlen);
  if(r < 0){
    fprintf(stderr, "%s\n", strerror(errno));
    return r;
  }else if(r != msgLen){
    fprintf(stderr, "SNMP Message partially transmitted\n");
    return 1;
  }
  return 0;
}

int recvSNMP(int sockfd, struct addrinfo *serv_addr, byte *response, size_t len){
  ssize_t r = recvfrom(sockfd, response, len, 0, serv_addr->ai_addr, &serv_addr->ai_addrlen);
  if(r < 0){
    fprintf(stderr, "%s\n", strerror(errno));
    return r;
  }else if(r == 0){
    fprintf(stderr, "Peer SNMP controller closed socket\n");
    return 1;
  }
  return 0;
}

void closeSNMP(int sockfd, struct addrinfo *serv_addr){
  close(sockfd);
  freeaddrinfo(serv_addr);
  serv_addr = NULL;
}

// Message Creation Interface
byte requestID = 0;

byte* createOIDStr(const char *oid){
  char *copy = malloc(strlen(oid)+1);
  strcpy(copy, oid);
  size_t oidStrLen = 1;
  byte *oidStr = malloc(oidStrLen);
  oidStr[0] = 0x2b; // first two numbers of the oid are fixed translation
  char *tok = strtok(copy, ".");
  tok = strtok(NULL, ".");
  while((tok = strtok(NULL, ".")) != NULL){
    int d = atoi(tok);
    byte *l = malloc(0);
    size_t r = encodeOID(d, &l);
    oidStrLen += r;
    oidStr = realloc(oidStr, oidStrLen);
    memcpy(&oidStr[oidStrLen-r], l, r);
    free(l);
  }
  byte *l = malloc(0);
  size_t r = encodeLen(oidStrLen, &l);
  oidStr = realloc(oidStr, oidStrLen+r+1);
  memmove(&oidStr[r+1], &oidStr[0], oidStrLen);
  oidStr[0] = SNMP_ObjID;
  memcpy(&oidStr[1], l, r);
  free(l);
  free(copy);
#ifdef DEBUG
  static int count = 0;
  fprintf(stderr, "OID %d: ", count++);
  dumpSNMPMsg(stderr, oidStr);
#endif
  return oidStr;
}

byte* createGetRequestVBList(const char **oids, size_t num){
  size_t vbListSize = 0;
  byte *vbList = malloc(vbListSize);
  for(size_t i = 0; i < num; i++){
    byte *oidStr = createOIDStr(oids[i]);
    size_t oidStrLen = decodeLen(&oidStr[1]) + 2;
    size_t vbLen = oidStrLen + 2;
    byte *l = malloc(0);
    size_t r = encodeLen(vbLen, &l);
    byte *vb = malloc(vbLen + r + 1);
    vb[0] = SNMP_Sequence;
    memcpy(&vb[1], l, r);
    memcpy(&vb[1+r], oidStr, oidStrLen);
    vb[1+r+oidStrLen] = SNMP_Null;
    vb[1+r+oidStrLen+1] = 0x00;
#ifdef DEBUG
    fprintf(stderr, "VB %ld: ", i);
    dumpSNMPMsg(stderr, vb);
#endif
    size_t newSize = vbListSize + vbLen + r + 1;
    vbList = realloc(vbList, newSize);
    memcpy(&vbList[vbListSize], vb, vbLen + r + 1);
    vbListSize = newSize;
    free(l);
    free(oidStr);
    free(vb);
  }
  byte *l = malloc(0);
  size_t r = encodeLen(vbListSize, &l);
  vbList = realloc(vbList, vbListSize+1+r);
  memmove(&vbList[r+1], &vbList[0], vbListSize);
  vbList[0] = SNMP_Sequence;
  memcpy(&vbList[1], l, r);
  free(l);
#ifdef DEBUG
  fprintf(stderr, "VBList: ");
  dumpSNMPMsg(stderr, vbList);
#endif
  return vbList;
}

byte* createPDU(byte type, byte *vblist){
  // TODO technically the requestID should support multiple bytes but SNMP
  // doesn't seem to care & we don't use it rigoursly enough to care either
  byte ridStr[3] = {SNMP_Integer, 0x01, requestID++};
  byte errStr[3] = {SNMP_Integer, 0x01, 0x00};
  byte errIdx[3] = {SNMP_Integer, 0x01, 0x00};
  size_t vblistLen = decodeLen(&vblist[1]);
  size_t lenBytes = getEncodedLenLen(vblistLen);
  vblistLen += lenBytes + 1;
  size_t pduLen = 3 + 3 + 3 + vblistLen;
  byte *l = malloc(0);
  size_t r = encodeLen(pduLen, &l);
  byte *pduMsg = malloc(pduLen+1+r);
  pduMsg[0] = type;
  memcpy(&pduMsg[1], l, r);
  memcpy(&pduMsg[1+r], ridStr, 3);
  memcpy(&pduMsg[4+r], errStr, 3);
  memcpy(&pduMsg[7+r], errIdx, 3);
  memcpy(&pduMsg[10+r], vblist, vblistLen);
  free(l);
#ifdef DEBUG
  fprintf(stderr, "PDU: ");
  dumpSNMPMsg(stderr, pduMsg);
#endif
  return pduMsg;
}

byte* createGetRequestMessage(byte version, byte* community, size_t cLen, const char **oids, size_t num){
  // Version Payload
  byte verStr[3] = {SNMP_Integer, 0x01, version};
#ifdef DEBUG
  fprintf(stderr, "Version Payload: ");
  dumpSNMPMsg(stderr, verStr);
#endif
  // Community Payload
  byte *l = malloc(0);
  size_t r = encodeLen(cLen, &l);
  byte *comStr = malloc(cLen+r+1);
  comStr[0] = SNMP_OctetString;
  memcpy(&comStr[1], l, r);
  memcpy(&comStr[1+r], community, cLen);
  free(l);
#ifdef DEBUG
  fprintf(stderr, "Community Payload: ");
  dumpSNMPMsg(stderr, comStr);
#endif
  // PDU Payload
  byte *vblist = createGetRequestVBList(oids, num);
  byte *pdu = createPDU(SNMP_GetReq, vblist);
  size_t pduLen = decodeLen(&pdu[1]);
  size_t lenBytes = getEncodedLenLen(pduLen);
  pduLen += lenBytes + 1;
  // GetRequest = Version + Community + PDU
  size_t msgLen = 3 + cLen+r+1 + pduLen;
  byte *l2 = malloc(0);
  size_t r2 = encodeLen(msgLen, &l2);
  byte *msg = malloc(msgLen+1+r2);
  msg[0] = SNMP_Sequence;
  memcpy(&msg[1], l2, r2);
  memcpy(&msg[1+r2], verStr, 3);
  memcpy(&msg[4+r2], comStr, cLen+r+1);
  memcpy(&msg[4+r2+cLen+r+1], pdu, pduLen);
  free(l2);
  free(vblist);
  free(pdu);
  free(comStr);
#ifdef DEBUG
  fprintf(stderr, "MSG: ");
  dumpSNMPMsg(stderr, msg);
#endif
  return msg;
}

// Utility Functions
size_t encodeLen(size_t len, byte **encodedStr){
  if(len <= 0x7f){
    *encodedStr = realloc(*encodedStr, 1);
    (*encodedStr)[0] = (byte) len;
    return 1;
  }
  int bytes = 1;
  int c = len;
  while(c >= 256){
    bytes += 1;
    c /= 256;
  }
  *encodedStr = realloc(*encodedStr, bytes+1);
  (*encodedStr)[0] = (byte) bytes | 0x80;
  for(int b = 1; b <= bytes; b++){
    (*encodedStr)[bytes+1-b] = (byte) len % 256;
    len /= 256;
  }
  return bytes+1;
}

size_t getEncodedLenLen(size_t len){
  if(len <= 0x7f) return 1;
  size_t bytes = 1;
  while(len >= 256){
    bytes += 1;
    len /= 256;
  }
  return bytes+1; 
}

size_t decodeLen(byte *b){
  if(b[0] & 0x80){
    int lenBytes = b[0] & (~0x80);
    size_t r = 0;
    for(int i = 1; i <= lenBytes; i++){
      r *= 256;
      r += b[i] % 256;
    }
    return r;
  }
  return (size_t) b[0];
}

size_t encodeOID(int oid, byte **encodedStr){
  size_t bytes = 1;
  int c = oid;
  while(c >= 128){
    bytes += 1;
    c /= 128;
  }
  *encodedStr = realloc(*encodedStr, bytes);
  for(int b = 1; b <= bytes; b++){
    if(b == 1) (*encodedStr)[bytes-b] = (byte) oid % 128;
    else (*encodedStr)[bytes-b] = (byte) (oid % 128) | 0x80;
    oid /= 128;
  }
  return bytes;
}

int decodeOID(byte *b){
  int r = 0;
  int idx = 0;
  while(b[idx] & 0x80){
    int val = b[idx] & (~0x80);
    r *= 128;
    val *= 128;
    r += val;
    idx++;
  }
  r += b[idx];
  return r;
}

void dumpSNMPMsg(FILE *f, byte *msg){
  size_t len = decodeLen(&msg[1]);
  fprintf(f, "%02x ", msg[0]);
  size_t lenLen = getEncodedLenLen(len);
  int idx = 1;
  for(int i = 0; i < lenLen; i++){
    fprintf(f, "%02x ", msg[idx+i]);
  }
  idx = 1+lenLen;
  for(int i = idx; i < len+idx; i++){
    fprintf(f, "%02x ", msg[i]);
  }
  fprintf(f, "\n");
}

void dumpBytes(FILE *f, byte *b, size_t len){
  for(int i = 0; i < len; i++){
    fprintf(f, "%02x ", b[i]);
  }
  fprintf(f, "\n");
}

#ifdef __cplusplus
}
#endif
