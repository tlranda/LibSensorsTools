#include "snmp.h"
#include <vector>
#include <unordered_map>
#include <string>

#define HOST_STATUS_NORMAL 0
#define HOST_STATUS_CLOSED 1
#define HOST_STATUS_ERROR 2

#define NUM_OIDS 20

typedef struct SNMPHostCache{
	uint8_t status:2;
	uint8_t hostID:6;
	std::unordered_map<char*,int> values;
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
		//TODO once we know what fields to query through SNMP populate the oids & elements here; currently filled with some random OIDs for testing
		char *oids[NUM_OIDS] =
			{
				(char*)"1.3.6.1.2.1.11.1.0",
				(char*)"1.3.6.1.2.1.11.2.0",
				(char*)"1.3.6.1.2.1.11.3.0",
				(char*)"1.3.6.1.2.1.11.4.0",
				(char*)"1.3.6.1.2.1.11.5.0",
				(char*)"1.3.6.1.2.1.11.6.0",
				(char*)"1.3.6.1.2.1.11.8.0",
				(char*)"1.3.6.1.2.1.11.9.0",
				(char*)"1.3.6.1.2.1.11.10.0",
				(char*)"1.3.6.1.2.1.11.11.0",
				(char*)"1.3.6.1.2.1.11.12.0",
				(char*)"1.3.6.1.2.1.11.13.0",
				(char*)"1.3.6.1.2.1.11.14.0",
				(char*)"1.3.6.1.2.1.11.15.0",
				(char*)"1.3.6.1.2.1.11.16.0",
				(char*)"1.3.6.1.2.1.11.17.0",
				(char*)"1.3.6.1.2.1.11.18.0",
				(char*)"1.3.6.1.2.1.11.19.0",
				(char*)"1.3.6.1.2.1.11.20.0",
				(char*)"1.3.6.1.2.1.11.21.0",
			};
		char *elements[NUM_OIDS] =
			{
				(char*)"name1",
				(char*)"name2",
				(char*)"name3",
				(char*)"name4",
				(char*)"name5",
				(char*)"name6",
				(char*)"name7",
				(char*)"name8",
				(char*)"name9",
				(char*)"name10",
				(char*)"name11",
				(char*)"name12",
				(char*)"name13",
				(char*)"name14",
				(char*)"name15",
				(char*)"name16",
				(char*)"name17",
				(char*)"name18",
				(char*)"name19",
				(char*)"name20",
			};
		SNMPCache myCache;
    
  public:
    SNMPMonitor();
    SNMPMonitor(char **hosts, size_t num, FILE *out, FILE *err, FILE *log);
    ~SNMPMonitor();

    void registerHost(char *host);   
    void registerOIDS(char **oids, size_t num);
		void setCommunity(char *community);
		void setFiles(FILE *out, FILE *err, FILE *log);

    void cache();
    void update();

		void parseResponseSequence(size_t hostID, byte *response);
};
