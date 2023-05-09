#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <ctype.h>
#include <errno.h>

/*
 * For debug option. If you want to debug, set 1.
 * If not, set 0.
 */
#define DEBUG 0

#define MAX_SYMBOL_TABLE_SIZE 1024
#define MEM_TEXT_START 0x00400000
#define MEM_DATA_START 0x10000000
#define BYTES_PER_WORD 4
#define INST_LIST_LEN 22

/******************************************************
 * Structure Declaration
 *******************************************************/

typedef struct inst_struct
{
    char *name;
    char *op;
    char type;
    char *funct;
} inst_t;

typedef struct symbol_struct
{
    char name[32];
    uint32_t address;
} symbol_t;

enum section
{
    DATA = 0,
    TEXT,
    MAX_SIZE
};

/******************************************************
 * Global Variable Declaration
 *******************************************************/

inst_t inst_list[INST_LIST_LEN] = {
    //  idx
    {"add", "000000", 'R', "100000"},  //    0
    {"sub", "000000", 'R', "100010"},  //    1
    {"addiu", "001001", 'I', ""},      //    2
    {"addu", "000000", 'R', "100001"}, //    3
    {"and", "000000", 'R', "100100"},  //    4
    {"andi", "001100", 'I', ""},       //    5
    {"beq", "000100", 'I', ""},        //    6
    {"bne", "000101", 'I', ""},        //    7
    {"j", "000010", 'J', ""},          //    8
    {"jal", "000011", 'J', ""},        //    9
    {"jr", "000000", 'R', "001000"},   //   10
    {"lui", "001111", 'I', ""},        //   11
    {"lw", "100011", 'I', ""},         //   12
    {"nor", "000000", 'R', "100111"},  //   13
    {"or", "000000", 'R', "100101"},   //   14
    {"ori", "001101", 'I', ""},        //   15
    {"sltiu", "001011", 'I', ""},      //   16
    {"sltu", "000000", 'R', "101011"}, //   17
    {"sll", "000000", 'R', "000000"},  //   18
    {"srl", "000000", 'R', "000010"},  //   19
    {"sw", "101011", 'I', ""},         //   20
    {"subu", "000000", 'R', "100011"}  //   21
};

symbol_t SYMBOL_TABLE[MAX_SYMBOL_TABLE_SIZE]; // Global Symbol Table

uint32_t symbol_table_cur_index = 0; // For indexing of symbol table

/* Temporary file stream pointers */
FILE *data_seg;
FILE *text_seg;

/* Size of each section */
uint32_t data_section_size = 0;
uint32_t text_section_size = 0;

/******************************************************
 * Function Declaration
 *******************************************************/

/* Change file extension from ".s" to ".o" */
char *change_file_ext(char *str)
{
    char *dot = strrchr(str, '.');

    if (!dot || dot == str || (strcmp(dot, ".s") != 0))
        return NULL;

    str[strlen(str) - 1] = 'o';
    return "";
}

/* Add symbol to global symbol table */
void symbol_table_add_entry(symbol_t symbol)
{
    SYMBOL_TABLE[symbol_table_cur_index++] = symbol;
#if DEBUG
    printf("%s: 0x%08x\n", symbol.name, symbol.address);
#endif
}

/* Convert integer number to binary string */
char *num_to_bits(unsigned int num, int len)
{
    char *bits = (char *)malloc(len + 1);
    int idx = len - 1, i;
    while (num > 0 && idx >= 0)
    {
        if (num % 2 == 1)
        {
            bits[idx--] = '1';
        }
        else
        {
            bits[idx--] = '0';
        }
        num /= 2;
    }
    for (i = idx; i >= 0; i--)
    {
        bits[i] = '0';
    }
    bits[len] = '\0';
    return bits;
}

