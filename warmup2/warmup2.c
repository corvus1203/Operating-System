#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/time.h>
#include <unistd.h>
#include <math.h>
#include <dirent.h>
#include <errno.h>
#include <signal.h>

#include "cs402.h"
#include "my402list.h"

typedef struct timePacket{
    long int id;
    struct timeval arrivalTime;
    struct timeval q1Enter;
    struct timeval q1Leave;
    struct timeval q2Enter;
    struct timeval q2Leave;
    struct timeval sBegin;
    struct timeval sLeave;
    
    int serviceNumber;
    long int tokensNeed;
    long int serviceTime;
    long int interArrivalTime;
}Packet;

void initial(void);
void printErr(char * err);
void printParameter(void);
void commandLine(int argc, char * argv[]);
void checkInt(char * number, long int * x, char * string);
void checkDouble(char * number, double * x, char * string);
void * packetGenerator(void * arg);
void * tokenGenerator(void * arg);
void * service(void * arg);
void start(void);
long int parseTime(struct timeval time);
void checkQ1(struct timeval time);
void printEmulationP(void);
void calculate(Packet * packet);
void calculateInterArrival(Packet* packet);
void statistics(void);
void readFile(char * file);
void readLine(long int * interval, long int * tokenN, long int * serviceT);
void * monitor(void * arg);
void cleanUp(void);

double lambda; // packet
double mu;     // service
double r;      // token
long int B;    // bucket
long int P;    // token need
long int num;  // num of packet
int mode = 0;  // mode
pthread_t t[4];
My402List * Q1, * Q2;
long int tokenCount = 0; //lalala
long int tokenId = 0;
long int tokenDrop = 0;
long int packetGenerate = 0;
long int packetDiscard = 0;
pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cv = PTHREAD_COND_INITIALIZER;
struct timeval startTime;
struct timeval endTime;
char* filename;
int packGDead = 0;
int tokenGDead = 0;

FILE * fp = NULL;
int lineNumber = 0;

long int totalNumLeft = 0;

double averageInterArrival = 0, averageService = 0, timeInQ1 = 0, timeInQ2 = 0, timeInS1 = 0, timeInS2 = 0, averageSystem = 0, sumSquareSystem = 0;

sigset_t set;
int terminate;


int main(int argc, char * argv[]) {
    initial();
    pthread_t thread;
    sigemptyset(&set);
    sigaddset(&set, SIGINT);
    sigprocmask(SIG_BLOCK, &set, 0);
    pthread_create(&thread, 0, monitor, 0);
    commandLine(argc, argv);
    start();
    if(mode == 1){
        fclose(fp);
    }
    cleanUp();
    gettimeofday(&endTime, NULL);
    long int pT = parseTime(endTime);
    fprintf(stdout, "%08ld.%03ldms: emulation ends\n\n", pT/1000, pT%1000);
    
    statistics();
}

void start() {
    Q1 = malloc(sizeof(My402List));
    Q2 = malloc(sizeof(My402List));
    My402ListInit(Q1);
    My402ListInit(Q2);
    printEmulationP();
    gettimeofday(&startTime, NULL);
    long int pT = parseTime(startTime);
    fprintf(stdout, "%08ld.%03ldms: emulation begins\n", pT/1000, pT%1000);
    
    int error;
    void* (*fun_ptr_arr[])(void *) = {packetGenerator, tokenGenerator, service, service};
    for(int i = 0; i < 4; i++){
        if((error = pthread_create(&t[i], 0, fun_ptr_arr[i], (void *)(long)(i-1)))){
            fprintf(stderr, "pthread_create: %s", strerror(error));
            exit(1);
        }
    }
    for(int i = 0; i < 4; i++){
        pthread_join(t[i], 0);
    }
}

