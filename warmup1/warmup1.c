#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <errno.h>
#include <dirent.h>

#include "cs402.h"
#include "my402list.h"



/**
 struct myData: used to store data from each line.
 */
typedef struct myData{
    char type;
    long time;
    int amount;
    char description[25];
}Data;

void printErr(char * error);
My402List* readFile(char * file);
int addToList(My402List* list, Data * data);
Data* parseLine(char* line);

void parseType(Data * data, char * token);
void parseTime(Data * data, char * token);
void parseAmount(Data * data, char * token);
void parseDescription(char * des, char * token);

void printList(My402List* list);
void printDash(void);
void printWords(void);
void output(My402List* list);

void formatDate(time_t time, char * date);
void formatDescription(char * source, char * description);
void formatAmout(char * amount, int money, char type);
void formatBalance(char * balance, int money, char type, long * total);

void exceedTenMillion(char * balance, char type);

int lineNumber = 0;
/**
 main function: used to check the input format. Run readFile and output to generate the output.
 argc: number of arguments.
 argv[]: intput char array.
 */
int main(int argc, char * argv[]){
    char * file = NULL;
    if(argc < 2 || strcmp("sort", argv[1])){
        fprintf(stderr, "malformed commandline - incorrect number of commandline arguments\nusage: warmup1 sort [tfile]\n");
        exit(1);
    }
    if(argc == 2){
        file = NULL;
    }else if(argc == 3){
        file = argv[2];
    }else{
        fprintf(stderr, "malformed commandline - incorrect number of commandline arguments\nusage: warmup1 sort [tfile]\n");
        exit(1);
    }
    My402List * list = readFile(file);
    output(list);
}

/**
 readFile: read the file line by line and call parseLine to check the data, addToList to store the data element.
 file: string of input file name.
 return: My402List *.
 */
My402List * readFile(char * file){
    My402List* list = malloc(sizeof(My402List));
    ssize_t read;
    size_t len = 0;
    FILE * fp;
    char * line = NULL;
    if(file == NULL){
        fp = stdin;
    }else{
        if(opendir(file) != NULL){
            fprintf(stderr, "input file %s: is a directory or input file is not in the right format\n",file);
            exit(1);
        }
        fp = fopen(file, "r");
        if(fp == NULL){
            fprintf(stderr, "input file %s: %s\n",file, strerror(errno));
            exit(1);
        }
    }
    while((read = getline(&line, &len, fp) != -1)){
        lineNumber++;
        if(strlen(line) > 1024){
            printErr("ERROR: LINE CHARACTERS > 1024");
        }
        if(strlen(line) == 1 && line[0] == '\n'){
            break;
        }
        Data* data = parseLine(line);
        addToList(list, data);
    }
    fclose(fp);
    return list;
}

/**
 printErr: print error message and exit.
 error: error message in char array.
 */
void printErr(char * error){
    fprintf(stderr, "%s, Line: %d\n", error, lineNumber);
    exit(1);
}

/**
 addToList: add data element to our list.
 list: target list that used to store data.
 data: Data stored info from each line.
 return true if succeed, otherwise, false.
 */
int addToList(My402List* list, Data * data){
    long time = data->time;
    if(My402ListEmpty(list)){
        My402ListAppend(list, data);
        return TRUE;
    }
    My402ListElem * first = My402ListFirst(list);
    My402ListElem * last = My402ListLast(list);
    if(time == ((Data*)(first->obj))->time || time  == ((Data*)(last->obj))->time){
        printErr("ERROR: TIME EQUALS");
    }
    if(time <  ((Data*)(first->obj))->time){
        My402ListPrepend(list, data);
    }else if(time >  ((Data*)(last->obj))->time){
        My402ListAppend(list, data);
    }else{
        while(first != NULL){
            if(time <  ((Data*)(first->obj))->time){
                My402ListInsertBefore(list, data, first);
                break;
            }else if(time == ((Data*)(first->obj))->time){
                printErr("ERROR: TIME EQUALS");
            }
            first = My402ListNext(list, first);
        }
    }
    return TRUE;
}

