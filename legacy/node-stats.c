#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/time.h>

int getCPUTemps = 0;
int polling = 0;
long int interval = 0;
int isIntel = 0;
int isAMD = 0;

#define die(e) do { perror(e); exit(1); } while(0);

char* __readline(char** in){
	char* ret = *in;
	for(int i = 0; ; i++){
		if(ret[i] == '\0' || ret[i] == '\n'){
			ret[i] = '\0';
			*in = &ret[i+1];
			break;
		}
	}
	return ret;
}

// prints command usage and options, then exits
void printHelp(){
	printf("Usage: node-stats [OPTIONS]\n");
	printf("\t-h | --help\n\t\t Print this help message and exit.\n");
	printf("\t-t | --temps\n\t\t Query current node temps.\n");
	printf("\t-p [interval] | --poll [interval]\n\t\t Run in polling mode. Print stats every specified interval in seconds. Interval must be >0.\n");
	exit(0);
}

// parse args and return to main with operations to do
void parseArgs(int num, char* args[]){
	// iterate through arg array
	char* arg;
	for(int i = 0; i < num; i++){
		arg = args[i];
		if(strcmp("--help", arg) == 0 || strcmp("-h", arg) == 0){
			printHelp();
		}else if(strcmp("--temps", arg) == 0 || strcmp("-t", arg) == 0){
			getCPUTemps = 1;
		}else if(strcmp("-p", arg) == 0 || strcmp("--poll", arg) == 0){
			polling = 1;
			if(i+1 >= num){
				fprintf(stderr, "Argument %s requires a value be passed.\n", arg);
				exit(1);
			}
			interval = strtol(args[++i], NULL, 10);
			if(interval <= 0){
				fprintf(stderr, "Invalid polling interval.\n");
				exit(1);
			}
		}else{ //non-recognized option
			fprintf(stderr, "Unrecognized command line argument: %s\n", arg);
			exit(1);
		}
	}
}

// query temps through sensors and print them
char* queryAMDTemps(){
	char* args[4] = {"/bin/bash", "-c", "sensors -jA | grep temp1_input | cut -d : -f 2 | tr --delete '\n '", NULL};
	char buffer[4096];
	int mypipe[2];

	if(pipe(mypipe)) die("pipe");

	int pid = fork();
	if(pid < 0) die("fork");
	if(pid == 0){
		dup2(mypipe[1], STDOUT_FILENO);
		close(mypipe[0]);
		close(mypipe[1]);
		execv(args[0], args);
		die("exec");
	}
	
	int n = read(mypipe[0], &buffer, sizeof(buffer)-1);
	if(n == 0) die("likely child didn't exec")
	buffer[n] = '\0';
	char* str = (char*)malloc(strlen(buffer)+1);
	strcpy(str, buffer);
	char* line;
	line = __readline(&str);
	return line;
}

// single temperature sampling can be done the same with sensors on both platforms
// this may need to be a different function later if we expand to multiple temp
// readings for CPU
char* queryIntelTemps(){
	char* args[4] = {"/bin/bash", "-c", "sensors -jA | grep _input | cut -d : -f 2 | tr --delete '\n '", NULL};
	char buffer[4096];
	int mypipe[2];

	if(pipe(mypipe)) die("pipe");

	int pid = fork();
	if(pid < 0) die("fork");
	if(pid == 0){
		dup2(mypipe[1], STDOUT_FILENO);
		close(mypipe[0]);
		close(mypipe[1]);
		execv(args[0], args);
		die("exec");
	}
	
	int n = read(mypipe[0], &buffer, sizeof(buffer)-1);
	if(n == 0) die("likely child didn't exec")
	buffer[n] = '\0';
	char* str = (char*)malloc(strlen(buffer)+1);
	strcpy(str, buffer);
	char* line;
	line = __readline(&str);
	return line;
}

char* queryCPUTemps(){
	if(isAMD) return queryAMDTemps();
	if(isIntel) return queryIntelTemps();
	return NULL;
}

// determine AMD/Intel/Other CPU
void queryDevice(){
	char* args[4] = {"/bin/bash","-c","cat /proc/cpuinfo | grep vendor_id | head -n 1 | cut -f 2 -d : | xargs", NULL};
	char buffer[4096];
	int mypipe[2];

	// setup interprocess pipe
	if(pipe(mypipe)) die("pipe");

	// make child and send it to do work
	int pid = fork();
	if(pid < 0) die("fork");
	if(pid == 0){
		dup2(mypipe[1], STDOUT_FILENO);
		close(mypipe[0]);
		close(mypipe[1]);
		execv(args[0], args);
		die("exec");
	}
	close(mypipe[1]);
	wait(NULL);

	// once child is done process its output
	int n = read(mypipe[0], &buffer, sizeof(buffer)-1);
	if(n == 0) die("likely child didn't exec")
	buffer[n] = '\0';
	char* str = (char*)malloc(strlen(buffer)+1);
	strcpy(str, buffer);
	char* str2 = __readline(&str);
	if(strcmp(str2, "GenuineIntel") == 0){
		isIntel = 1;
	}else if(strcmp(str2, "AuthenticAMD") == 0){
		isAMD = 1;
	}else{
		fprintf(stderr, "Unsupported CPU found - %s\n", str2);
		exit(1);
	}
}

void printHeader(){
	printf("timestamp,");
	if(getCPUTemps && isAMD) printf("k10temp-pci-00d3,k10temp-pci-00c3,k10temp-pci-00bd,k10temp-pci-00cb");
	if(getCPUTemps && isIntel) printf("package0,core0,core1,core2,core3,core4,core5,core6,core7,core8,core9,core10,core11,core12,core13,package1,core14,core15,core16,core17,core18,core19,core20,core21,core22,core23,core24,core25,core26,core27");
	printf("\n");
}

// main function
int main(int argc, char* argv[]){
	// arg checking/processing
	if(argc == 1) printHelp();
	parseArgs(argc-1, &(argv[1]));
	queryDevice();
	// do requested operations & pass results to the user through output
	printHeader();
	struct timeval t;
	do{
		if(gettimeofday(&t, NULL)) die("gettimeofday");
		printf("%ld,", t.tv_sec*1000+t.tv_usec);
		if(getCPUTemps){
			char* cpuTemps = queryCPUTemps();
			printf("%s", cpuTemps);
		}
		printf("\n");
		sleep(interval);
	}while(polling);
}