void * packetGenerator(void * arg){
    struct timeval time;
    long int timeLastAdd = 0, timeSleep;
    long int interval = 0; //milisec
    long int tokenN = 0;
    long int serviceT = 0;
    interval = round(1000/lambda);
    if(interval > 10000){
        interval = 10000; //milisec
    }
    interval *= 1000; //microseconds
    
    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, 0);
    while(1){
        if(mode == 1){
            readLine(&interval, &tokenN, &serviceT);
            interval *= 1000;
        }
        gettimeofday(&time, NULL);
        long int pT = parseTime(time);
        timeSleep = timeLastAdd + interval - pT;
        
        if(timeSleep > 0){
            pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, 0);
            usleep((int)timeSleep);
            pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, 0);
        }
        pthread_mutex_lock(&m);
        if(terminate){
            pthread_mutex_unlock(&m);
            return 0;
        }
        packetGenerate++;
        Packet* packet = malloc(sizeof(Packet));
        if(mode == 0){
            packet->tokensNeed = P;
        }else if(mode == 1){
            packet->tokensNeed = tokenN;
            packet->serviceTime = serviceT;
        }
        packet->id = packetGenerate;
        gettimeofday(&packet->arrivalTime, NULL);
        pT = parseTime(packet->arrivalTime);
        packet->interArrivalTime = pT-timeLastAdd;
        calculateInterArrival(packet);
        
        if(packet->tokensNeed <= B){
            fprintf(stdout, "%08ld.%03ldms: p%ld arrives, needs %ld tokens, inter-arrival time = %ld.%03ldms\n", pT/1000, pT%1000, packet->id, packet->tokensNeed, packet->interArrivalTime/1000, packet->interArrivalTime%1000);
            My402ListAppend(Q1, packet);
            gettimeofday(&packet->q1Enter, NULL);
            long int enter = parseTime(packet->q1Enter);
            fprintf(stdout, "%08ld.%03ldms: p%ld enters Q1\n", enter/1000, enter%1000, packet->id);
        }else{
            packetDiscard++;
            fprintf(stdout, "%08ld.%03ldms: p%ld arrives, needs %ld tokens, inter-arrival time = %ld.%03ldms, dropped\n", pT/1000, pT%1000, packet->id, packet->tokensNeed, packet->interArrivalTime/1000, packet->interArrivalTime%1000);
            free(packet);
        }
        timeLastAdd = pT;
        if(My402ListLength(Q1) == 1){
            checkQ1(time);
        }
        if(packetGenerate == num){
            pthread_mutex_unlock(&m);
            return 0;
        }
        pthread_mutex_unlock(&m);
    }
    return 0;
}

void checkQ1(struct timeval time){
    My402ListElem* firstE = My402ListFirst(Q1);
    Packet* firstP = (Packet*) (firstE->obj);
    if(firstP->tokensNeed <= tokenCount){
        tokenCount -= firstP->tokensNeed;
        My402ListUnlink(Q1, firstE);
        gettimeofday(&firstP->q1Leave, NULL);
        long int pT = parseTime(firstP->q1Leave);
        timersub(&firstP->q1Leave, &firstP->q1Enter, &time);
        long int diff = time.tv_sec*1000000 + time.tv_usec;
        
        fprintf(stdout, "%08ld.%03ldms: p%ld leaves Q1, time in Q1 = %ld.%03ldms, token bucket now has %ld token\n", pT/1000, pT%1000, firstP->id, diff/1000, diff%1000, tokenCount);
        My402ListAppend(Q2, firstP);
        gettimeofday(&firstP->q2Enter, NULL);
        pT = parseTime(firstP->q2Enter);
        fprintf(stdout, "%08ld.%03ldms: p%ld enters Q2\n", pT/1000, pT%1000, firstP->id);
        pthread_cond_broadcast(&cv);
    }
}

