#include "deepgreen-snmp.hpp"

int main(int argc, char* argv[]){
  char *hosts[2] = {(char*)"submerpdu-front.clemson.edu", (char*)"submerpdu-rear.clemson.edu"};
  SNMPMonitor mon = SNMPMonitor(hosts, 2, NULL, NULL, NULL);
  mon.cache();
	int i = 0;
	while(i++<10){
    mon.update();
    sleep(1);
  }
  return 0;
}
