#ifndef LIBSENSORS_SNMP_HOSTS_AND_OIDS
#define LIBSENSORS_SNMP_HOSTS_AND_OIDS
#define N_PDU_ENDPOINTS 2
char *pdu_endpoints[N_PDU_ENDPOINTS] = {
    (char*)"your_domain_here",
    (char*)"your_other_domain_here",
    };

#define N_PDU_OIDS 3
const char *pdu_oids[N_PDU_OIDS] =
{
    "1.3.6.1.4.1.318.1.1.26.6.3.1.5.1", //Phase Load
    "1.3.6.1.4.1.318.1.1.26.8.3.1.5.1", //Bank 1 Load
    "1.3.6.1.4.1.318.1.1.26.8.3.1.5.2"  //Bank 2 Load
};
const char *pdu_oid_elements[N_PDU_OIDS] =
{
    "phase",
    "bank1",
    "bank2"
};
#endif