void * tokenGenerator(void * arg){
    struct timeval time;
    long int timeLastAdd = 0, timeSleep;
    long int interval = round(1000/r) ;
    if(interval > 10000){
        interval = 10000; //milisec
    }
    interval *= 1000; //microseconds
    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, 0);
    while(1){
        gettimeofday(&time, NULL);
        long int pT = parseTime(time);
        timeSleep = timeLastAdd + interval - pT;  //microseconds
        if(timeSleep > 0){
            pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, 0);
            usleep((int)timeSleep);
            pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, 0);
        }
        pthread_mutex_lock(&m);
        if(terminate){
            pthread_mutex_unlock(&m);
            return 0;
        }
        tokenId++;
        gettimeofday(&time, NULL);
        pT = parseTime(time);
        timeLastAdd = pT;
        if(tokenCount < B){
            tokenCount++;
            fprintf(stdout, "%08ld.%03ldms: token t%ld arrives, token bucket now has %ld tokens\n", pT/1000, pT%1000, tokenId, tokenCount);
        }else {
            tokenDrop++;
            fprintf(stdout, "%08ld.%03ldms: token t%ld arrives, dropped\n", pT/1000, pT%1000, tokenId);
        }
        if(!My402ListEmpty(Q1)){
            checkQ1(time);
        }
        if(packetGenerate == num && My402ListEmpty(Q1)){
            pthread_cond_broadcast(&cv);
            pthread_mutex_unlock(&m);
            return 0;
        }
        pthread_mutex_unlock(&m);
    }
    return 0;
}

long int parseTime(struct timeval time){
    struct timeval res;
    timersub(&time, &startTime, &res);
    return 1000000 * res.tv_sec + res.tv_usec;
}

void * service(void * arg){
    int serviceNumber = (int)(long)arg;
    struct timeval time;
    long int requestingTime;
    requestingTime = round(1000/mu);
    if(requestingTime > 10000){
        requestingTime = 10000; //milisec
    }
    
    while(1){
        pthread_mutex_lock(&m);
        while(My402ListEmpty(Q2) || terminate){
            if(terminate || (packetGenerate == num && My402ListEmpty(Q1))){
                pthread_mutex_unlock(&m);
                return 0;
            }
            pthread_cond_wait(&cv, &m);
        }
        My402ListElem* firstE = My402ListFirst(Q2);
        Packet* firstP = (Packet*) (firstE->obj);
        if(mode == 1){
            requestingTime = firstP->serviceTime;
        }
        My402ListUnlink(Q2, firstE);
        firstP->serviceNumber = serviceNumber;
        gettimeofday(&firstP->q2Leave, NULL);
        long int pT = parseTime(firstP->q2Leave);
        timersub(&firstP->q2Leave, &firstP->q2Enter, &time);
        long int diff = time.tv_sec*1000000 + time.tv_usec;
        
        fprintf(stdout, "%08ld.%03ldms: p%ld leaves Q2, time in Q2 = %ld.%03ldms\n", pT/1000, pT%1000, firstP->id, diff/1000, diff%1000);
        gettimeofday(&firstP->sBegin, NULL);
        pT = parseTime(firstP->sBegin);
        fprintf(stdout, "%08ld.%03ldms: p%ld begins service at S%d, requesting %ldms of service\n", pT/1000, pT%1000, firstP->id, serviceNumber, requestingTime);
        pthread_mutex_unlock(&m);
        
        if(requestingTime > 0){
            usleep(1000*(int)requestingTime);
        }
        
        pthread_mutex_lock(&m);
        gettimeofday(&firstP->sLeave, NULL);
        pT = parseTime(firstP->sLeave);
        timersub(&firstP->sLeave, &firstP->sBegin, &time);
        diff = time.tv_sec*1000000 + time.tv_usec;
        timersub(&firstP->sLeave, &firstP->arrivalTime, &time);
        long int diff2 = time.tv_sec*1000000 + time.tv_usec;
        fprintf(stdout, "%08ld.%03ldms: p%ld departs from S%d, service time = %ld.%03ldms, time in system = %ld.%03ldms\n", pT/1000, pT%1000, firstP->id, serviceNumber, diff/1000, diff%1000, diff2/1000, diff2%1000);
        calculate(firstP);
        free(firstP);
        if(tokenGDead && My402ListEmpty(Q2)){
            pthread_mutex_unlock(&m);
            break;
        }
        pthread_mutex_unlock(&m);
    }
    return 0;
}

