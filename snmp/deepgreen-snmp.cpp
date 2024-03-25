#include "deepgreen-snmp.hpp"
#include "snmp.h"

SNMPMonitor::SNMPMonitor(){
  this->sockets = {};
  this->addresses = {};
  this->numHosts = 0;
  this->requestMessages = {};
  this->community = (char*)"public";
  this->out = stdout;
  this->err = stderr;
  this->log = stderr;
}

SNMPMonitor::SNMPMonitor(char **hosts, size_t num, FILE *out, FILE *err, FILE *log){
  this->sockets = {};
  this->addresses = {};
  this->numHosts = 0;
  this->requestMessages = {};
  this->community = (char*)"public";
  this->out = out == NULL ? stdout : out;
  this->err = err == NULL ? stderr : err;
  this->log = log == NULL ? stderr : log;

  for(size_t i = 0; i < num; i++) this->registerHost(hosts[i]);
}

SNMPMonitor::~SNMPMonitor(){
  for(size_t i = 0; i < this->numHosts; i++) closeSNMP(this->sockets.at(i), this->addresses.at(i));
  this->sockets.clear();
  this->addresses.clear();
  this->connectedHosts.clear();
  this->numHosts = 0;

  for(size_t i = 0; i < this->requestMessages.size(); i++) free(this->requestMessages.at(i));
  this->requestMessages.clear();
}

void SNMPMonitor::registerHost(char *host){
  struct addrinfo *addr;
  int sockfd = openSNMP(host, &addr);
  if(sockfd != -1){
    this->numHosts += 1;
    this->sockets.push_back(sockfd);
    this->addresses.push_back(addr);
    this->connectedHosts.push_back(host);
  }else{
    fprintf(this->log, "Couldn't open host %s\n", host);
  }
}

void SNMPMonitor::registerOIDS(const char **oids, size_t num){
  byte *msg = createGetRequestMessage(SNMP_V1, (byte*)this->community, strlen(this->community), oids, num);
  if(decodeLen(&msg[1]) > SNMP_SendMax){
#ifdef DEBUG
		fprintf(this->err, "Message too large to pass spliting into multiple\nOriginal Message: ");
		dumpSNMPMsg(this->err, msg);
#endif
		size_t msgLen = decodeLen(&msg[1]);
		free(msg);
		int fragments = (msgLen / SNMP_SendMax) + (msgLen % SNMP_SendMax == 0 ? 0 : 1);
		int oidsPerFrag = (num / fragments) + (num % fragments == 0 ? 0 : 1);
		size_t used = 0;
		while(used < num){
			int group = oidsPerFrag+used < num ? oidsPerFrag : num-used;
			byte *cur = createGetRequestMessage(SNMP_V1, (byte*)this->community, strlen(this->community), &oids[used], group);
			//TODO technically splitting in this manner doesn't ensure all fragments are now less than 255 but in practice this should do
			this->requestMessages.push_back(cur);
			used += group;
		}
  }else{
    this->requestMessages.push_back(msg);
  }
}

void SNMPMonitor::setCommunity(char *community){
	this->community = community;
}

void SNMPMonitor::setFiles(FILE *out, FILE *err, FILE *log){
	if(out) this->out = out;
	if(err) this->err = err;
	if(log) this->log = log;
}

void SNMPMonitor::cache(){
	// cache struct setup
	this->myCache.hostCaches = {};
	for(size_t i = 0; i < this->numHosts; i++){
		SNMPHostCache hc;
		hc.status = HOST_STATUS_NORMAL;
		hc.hostID = (uint8_t) i;
		hc.values = {};
		for(size_t j = 0; j < NUM_OIDS; j++){
			hc.values[std::string(this->oids[j])] = -1;
		}
		this->myCache.hostCaches.push_back(hc);
	}
	// create SNMP messages to be used throughout application lifetime
	this->registerOIDS(this->oids, NUM_OIDS);
	// write header to output file
	for(size_t i = 0; i < this->numHosts; i++){
		for(size_t j = 0; j < NUM_OIDS; j++){
			if(i || j) fprintf(this->out, ",%s:%s", this->connectedHosts[i], this->elements[j]);
			else fprintf(this->out, "%s:%s", this->connectedHosts[i], this->elements[j]);
		}
	}
}