/* Record .text section to output file */
void record_text_section(FILE *output)
{
    uint32_t cur_addr = MEM_TEXT_START;
    char line[1024];

    /* Point to text_seg stream */
    rewind(text_seg);

    /* Print .text section */
    while (fgets(line, 1024, text_seg) != NULL)
    {
        char inst[0x1000] = {0};
        char op[32] = {0};
        char label[32] = {0};
        char type = '0';
        int i, idx = 0;
        int rs, rt, rd, imm, shamt;
        int addr;

        rs = rt = rd = imm = shamt = addr = 0;
#if DEBUG
        printf("0x%08x: ", cur_addr);
#endif
        /* Find the instruction type that matches the line */
        char _line[1024] = {0};
        strcpy(_line, line);                 // line 문자열을 _line 배열로 복사
        char *temp = strtok(_line, " \t\n"); // _line 문자열을 " \t\n" 구분자를 기준으로 분리하여 첫 번째 토큰의 주소를 temp 포인터에 저장

        while (idx < INST_LIST_LEN && type == '0') // idx 변수가 INST_LIST_LEN 이하이고 type 변수가 '0'일 때까지 반복
        {
            if (!strcmp(temp, inst_list[idx].name)) // temp 포인터가 가리키는 문자열과 inst_list[idx] 구조체의 name 멤버가 같은지 비교
            {
                type = inst_list[idx].type; // 같다면 inst_list[idx] 구조체의 type 멤버를 type 변수에 저장
            }
            idx++; // idx 변수를 1 증가
        }
        idx--; // idx 변수를 1 감소

        switch (type)
        {
        case 'R':
            /* blank */
            strcpy(op, inst_list[idx].op); // inst_list[idx] 구조체의 op 멤버를 op 문자열에 복사합니다.

            if (!strcmp(temp, "jr")) // temp 포인터가 가리키는 문자열이 "jr"인 경우
            {
                temp = strtok(NULL, "$,\t\n"); // 다음 토큰을 읽어들이기 위해 temp 포인터를 이동시킵니다.
                rs = atoi(temp);               // temp 포인터가 가리키는 문자열을 정수로 변환하여 rs 변수에 저장합니다.
            }
            else if (!strcmp(temp, "sll") || !strcmp(temp, "srl"))
            {
                temp = strtok(NULL, "$,\t\n");  // 다음 토큰을 읽어들이기 위해 temp 포인터를 이동시킵니다.
                rd = atoi(temp);                // temp 포인터가 가리키는 문자열을 정수로 변환하여 rd 변수에 저장합니다.
                temp = strtok(NULL, "$\t, \n"); // 다음 토큰을 읽어들이기 위해 temp 포인터를 이동시킵니다.
                rt = atoi(temp);                // temp 포인터가 가리키는 문자열을 정수로 변환하여 rt 변수에 저장합니다.
                temp = strtok(NULL, "$\t, \n"); // 다음 토큰을 읽어들이기 위해 temp 포인터를 이동시킵니다.
                shamt = atoi(temp);             // temp 포인터가 가리키는 문자열을 정수로 변환하여 shamt 변수에 저장합니다.
            }
            else // 위의 경우가 아닌 경우
            {
                temp = strtok(NULL, "$\t, \n");  // 다음 토큰을 읽어들이기 위해 temp 포인터를 이동시킵니다.
                rd = atoi(temp);                 // temp 포인터가 가리키는 문자열을 정수로 변환하여 rd 변수에 저장합니다.
                temp = strtok(NULL, "$,\t, \n"); // 다음 토큰을 읽어들이기 위해 temp 포인터를 이동시킵니다.
                rs = atoi(temp);                 // temp 포인터가 가리키는 문자열을 정수로 변환하여 rs 변수에 저장합니다.
                temp = strtok(NULL, "$,\t \n");  // 다음 토큰을 읽어들이기 위해 temp 포인터를 이동시킵니다.
                rt = atoi(temp);                 // temp 포인터가 가리키는 문자열을 정수로 변환하여 rt 변수에 저장합니다.
            }
            fprintf(output, "%s%s%s%s%s%s", op, num_to_bits(rs, 5),
                    num_to_bits(rt, 5), num_to_bits(rd, 5), num_to_bits(shamt, 5), inst_list[idx].funct);
            /* blank */
#if DEBUG
            printf("op:%s rs:$%d rt:$%d rd:$%d shamt:%d funct:%s\n",
                   inst_list[idx].name, rs, rt, rd, shamt, inst_list[idx].funct);
#endif
            break;

        case 'I': // temp == op인 상태
            /* blank */
            if (!strcmp(temp, "lui"))
            {
                temp = strtok(NULL, "$,\t\n");
                rt = atoi(temp);
                temp = strtok(NULL, "$, \t\n");
                imm = strtol(temp, NULL, (strchr(temp, 'x') != NULL) ? 16 : 10);
            }
            else if (!strcmp(temp, "beq") || !strcmp(temp, "bne"))
            {
                temp = strtok(NULL, "$, \t\n");
                rs = atoi(temp);
                temp = strtok(NULL, "$,\t \n");
                rt = atoi(temp);

                temp = strtok(NULL, "$, \t\n");
                for (i = 0; i < MAX_SYMBOL_TABLE_SIZE; i++)
                {
                    if (!strcmp(temp, SYMBOL_TABLE[i].name))
                    {
                        imm = ((SYMBOL_TABLE[i].address - cur_addr - 4)) / 4;
                        break;
                    }
                }
            }
            else if ((!strcmp(temp, "lw")) || (!strcmp(temp, "sw")))
            {

                temp = strtok(NULL, "$, \t\n");
                rt = atoi(temp);
                temp = strtok(NULL, "$\t,\n)(");

                imm = atoi(temp);
                temp = strtok(NULL, "$,\t\n)(");
                rs = atoi(temp);
            }
            else
            {
                temp = strtok(NULL, "$, \t\n");
                rt = atoi(temp);
                temp = strtok(NULL, "$, \t\n");
                rs = atoi(temp);
                temp = strtok(NULL, "$, \t\n");
                imm = strtol(temp, NULL, (strchr(temp, 'x') != NULL) ? 16 : 10);
            }

            fprintf(output, "%s%s%s%s", inst_list[idx].op, 
            num_to_bits(rs, 5), num_to_bits(rt, 5), num_to_bits(imm, 16));

            /* blank */
#if DEBUG
            printf("op:%s rs:$%d rt:$%d imm:0x%x\n",
                   inst_list[idx].name, rs, rt, imm);
#endif
            break;

        case 'J':
            /* blank */

            temp = strtok(NULL, " \t\n");
            for (i = 0; i < symbol_table_cur_index; i++)
            {
                if (!strcmp(temp, SYMBOL_TABLE[i].name))
                    break;
            }
            addr = (int)SYMBOL_TABLE[i].address >> 2;

            // 출력
            fprintf(output, "%s%s", inst_list[idx].op, num_to_bits(addr, 26));
#if DEBUG
            printf("op:%s addr:%i\n", op, addr);
#endif
            break;

        default:
            break;
        }
        fprintf(output, "\n");

        cur_addr += BYTES_PER_WORD;
    }
}