void commandLine(int argc, char * argv[]){
    /*if(argc % 2 == 0){
        printErr("wrong command, should be warmup2 [-lambda lambda] [-mu mu] [-r r] [-B B] [-P P] [-n num] [-t tsfile]");
    }*/
    for(int i = 1; i < argc; i += 2){
        if(i+1 >= argc){
            printErr("missing command, should be warmup2 [-lambda lambda] [-mu mu] [-r r] [-B B] [-P P] [-n num] [-t tsfile]");
        }
        if(!strcmp(argv[i], "-lambda")){
            checkDouble(argv[i+1], &lambda, "lambda");
        } else if (!strcmp(argv[i], "-mu")) {
            checkDouble(argv[i+1], &mu, "mu");
        } else if (!strcmp(argv[i], "-r")) {
            checkDouble(argv[i+1], &r, "r");
        } else if (!strcmp(argv[i], "-B")) {
            checkInt(argv[i+1], &B, "B");
        } else if (!strcmp(argv[i], "-P")) {
            checkInt(argv[i+1], &P, "P");
        } else if (!strcmp(argv[i], "-n") && !mode) {
            checkInt(argv[i+1], &num, "num");
        } else if (!strcmp(argv[i], "-t")){
            mode = 1;
            filename = argv[i+1];
            readFile(argv[i+1]);
        } else {
            printErr("wrong command, should be warmup2 [-lambda lambda] [-mu mu] [-r r] [-B B] [-P P] [-n num] [-t tsfile]");
        }
    }
    //printParameter();
}

void printErr(char * err) {
    fprintf(stderr, "Error: %s\n", err);
    exit(1);
}

void printParameter() {
    printf("lambda is : %.6g\n", lambda);
    printf("mu     is : %.6g\n", mu);
    printf("r      is : %.6g\n", r);
    printf("B      is : %ld\n", B);
    printf("P      is : %ld\n", P);
    printf("num    is : %ld\n", num);
}

void checkDouble(char * number, double * x, char * string){
    char * ptr;
    *x = strtod(number, &ptr);
    if(strlen(ptr) > 0 || *x <= 0){
        if(mode == 0){
            fprintf(stderr, "ERROR: %s is invalid format for %s, should be a positive real number\n",number ,string);
        }else if(mode == 1){
            fprintf(stderr, "ERROR: %s is invalid format for %s, should be a positive real number, at line %d\n",number , string, lineNumber);
        }
        exit(1);
    }
}

void checkInt(char * number, long int * x, char * string){
    char * ptr;
    *x = strtol(number, &ptr, 10);
    double y = strtod(number, NULL);
    if(strlen(ptr) > 0 || *x <= 0 || y > 2147483647){
        if(mode == 0){
            fprintf(stderr, "ERROR: %s is invalid format for %s, should be a positive int number and less than 2147483647\n",number, string);
        }else if(mode == 1){
            fprintf(stderr, "ERROR: %s is invalid format for %s, should be a positive int number and less than 2147483647, at line %d\n",number, string, lineNumber);
        }
        exit(1);
    }
}

void initial() {
    lambda = 1;    // packets per second
    mu = 0.35;     // packets per second
    r = 1.5;       // tokens per second
    B = 10;        // tokens
    P = 3;         // tokens;
    num = 20;      // packets
}

void printEmulationP(){
    fprintf(stdout, "Emulation Parameters:\n");
    fprintf(stdout, "\tnumber to arrive = %ld\n", num);
    if(!mode){
        fprintf(stdout, "\tlambda = %.6g\n", lambda);
        fprintf(stdout, "\tmu = %.6g\n", mu);
    }
    fprintf(stdout, "\tr = %.6g\n", r);
    fprintf(stdout, "\tB = %ld\n", B);
    if(!mode){
        fprintf(stdout, "\tP = %ld\n", P);
    }
    if(mode){
      fprintf(stdout, "\ttsfile = %s\n\n", filename);
    }
}

