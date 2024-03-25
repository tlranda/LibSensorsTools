#include "deepgreen-snmp.hpp"

int main(int argc, char* argv[]){
  // SNMP hosts we will communicate with
  char *hosts[2] = {(char*)"submerpdu-front.clemson.edu", (char*)"submerpdu-rear.clemson.edu"};
  // SNMP Monitor object (hosts, # hosts, out FILE*, err FILE*, log FILE*)
  // out - CSV of metrics collected through SNMP [stdout]
  // err - runtime debug information; should be empty unless compiled specifically [stderr]
  // log - unexpected runtime behaviors, i.e. network disconnect [stderr]
  SNMPMonitor mon = SNMPMonitor(hosts, 2, NULL, NULL, NULL);
  // initialize the SNMP Monitor's cache, writes header to out FILE*
  mon.cache();
  fprintf(stdout, "\n");
  int i = 0;
  while(i++<10){
    // update will send out the SNMP requests and cache response values, writes data to out FILE*
    mon.update();
    fprintf(stdout, "\n");
    sleep(1);
  }
  return 0;
}