/* Record .data section to output file */
void record_data_section(FILE *output)
{
    uint32_t cur_addr = MEM_DATA_START;
    char line[1024];

    /* Point to data segment stream */
    rewind(data_seg);

    /* Print .data section */
    while (fgets(line, 1024, data_seg) != NULL)
    {
        /* blank */
        char *temp = strtok(line, " \t\n"); // temp == .word
        temp = strtok(NULL, " \t\n");       // ex) temp == 1, 2, 0x100

        uint32_t num = 0;
        char *endptr;
        num = strtoul(temp, &endptr, 0);

        char *bits = num_to_bits(num, 32);
        fputs(bits, output);
        fputs("\n", output);

#if DEBUG
        printf("0x%08x: ", cur_addr);
        printf("%s", line);
#endif
        cur_addr += BYTES_PER_WORD;
    }
}

/* Fill the blanks */
void make_binary_file(FILE *output)
{
#if DEBUG
    char line[1024] = {0};
    rewind(text_seg);
    /* Print line of text segment */
    while (fgets(line, 1024, text_seg) != NULL)
    {
        printf("%s", line);
    }
    printf("text section size: %d, data section size: %d\n",
           text_section_size, data_section_size);
#endif

    /* Print text section size and data section size */
    /* blank */
    fprintf(output, "%s\n%s\n", num_to_bits(text_section_size, 32), num_to_bits(data_section_size, 32));
    /* Print .text section */
    record_text_section(output);

    /* Print .data section */
    record_data_section(output);
}