void calculateInterArrival(Packet* packet){
    averageInterArrival = (averageInterArrival * (packet->id-1) + packet->interArrivalTime / 1000.0) / (packet->id);
}

void calculate(Packet* packet){
    struct timeval time;
    timersub(&packet->sLeave, &packet->sBegin, &time);
    averageService = (averageService * totalNumLeft + time.tv_sec * 1000 + time.tv_usec / 1000.0) / (totalNumLeft + 1);
    timersub(&packet->q1Leave, &packet->q1Enter, &time);
    timeInQ1 += time.tv_sec * 1000 + time.tv_usec / 1000.0;
    timersub(&packet->q2Leave, &packet->q2Enter, &time);
    timeInQ2 += time.tv_sec * 1000 + time.tv_usec / 1000.0;
    timersub(&packet->sLeave, &packet->sBegin, &time);
    if(packet->serviceNumber == 1){
        timeInS1 += time.tv_sec * 1000 + time.tv_usec / 1000.0;
    }else if(packet->serviceNumber == 2){
        timeInS2 += time.tv_sec * 1000 + time.tv_usec / 1000.0;
    }
    timersub(&packet->sLeave, &packet->arrivalTime, &time);
    averageSystem = (averageSystem * totalNumLeft + time.tv_sec * 1000 + time.tv_usec / 1000.0) / (totalNumLeft + 1);
    sumSquareSystem += (time.tv_sec * 1000 + time.tv_usec / 1000.0) * (time.tv_sec * 1000 + time.tv_usec / 1000.0);
    
    totalNumLeft++;
}

void statistics(){
    fprintf(stdout, "Statistics:\n\n");
    if(packetGenerate == 0){
        fprintf(stdout, "\taverage packet inter-arrival time = N/A, no packet generated\n");
    }else{
        fprintf(stdout, "\taverage packet inter-arrival time = %.6g\n", averageInterArrival / 1000);
    }
    if(totalNumLeft == 0){
        fprintf(stdout, "\taverage packet service time = N/A, no packet completed\n\n");
    }else{
        fprintf(stdout, "\taverage packet service time = %.6g\n\n", averageService / 1000);
    }
    
    int long pT = parseTime(endTime);
    double averageQ1 = timeInQ1 / (pT / 1000.0);
    double averageQ2 = timeInQ2 / (pT / 1000.0);
    double averageS1 = timeInS1 / (pT / 1000.0);
    double averageS2 = timeInS2 / (pT / 1000.0);
    
    fprintf(stdout, "\taverage number of packets in Q1 = %.6g\n", averageQ1);
    fprintf(stdout, "\taverage number of packets in Q2 = %.6g\n", averageQ2);
    fprintf(stdout, "\taverage number of packets at S1 = %.6g\n", averageS1);
    fprintf(stdout, "\taverage number of packets at S2 = %.6g\n\n", averageS2);
    
    if(totalNumLeft == 0){
        fprintf(stdout, "\taverage time a packet spent in system = N/A, no packet completed\n");
    }else{
        fprintf(stdout, "\taverage time a packet spent in system = %.6g\n", averageSystem / 1000);
    }
    
    if(totalNumLeft == 0){
        fprintf(stdout, "\tstandard deviation for time spent in system = N/A, no packet completed\n\n");
    }else if(totalNumLeft == 1){
        fprintf(stdout, "\tstandard deviation for time spent in system = 0\n\n");
    }else{
        double standard = sqrt((sumSquareSystem / totalNumLeft) - averageSystem * averageSystem);
        fprintf(stdout, "\tstandard deviation for time spent in system = %.6g\n\n", standard / 1000);
    }
    
    if(tokenId == 0){
        fprintf(stdout, "\ttoken drop probability = N/A, no token generated\n");
    }else{
        double p1 = (double)tokenDrop / tokenId;
        fprintf(stdout, "\ttoken drop probability = %.6g\n", p1);
    }
    
    if(packetGenerate == 0){
        fprintf(stdout, "\tpacket drop probability = N/A, no packet generated\n");
    }else{
        double p2 = (double)packetDiscard / packetGenerate;
        fprintf(stdout, "\tpacket drop probability = %.6g\n", p2);
    }
    
}