/**
 parseLine: parse info from each line.
 line: string from each line.
 return Data * type that stored the info.
 */
Data* parseLine(char* line){
    char * tokens[4];
    Data* data = malloc(sizeof(Data));
    char * token = strtok(line, "\t");
    int count = 0;
    while (token != NULL && count < 4) {
        tokens[count++] = token;
        token = strtok(NULL, "\t");
    }
    if(count != 4 || token != NULL){
        printErr("ERROR: STRING FAILS, SECTION != 4");
    }
    parseType(data, tokens[0]);
    parseTime(data, tokens[1]);
    parseAmount(data, tokens[2]);
    parseDescription(data->description, tokens[3]);
    return data;
}

/**
 parseType: check whether the first char is '+' or '-'.
 data: Data stored info from each line.
 token: piece of string from specific line.
 */
void parseType(Data * data, char * token){
    if(strlen(token) > 1){
        printErr("ERROR: TYPE IS TOO LONG, SHOULD BE '+' OR '-'");
    }
    if(token[0] == '+' || token[0] == '-'){
        data->type = token[0];
    }else{
        printErr("ERROR: TYPE IS WRONG, SHOULD BE '+' OR '-'");
    }
}

/**
 parseTime: check whether the token is satisfied the format of time.
 data: Data stored info from each line.
 token: piece of string from specific line.
 */
void parseTime(Data * data, char * token){
    long now = time(NULL);
    if(strlen(token) > 10){
        printErr("ERROR: TIME FORMAT ERROR, TOO MANY CHARACTERS");
    }else{
        for(int i = 0; i < strlen(token); i++){
            if(token[i] > '9' || token[i] < '0'){
                printErr("ERROR: TIME FORMAT ERROR, CONTAINS NON-DIGITS");
            }
        }
        data->time = strtoul(token, NULL, 10);
        if(data->time >= now || data->time < 0){
            printErr("ERROR: TIME EXCEEDS CURRENT TIME LIMITS");
        }
    }
}

/**
 parseAmount: check whether the token is satisfied the format of amount.
 data: Data stored info from each line.
 token: piece of string from specific line.
 */
void parseAmount(Data * data, char * token){
    if(strlen(token) > 10){
        printErr("ERROR: AMOUNT HAS TOO MANY DIGITS");
    }else if(strlen(token) > 4 && token[0] == '0'){
        printErr("ERROR: AMOUNT FORMAT ERROR, A NUMBER CANNOT START WITH '0'");
    }
    int dotNumber = 0;
    for(int i = 0; i < strlen(token); i++){
        if(token[i] == '.' && i == strlen(token) - 3){
            dotNumber++;
        }else if(token[i] < '0' || token[i] > '9'){
            printErr("ERROR: WRONG AMOUNT FORMAT, CONTAINS NON-DIGIT CHARACTER or THE POSITION OF PERIOD IS WRONG\nCORRECT FORMAT: ???????.??");
        }
    }
    if(dotNumber != 1){
         printErr("ERROR: WRONG AMOUNT FORMAT, WRONG NUMBER OF DOTS");
    }
    token[strlen(token) - 3] = token[strlen(token) - 2];
    token[strlen(token) - 2] = token[strlen(token) - 1];
    token[strlen(token) - 1] = '.';
    int k = atoi(token);
    if(k == 0){
        printErr("ERROR: AMOUNT CANNOT BE ZERO");
    }
    data->amount = k;
}

/**
 parseDescription: copy the string from token to description.
 des: char* of description.
 token: piece of string from specific line.
 */
void parseDescription(char * des, char * token){
    if(strlen(token) == 1 && token[0] == '\n'){
        printErr("ERROR: EMPTY DESCRIPTION");
    }
    for(int i = 0; i < 24; i++){
        des[i] = token[i];
    }
    des[24] = '\0';
}

/**
 printList: test function to print the raw data.
 */
void printList(My402List* list){
    if(My402ListEmpty(list)){
        fprintf(NULL, "empty");
    }
    My402ListElem* elem = My402ListFirst(list);
    while(elem != &(list->anchor)){
        Data* data = elem->obj;
        printf("TYPE: %c, TIME: %ld, AMOUNT: %d, DES: %s\n", data->type, data->time, data->amount, data->description);
        elem = elem->next;
    }
}

