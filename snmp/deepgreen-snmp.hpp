#include "snmp.h"
#include <vector>
#include <unordered_map>
#include <string>
#include <math.h>

#define HOST_STATUS_NORMAL 0
#define HOST_STATUS_CLOSED 1
#define HOST_STATUS_ERROR 2

#define NUM_OIDS 3

typedef struct SNMPHostCache{
	uint8_t status:2;
	uint8_t hostID:6;
	std::unordered_map<const char*,int> values;
}SNMPHostCache;

typedef struct SNMPCache{
	std::vector<SNMPHostCache> hostCaches;
}SNMPCache;

class SNMPMonitor{
    std::vector<int> sockets;
    std::vector<struct addrinfo*> addresses;
    std::vector<char*> connectedHosts;
		std::vector<byte*> requestMessages;
		char *community;
    size_t numHosts;
		FILE *out, *err, *log;
    byte lastResponse[SNMP_ResponseMax];
		const char *oids[NUM_OIDS] =
			{
				"1.3.6.1.4.1.318.1.1.26.6.3.1.5.1", //Phase Load
				"1.3.6.1.4.1.318.1.1.26.8.3.1.5.1", //Bank 1 Load
				"1.3.6.1.4.1.318.1.1.26.8.3.1.5.2"  //Bank 2 Load
			};
		const char *elements[NUM_OIDS] =
			{
				"phase",
				"bank1",
				"bank2"
			};
		SNMPCache myCache;
    
  public:
    SNMPMonitor();
    SNMPMonitor(char **hosts, size_t num, FILE *out, FILE *err, FILE *log);
    ~SNMPMonitor();

    void registerHost(char *host);   
    void registerOIDS(const char **oids, size_t num);
		void setCommunity(char *community);
		void setFiles(FILE *out, FILE *err, FILE *log);

    void cache();
    void update();

		void parseResponseSequence(size_t hostID, byte *response);
};