void readFile(char * file){
    ssize_t read;
    size_t len = 0;
    char * line = NULL;
    if(opendir(file) != NULL){
        fprintf(stderr, "input file %s: is a directory or input file is not in the right format\n",file);
        exit(1);
    }
    fp = fopen(file, "r");
    if(fp == NULL){
        fprintf(stderr, "input file %s: %s\n",file, strerror(errno));
        exit(1);
    }
    if((read = getline(&line, &len, fp) != -1)){
        lineNumber++;
        if(strlen(line) > 1024){
            printErr("LINE CHARACTERS > 1024");
        }
        if(strlen(line) == 1 && line[0] == '\n'){
            printErr("Empty Line");
        }
    }else{
        printErr("Empty data");
    }
    char * token = strtok(line, " \t\n");
    checkInt(token, &num, "num");
    
    token = strtok(NULL, " \t");
    if(token != NULL){
        printErr("Number format is wrong, should only be a number at first line");
    }
}

void readLine(long int * interval, long int * tokenN, long int * serviceT){
    ssize_t read;
    size_t len = 0;
    char * line = NULL;
    if((read = getline(&line, &len, fp) != -1)){
        lineNumber++;
        if(strlen(line) > 1024){
            printErr("LINE CHARACTERS > 1024");
        }
        if(strlen(line) == 1 && line[0] == '\n'){
            printErr("Empty Line");
        }
    }else if(lineNumber <= num){
        printErr("missing data, too few data");
    }
    char * tokens[3];
    char * token = strtok(line, " \t");
    int count = 0;
    while (token != NULL && count < 3) {
        tokens[count++] = token;
        token = strtok(NULL, " \t\n");
    }
    if(count != 3 || token != NULL){
        printErr("STRING FAILS, SECTION != 3");
    }
    checkInt(tokens[0], interval, "inter-arrival-time");
    checkInt(tokens[1], tokenN, "tokens needed");
    checkInt(tokens[2], serviceT, "service time");
}

void * monitor(void * arg){
    int sig;
    struct timeval time;
    sigwait(&set, &sig);
    pthread_mutex_lock(&m);
    gettimeofday(&time, NULL);
    long int pT = parseTime(time);
    fprintf(stderr, "\n");
    fprintf(stderr, "%08ld.%03ldms: SIGINT caught, no new packets or tokens will be allowed\n", pT/1000, pT%1000);
    terminate = 1;
    pthread_cancel(t[0]);
    pthread_cancel(t[1]);
    pthread_cond_broadcast(&cv);
    pthread_mutex_unlock(&m);
    return 0;
}

void cleanUp(){
    struct timeval time;
    while(!My402ListEmpty(Q1)){
        My402ListElem * elem = My402ListFirst(Q1);
        Packet * package = (Packet * ) elem->obj;
        gettimeofday(&time, NULL);
        long int pT = parseTime(time);
        fprintf(stdout, "%08ld.%03ldms: p%ld removed from Q1\n", pT/1000, pT%1000, package->id);
        free(package);
        My402ListUnlink(Q1, elem);
    }
    free(Q1);
    while(!My402ListEmpty(Q2)){
        My402ListElem * elem = My402ListFirst(Q2);
        Packet * package = (Packet * ) elem->obj;
        gettimeofday(&time, NULL);
        long int pT = parseTime(time);
        fprintf(stdout, "%08ld.%03ldms: p%ld removed from Q2\n", pT/1000, pT%1000, package->id);
        free(package);
        My402ListUnlink(Q2, elem);
    }
    free(Q2);
}
