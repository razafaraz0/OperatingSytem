#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h> // for close
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <errno.h>
#include <sys/stat.h>
#include <semaphore.h>
#include <string.h>
#include "linkedList.c"


#define NUM_THREADS 10
#define BUFFSIZE 100
#define SANE_LEN 128
#define CREATE_SEM 1
#define OPEN_SEM 0
#define RES_SEM_SUFFIX_LEN 11

char* sem_name;

typedef enum{FREE, PROCESSED, PROCESSING} queue_state;

struct request {
	char keyword[SANE_LEN];
	int index;
};

struct shared_data
{
	queue_state state_queue[NUM_THREADS];
	struct request request_queue[NUM_THREADS];
	int result_queue[NUM_THREADS][BUFFSIZE];

	int in[NUM_THREADS];
};

void* openSharedMemSeg(char* shm_name)
{
	int shm_fd;
	void *shm_start; //same as ptr
 	struct stat sbuf; /* info about shm */
	struct shared_data *sdptr;
	char* p;

	shm_fd = shm_open(shm_name, O_RDWR, 0600);

	if(shm_fd < 0)
	{
	 	printf("shared memory failed\n"); 
	 	exit(1); 
	}
	
	printf("shm open success, fd = %d.\n", shm_fd);
	
	fstat(shm_fd, &sbuf);
	printf("shm size = %d\n", (int) sbuf.st_size);
	
	shm_start = mmap(NULL, sbuf.st_size, PROT_READ | PROT_WRITE ,MAP_SHARED, shm_fd, 0);
 	
 	if (shm_start < 0) {
		 perror("can not map the shm \n");
		exit (1);
 	}

	printf("mapped shm; start_address=%u.\n", (unsigned int) shm_start);	
	close(shm_fd);

	sdptr = (struct shared_data *) shm_start;
	return sdptr;
}


sem_t* createOrOpenSemaphore(char* sem_name, int create, int init_val)
{
	sem_t *sem;

	if( create == 1){
		sem_unlink(sem_name);
	   	sem = sem_open(sem_name, O_RDWR | O_CREAT, 0660, init_val);
	}
	else if ( create == 0) sem = sem_open(sem_name, O_RDWR);

	if (sem < 0)
	{
		perror("Cannot create Semaphore.\n");
		exit(1);
	}

	printf("sem %s opened.\n", sem_name);

	return sem;
}

void sanityCheck(char* keyword, char* mem_name, char* sem_name_suffix)
{
	if( strlen(keyword) > SANE_LEN) {
		printf("Filename should be less than %d characters.", SANE_LEN);
		exit(1);
	}
	if (strlen(mem_name) > SANE_LEN){
		printf("Name of the memory segment should be less than %d characters.", SANE_LEN);
		exit(1);
	}
	if (strlen(sem_name_suffix) > SANE_LEN)
	{
		printf( "The suffix of semaphore names should be less than %d characters.", SANE_LEN);
		exit(1);
	}
}

void* generateSemName(char* full_sem_name, char* sem_type, int index)
{
	//Suffix
	char intStr[12];
	sprintf(intStr, "%d", index);
	char suffix[RES_SEM_SUFFIX_LEN] = "_resQueue_";
	strcat(suffix, intStr);

	strcpy(full_sem_name, sem_name);
   	strcat(full_sem_name, sem_type);
	strcat(full_sem_name, suffix);
}

void addResultToFile(char* keyword)
{
	FILE *fp;

	int res_file_prefix = 11;
	char res_filename[res_file_prefix + SANE_LEN];

	strcpy(res_filename, "client_res_");
	strcat(res_filename, keyword);

	fp = fopen(res_filename, "a");

	if (fp == NULL)
	{
		printf("\n\nCOULD NOT OPEN RESULT FILE %s\nPrint output to terminal.\n", res_filename);
		printall();
		return;
	}

	fprintf(fp, "Occurence of keyword: ");
	fprintf(fp, keyword);
	fputc(10, fp);
    fputc(10, fp);	//put newline.

	while (!isEmpty())
	{
		char intStr[12];
		sprintf(intStr, "%d", getNext());

		fprintf(fp, "Keyword found on line: ");
		fprintf(fp, intStr);
		fputc(10, fp);
	}

	fclose(fp);

	printf("\n\n******************************\n");
	printf("Results added to appropriate files for \"%s\"\n\n", keyword);
}

