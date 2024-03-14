// Headers and why they're included
// Document necessary compiler flags when necessary
//#include "snmp_hosts_and_oids.h" // Private information with Hosts and OIDs to query
#include "snmp.h" // SNMP library headers
#include <vector> // Vector types
#include <math.h> // ?
#include "io/argparse_libsensors.h" // DebugLevels, OutputFormats, argstruct/args, Output, update

//#define HOST_STATUS_NORMAL 0
//#define HOST_STATUS_CLOSED 1
//#define HOST_STATUS_ERROR 2
// End Headers

typedef struct oid_cache_t {
  //uint8_t status:2 = HOST_STATUS_NORMAL;
  std::vector<int> values;
  std::vector<std::string> oid_names;
  std::vector<std::string> field_names;
} OID_cache;

typedef struct pdu_cache_t {
  // IDs
  int index;
  struct addrinfo *addr;
  int sockfd = -1;
  char *host;
  std::vector<byte*> requestMessages;
  byte lastResponse[SNMP_ResponseMax];
  // Cached data
  OID_cache oid_cache;

  // Destructor should be safe and reliably free memory
  ~pdu_cache_t() {
    if (sockfd != -1) closeSNMP(sockfd, addr);
    for (size_t i = 0; i < requestMessages.size(); i++) free(requestMessages.at(i));
    requestMessages.clear();
  }
} pdu_cache;


// End Class and Type declarations

// Function declarations
void cache_pdus(void);
int update_pdus(void);
// End Function declarations

// External variable declarations
extern std::vector<std::unique_ptr<pdu_cache>> known_pdus;
extern int pdus_to_satisfy;
// End External variable declarations

