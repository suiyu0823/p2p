/****************************************************
 data_structs.h
 Database struct prototypes and implementations.
*****************************************************/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define MAX 100
#define YES 1
#define NO  0
typedef int bool;


/*============================
 Define basic data structures
 =============================*/
struct pdu
{
    char type;
	char peer[20];
	char content[20];
	char address[20];
	char port[40];
};
typedef struct pdu frame;

struct db
{
    char file_name[20];
    frame *file_ref;
    int ref_ptr;
};
typedef struct db database;