/**
 output: print the data in desired form.
 list: list stored the data element.
 */
void output(My402List* list){
    printDash();
    printWords();
    printDash();
    My402ListElem* elem;
    long total = 0;
    for(elem = My402ListFirst(list); elem != NULL; elem = My402ListNext(list, elem)){
        Data * data = elem->obj;
        char date[16], description[25], amount[15], balance[15];
        formatDate(data->time, date);
        formatDescription(data->description, description);
        formatAmout(amount, data->amount, data->type);
        formatBalance(balance, data->amount, data->type, &total);
        printf("| %15s | %-24s | %14s | %14s |\n", date, description, amount, balance);
    }
    printDash();
}

/**
 printDash.
 */
void printDash(){
    printf("+-----------------+--------------------------+----------------+----------------+\n");
}

/**
 printWords.
 */
void printWords(){
    printf("|       Date      | Description              |         Amount |        Balance |\n");
}

/**
 formatDate: format the date in desired form.
 time: 10 digits int used to tranfer to Date.
 date: char* date.
 */
void formatDate(time_t time, char * date){
    char buf[26];
    strncpy(buf, ctime(&time), sizeof(buf));
    for(int i = 0; i < 11; i++){
        date[i] = buf[i];
    }
    for(int i = 11; i < 15; i++){
        date[i] = buf[i+9];
    }
    date[15] = '\0';
}

/**
 formatDate: format the date in desired form.
 time: 10 digits int used to tranfer to Date.
 date: char*  of date.
 */
void formatDescription(char * source, char * description){
    int i = 0;
    while(source[i] != '\0' && i < strlen(source) && source[i] != '\n'){
        description[i] = source[i];
        i++;
    }
    description[i] = '\0';
}

/**
 formatAmount: format the amount in desired form.
 money: 100 times of origianl amount.
 amount: char*  of amount.
 type: '+' or '-'.
 */
void formatAmout(char * amount, int money, char type){
    int i = 14;
    amount[i--] = '\0';
    if(type == '-'){
        amount[0] = '(';
        amount[13] = ')';
    }else{
        amount[0] = ' ';
        amount[13] = ' ';
    }
    i--;
    if(money == 0){
        amount[12] = '0';
        amount[11] = '0';
        i -= 2;
    }
    while(money > 0 && i > 0){
        if(i == 10){
            amount[i--] = '.';
        }else if(i == 6 || i == 2){
            amount[i--] = ',';
        }else{
            amount[i--] = (char) (money % 10 + '0');
            money /= 10;
        }
    }
    if(i == 11){
        amount[i--] = '0';
    }
    if(i == 10){
        amount[10] = '.';
        amount[9] = '0';
        i -= 2;
    }
    while(i > 0){
        amount[i--] = ' ';
    }
}

/**
 formatBalance format the amount in desired form.
 money: 100 times of origianl amount.
 balance: char*  of balance.
 type: '+' or '-'.
 total: 100 times of original balance.
 */
void formatBalance(char * balance, int money, char type, long * total){
    if(type == '+'){
        *total += money;
    }else{
        *total -= money;
    }
    long limit = 1000000000;
    if(*total >= limit){
        exceedTenMillion(balance, '+');
        return;
    }else if(-*total >= limit){
        exceedTenMillion(balance, '-');
        return;
    }
    if(*total >= 0){
        formatAmout(balance, (int)*total, '+');
    }else{
        formatAmout(balance, (int)-*total, '-');
    }
}

/**
 exceedTenMillion: specific format for balance when it exceeds certain limit.
 */
void exceedTenMillion(char * balance, char type){
    char* amount = "";
    if(type == '+'){
        amount = " \?,\?\?\?,\?\?\?.\?\? ";
    }else if(type == '-'){
        amount = "(\?,\?\?\?,\?\?\?.\?\?)";
    }
    for(int i = 0; i < 14; i++){
        balance[i] = amount[i];
    }
    balance[14] = '\0';
}
