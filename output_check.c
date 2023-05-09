#include <stdio.h>
#include <string.h>

int main(){
    char* myoutput="000000000000000000000000011001000000000000000000000000001100100000010010001101000101011001111000";
    
    char* sampleoutput="000000000000000000000000011001000000000000000000000000001100100000010010001101000101011001111000";

    for(int i=0;strcmp(myoutput, sampleoutput)!=0;i++){
        int check=strcmp(myoutput, sampleoutput);
        printf("check%d: %d\n", i, check);
    }
    printf("\ncheck finish\n");
    return 0;
}

/*
memo
13th
19th
22th
*/