void SNMPMonitor::update(){
  for(size_t i = 0; i < this->requestMessages.size(); i++){
    for(size_t j = 0; j < this->numHosts; j++){
#ifdef DEBUG
			fprintf(this->err, "Sending message (%ld,%ld):\n", i, j);
			dumpSNMPMsg(this->err, this->requestMessages.at(i));
#endif
      int r = sendSNMP(this->sockets.at(j), this->addresses.at(j), this->requestMessages.at(i));
      if(r < 0){
        //TODO handle send error; depends on error i.e. close host, cleanup, reconnect, send again, etc.
        fprintf(this->log, "send error\n");
        continue;
      }else if(r > 0){
        //TODO handle partial send; i.e. send again
        fprintf(this->log, "partial send\n");
        continue;
      }
      r = recvSNMP(this->sockets.at(j), this->addresses.at(j), this->lastResponse, SNMP_ResponseMax);
      if(r < 0){
        //TODO handle recv error; depends on error i.e. close host, cleanup, reconnect, receive again, etc.
        fprintf(this->log, "recv error\n");
        continue;
      }else if(r > 0){
        //TODO handle peer close; i.e. attempt to reconnect
        fprintf(this->log, "peer close\n");
        continue;
      }
#ifdef DEBUG
			fprintf(this->err, "Recieved message (%ld,%ld):\n", i, j);
			dumpSNMPMsg(this->err, this->lastResponse);
#endif
			this->parseGetResponseSequence(j, this->lastResponse);
    }
  }
	for(size_t i = 0; i < this->numHosts; i++){
		for(size_t j = 0; j < NUM_OIDS; j++){
			if(i || j) fprintf(this->out, ",%d", this->myCache.hostCaches[i].values[this->oids[j]]);
			else fprintf(this->out, "%d", this->myCache.hostCaches[i].values[this->oids[j]]);
		}
	}
}

int parseInteger(byte *string){
	size_t len = decodeLen(&string[1]);
	size_t lenLen = getEncodedLenLen(len);
	int r = 0;
	for(size_t cur = 1+lenLen; cur < 1+lenLen+len; cur++){
		r << 8;
		r += (int)string[cur];
	}
	return r;
}

void SNMPMonitor::parseGetResponseSequence(size_t hostID, byte *response){
	byte type = response[0];
	if(type != SNMP_Sequence) return;
	size_t seqLen = decodeLen(&response[1]);
	size_t lenLen = getEncodedLenLen(seqLen);
	size_t cur = 1+lenLen; // start cursor after sequence metadata
	size_t seqOffset = 0;
	size_t len;
	// jump over components until we get to the response portion
	while(seqOffset < seqLen){
		type = response[cur];
		len = decodeLen(&response[cur+1]);
		lenLen = getEncodedLenLen(len);
		if(type == SNMP_GetRsp) break;
		seqOffset += 1+lenLen+len;
		cur += 1+lenLen+len;
	}
	// begin processing the get response portion
	cur += 1+lenLen;
	seqOffset += 1+lenLen;
	// response ID component (skip)
	len = decodeLen(&response[cur+1]);
	lenLen = getEncodedLenLen(len);
	cur += 1+lenLen+len;
	seqOffset += 1+lenLen+len;
	// error component (if error occured stop)
	byte error = response[cur+2];
	if(error != 0x00) return;
	cur += 3;
	seqOffset += 3;
	// error index component (skip)
	len = decodeLen(&response[cur+1]);
	lenLen = getEncodedLenLen(len);
	cur += 1+lenLen+len;
	seqOffset += 1+lenLen+len;
	// now at the varbind list where all the requests are answered
	while(seqOffset < seqLen){
		size_t listLen = decodeLen(&response[cur+1]);
		size_t listOffset = 0;
		lenLen = getEncodedLenLen(listLen);
		cur += 1+lenLen;
		seqOffset += 1+lenLen;
		while(listOffset < listLen){
			// varbind metadata
			len = decodeLen(&response[cur+1]);
			lenLen = getEncodedLenLen(len);
			cur += 1+lenLen;
			seqOffset += 1+lenLen;
			listOffset += 1+lenLen;
			// OID component
			std::string oid = "";
			len = decodeLen(&response[cur+1]);
			lenLen = getEncodedLenLen(len);
			bool skip = false;
			cur += 1+lenLen;
			seqOffset += 1+lenLen;
			listOffset += 1+lenLen;
			size_t c = 0;
			for(size_t i = 0; i < len; i++){
				if(skip){
					if(!(response[cur+i] & 0x80)) skip = false;
					continue;
				}
				if(response[cur+i] & 0x80) skip = true;
				if(response[cur+i] == 0x2b){
					oid += "1.3";
				}else{
					oid += "."+std::to_string(decodeOID(&response[cur+i]));
				}
			}
			cur += len;
			seqOffset += len;
			listOffset += len;
			// value component
			len = decodeLen(&response[cur+1]);
			lenLen = getEncodedLenLen(len);
			int val = parseInteger(&response[cur]);
			this->myCache.hostCaches[hostID].values[oid] = val;
			cur += 1+lenLen+len;
			seqOffset += 1+lenLen+len;
			listOffset += 1+lenLen+len;
		}
	}
}