int main(int argc , char *argv[])
{	
	char full_sem_name[SANE_LEN + RES_SEM_SUFFIX_LEN + 5];

	char* shm_name = argv[1];
	char *keyWord = argv[2];
	sem_name = argv[3];

	sanityCheck(keyWord, shm_name, sem_name);

	sem_t *sem_mutex;
	sem_t *sem_empty;
	sem_t *sem_full;

	sem_t *sem_mutex_result;
	sem_t *sem_empty_result;
	sem_t *sem_full_result;

	//semaphore mutex name
	char mutex_name[strlen(sem_name) + strlen("_mutex")];
	strcpy(mutex_name, sem_name);
	strcat(mutex_name, "_mutex");
	sem_mutex = createOrOpenSemaphore(mutex_name, OPEN_SEM, -1);

	//smeaphore empty name
	char empty_name[strlen(sem_name) + strlen("_empty")];
	strcpy(empty_name, sem_name);
	strcat(empty_name, "_empty");
	sem_empty = createOrOpenSemaphore(empty_name, OPEN_SEM, -1);

	//smeaphore full name
	char full_name[strlen(sem_name) + strlen("_full")];
	strcpy(full_name, sem_name);
	strcat(full_name, "_full");
	sem_full = createOrOpenSemaphore(full_name, OPEN_SEM, -1);

	int shm_size = sizeof(struct shared_data);
	struct shared_data* shm_start;

	//open shared memory
	shm_start = openSharedMemSeg(shm_name);

	

	sem_wait(sem_empty);
	sem_wait(sem_mutex);

	int free_res_queue;
	for(int i = 0 ; i <= NUM_THREADS ; ++i)
	{
		if(i == NUM_THREADS)
		{
			printf("Too many clients started\n");
			sem_post(sem_mutex);
			sem_post(sem_empty);
			exit(1);
		}
		
		if ( shm_start->state_queue[i] == FREE)
		{
			free_res_queue = i;

			shm_start->state_queue[i] = PROCESSING;

			
			/* *** PROCESSING RESULT Semaphores *** */
			//SEM NAMES
	
			//MUTEX SEM
			generateSemName( &full_sem_name, "_mutex", free_res_queue);
			sem_mutex_result = createOrOpenSemaphore( full_sem_name, CREATE_SEM, 1);

			//FULL SEM
			generateSemName( &full_sem_name, "_full", free_res_queue);
			sem_full_result = createOrOpenSemaphore( full_sem_name, CREATE_SEM, 0);

			//EMPTY SEM
			generateSemName( &full_sem_name, "_empty", free_res_queue);
			sem_empty_result = createOrOpenSemaphore( full_sem_name, CREATE_SEM, BUFFSIZE);


			for ( int j = 0; j < NUM_THREADS; ++j)
			{
				if ( shm_start->request_queue[j].index == -1)
				{
					shm_start->state_queue[free_res_queue] = PROCESSING;

					struct request rq;
					strcpy(rq.keyword, keyWord);
					rq.index = free_res_queue;

					memcpy( &shm_start->request_queue[j], &rq, sizeof(struct request));	

					break;
				}
			}
			break;
		}
	}
	
	sem_post(sem_mutex);
	sem_post(sem_full);

	//lock
	//char lock_name[strlen(sem_name) + strlen("_lock")];
	//strcpy(lock_name, sem_name);
	//strcat(lock_name, "_lock");
	//lock = createOrOpenSemaphore(lock_name, OPEN_SEM, -1);

	//synchronize semaphore creation.

	/* 
	//SEM NAMES
	
	//MUTEX SEM
	generateSemName( &full_sem_name, "_mutex", free_res_queue);
	sem_mutex_result = createOrOpenSemaphore( full_sem_name, OPEN_SEM, -1);

	//FULL SEM
	generateSemName( &full_sem_name, "_full", free_res_queue);
	sem_full_result = createOrOpenSemaphore( full_sem_name, OPEN_SEM, -1);

	//EMPTY SEM
	generateSemName( &full_sem_name, "_empty", free_res_queue);
	sem_empty_result = createOrOpenSemaphore( full_sem_name, OPEN_SEM, -1); */

	printf("\nBeginning RESULT PROCESSING.\n");

	int val;
	int out = 0;
	while(1)
	{
		/* Check if all results processed */
		sem_wait(sem_mutex_result);

		if ( shm_start->state_queue[free_res_queue] == PROCESSED && shm_start->in[free_res_queue] == out)
		{
			shm_start->state_queue[free_res_queue] = FREE;

			sem_post(sem_mutex_result);
			break;
		}

		sem_post(sem_mutex_result);

		/* If not then add then traverse queue and add them to linked list */

//		printf("Stuck at full? %s\n", keyWord);
		sem_wait(sem_full_result);
//		printf("Nope for %s\n", keyWord);
		sem_wait(sem_mutex_result);

		if (out == BUFFSIZE) out = 0;	

		val = shm_start->result_queue[free_res_queue][out];

		if (val != -1)
		{
			//read line number and add to linked list.
			add(val);
	
			shm_start->result_queue[free_res_queue][out] = -1;

			out = out + 1;	

			sem_post(sem_empty_result);
		}

		sem_post(sem_mutex_result);
	}

//	printf("\n\n***************\n");
//	printf("Processed keyword: %s\n\n", keyWord);

	sem_close(sem_mutex);
	sem_close(sem_full);
	sem_close(sem_empty);

	sem_close(sem_mutex_result);
	sem_close(sem_full_result);
	sem_close(sem_empty_result);

	addResultToFile(keyWord);

//	sem_unlink(sem_mutex_result);
//	sem_unlink(sem_empty_result);
//	sem_unlink(sem_full_result);

	return 0;
}