/* Fill the blanks */
void make_symbol_table(FILE *input)
{
    char line[1024] = {0};
    uint32_t address = 0;
    enum section cur_section = MAX_SIZE;

    /* Read each section and put the stream */

    while (fgets(line, 1024, input) != NULL)
    {
        char *temp;
        char _line[1024] = {0};
        strcpy(_line, line);
        temp = strtok(_line, "\t\n");
        symbol_t symbolTmp;
        char label[32];
        int r = 0;

        if (!strcmp(temp, ".data"))
        {
            /* blank */
            cur_section = DATA;
            address = MEM_DATA_START;
            /* blank */
            data_seg = tmpfile();
            continue;
        }
        else if (!strcmp(temp, ".text"))
        {
            /* blank */
            cur_section = TEXT;
            address = MEM_TEXT_START;
            /* blank */
            text_seg = tmpfile();
            continue;
        }
        if (cur_section == DATA)
        {
            /* blank */
            if (temp[strlen(temp) - 1] == ':')
            {
                symbol_t data;
                strncpy(data.name, temp, strlen(temp) - 1);
                data.name[strlen(temp) - 1] = '\0';
                data.address = address;
                symbol_table_add_entry(data);
                fputs(line + strlen(temp), data_seg);
            }
            else
            {
                fputs(line, data_seg);
            }
            data_section_size += BYTES_PER_WORD;
            /* blank */
        }
        else if (cur_section == TEXT)
        {
            /* blank */
            if (temp[strlen(temp) - 1] == ':')
            {
                char symbol_name[30];
                strncpy(symbol_name, temp, strlen(temp) - 1);
                symbol_name[strlen(temp) - 1] = '\0';
                SYMBOL_TABLE[symbol_table_cur_index].address = address;
                strcpy(SYMBOL_TABLE[symbol_table_cur_index].name, symbol_name);
                symbol_table_cur_index++;
                continue;
            }
            else if (!strcmp(temp, "la"))
            {
                int r = 0;
                char label[30] = {NULL};
                sscanf(line, "%*s $%d, %s", &r, label);
                for (int i = 0; i < symbol_table_cur_index; i++)
                {
                    if (!strcmp(SYMBOL_TABLE[i].name, label))
                    {
                        uint32_t label_address = SYMBOL_TABLE[i].address;
                        uint16_t upper_label_address = label_address >> 16;
                        if (upper_label_address != 0)
                        {
                            fprintf(text_seg, "lui $%d, 0x%x\n", r, upper_label_address);
                            text_section_size += BYTES_PER_WORD;
                        }
                        uint16_t lower_label_address = label_address & 0xffff;
                        if (lower_label_address != 0)
                        {
                            fprintf(text_seg, "ori $%d, $%d, 0x%x\n", r, r, lower_label_address);
                            text_section_size += BYTES_PER_WORD;
                            address += BYTES_PER_WORD;
                        }
                        break;
                    }
                }
            }
            else
            {
                fputs(line, text_seg);
                text_section_size += BYTES_PER_WORD;
            }
        }
        /* blank */

        address += BYTES_PER_WORD;
    }
}

int main(int argc, char *argv[])
{
    FILE *input, *output;
    char *filename;

    if (argc != 2)
    {
        fprintf(stderr, "Usage: %s <*.s>\n", argv[0]);
        fprintf(stderr, "Example: %s sample_input/example?.s\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    /* Read the input file */
    input = fopen(argv[1], "r");
    if (input == NULL)
    {
        perror("ERROR");
        exit(EXIT_FAILURE);
    }

    /* Create the output file (*.o) */
    filename = strdup(argv[1]); // strdup() is not a standard C library but fairy used a lot.
    if (change_file_ext(filename) == NULL)
    {
        fprintf(stderr, "'%s' file is not an assembly file.\n", filename);
        exit(EXIT_FAILURE);
    }

    output = fopen(filename, "w");
    if (output == NULL)
    {
        perror("ERROR");
        exit(EXIT_FAILURE);
    }

    /******************************************************
     *  Let's complete the below functions!
     *
     *  make_symbol_table(FILE *input)
     *  make_binary_file(FILE *output)
     *  ├── record_text_section(FILE *output)
     *  └── record_data_section(FILE *output)
     *
     *******************************************************/
    make_symbol_table(input);
    make_binary_file(output);

    fclose(input);
    fclose(output);

    return 0;
}