#define _GNU_SOURCE

#include <signal.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <errno.h>
#include <sys/stat.h>
#include <semaphore.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#define LINE_LEN 1024
#define NUM_THREADS 10
#define BUFFSIZE 100
#define SANE_LEN 128
#define CREATE_SEM 1
#define OPEN_SEM 0
#define RES_SEM_SUFFIX_LEN 11

//signal handler var
volatile sig_atomic_t done = 0;

/* Shared Variables */
sem_t* sem_mutex;
sem_t* sem_empty;
sem_t* sem_full;

sem_t* sem_mutex_threads;
sem_t* sem_empty_threads;
sem_t* sem_full_threads;

//sem_t* lock;

char* filename;
char* sem_name;

typedef enum{FREE, PROCESSED, PROCESSING} queue_state;

struct request {
	char keyword[SANE_LEN];
	int index;
};

struct shared_data {
	queue_state state_queue[NUM_THREADS];
	struct request request_queue[NUM_THREADS];
	int result_queue[NUM_THREADS][BUFFSIZE];

	int in[NUM_THREADS];	
};

struct shared_data* shm_start;

void *runner(void *param);

void* createSharedMemSeg(char* shm_name, int shm_size)
{
	//clean up shm with same name.
	shm_unlink(shm_name);

	int fd, i;
	struct stat sbuf;
	void* shm_start;
	struct shared_data* sdptr;
	char* p;

	fd = shm_open(shm_name, O_RDWR | O_CREAT, 0660);

	if ( fd < 0)
	{
		perror("Cannot create shared memory.\n");
		exit(1);
	}

	printf( "shm created, fd = %d\n", fd);

	ftruncate(fd, shm_size);
	fstat(fd, &sbuf);
	printf( "shm size = %d.\n", (int) sbuf.st_size);


	shm_start = mmap(NULL, sbuf.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

	if ( shm_start < 0)
	{
		perror("Cannot map shm.\n");
		exit(1);
	}

	printf( "mapped shm; start_address=%u.\n", (unsigned int) shm_start);
	close(fd);

	sdptr = (struct shared_data *) shm_start;

	return sdptr;
}

pthread_t createThread(void* request)
{
	int err;

	pthread_t tid;
	pthread_attr_t attr;

	pthread_attr_init(&attr);

	err = pthread_create(&tid, &attr, runner, request);

	if (err != 0)
	{
		printf( "Cannot create thread.\n");
		exit(1);
	}

	return tid;
}

sem_t* createOrOpenSemaphore(char* sem_name_full, int create, int init_val)
{
	sem_t *sem;

	if( create == 1) {
		printf("Unlink Status: ");
		fflush(stdout);
		sem_unlink(sem_name_full);
		sem = sem_open(sem_name_full, O_RDWR | O_CREAT, 0660, init_val);
	}
	else if ( create == 0) sem = sem_open(sem_name_full, O_RDWR);

	if (sem < 0)
	{
		perror("\nCannot create Semaphore.\n");
		exit(1);
	}

	printf("\nsem %s created.\n", sem_name_full);

	return sem;
}

void* generateSemName(char* full_sem_name, char* sem_type, int index)
{
	//Suffix.
	char intStr[12];
	sprintf(intStr, "%d", index);
	char suffix[RES_SEM_SUFFIX_LEN] = "_resQueue_";
	strcat(suffix, intStr);

	strcpy(full_sem_name, sem_name);
	strcat(full_sem_name, sem_type);
	strcat(full_sem_name, suffix);
}

void *runner(void* param)
{	
	int in = 0;

	char buff[LINE_LEN];
	char full_sem_name[SANE_LEN + RES_SEM_SUFFIX_LEN + 6]; // 5 is the length of the string "_empty".
	char keyword[SANE_LEN];
	int index;
	FILE* fp;

	fp = fopen( filename, "r");
	if ( fp == NULL)
	{
		printf("\n\nCould not open File %s.\n\n", filename);
	}

	struct request* sdptr = (struct request *) param;

	strcpy(keyword, sdptr->keyword);
	index = sdptr->index;

	/* *** Create Result Queue Semaphores *** */
	//mutex Sem
	generateSemName( &full_sem_name, "_mutex", index);
	sem_mutex_threads = createOrOpenSemaphore(full_sem_name, OPEN_SEM, -1);

	//full Semaphore
	generateSemName( &full_sem_name, "_full", index);
	sem_full_threads = createOrOpenSemaphore(full_sem_name, OPEN_SEM, -1);

	//empty Semaphore
	generateSemName( &full_sem_name, "_empty", index);
	sem_empty_threads = createOrOpenSemaphore(full_sem_name, OPEN_SEM, -1);


	//Find line numbers
	int lineno = 1;
	while(1)
	{
		if (feof(fp)) {	
			sem_wait(sem_mutex_threads);

			shm_start->state_queue[index] = PROCESSED;

			//To prevent a deadlock.
			sem_post(sem_full_threads);

			shm_start->in[index] = in;	
		
			sem_post(sem_mutex_threads);

			break;
		}

		fgets(buff, LINE_LEN, (FILE *) fp);
		

		char* location = strcasestr(buff, keyword);
		if ( location != NULL)
		{	
			sem_wait(sem_empty_threads);
			sem_wait(sem_mutex_threads);

			if (in == BUFFSIZE) in = 0;
		//	if (shm_start->result_queue[index][shm_start->in] == -1) {
				//sem_wait(sem_empty_threads);	
				shm_start->result_queue[index][in] = lineno;

				in = in + 1;

				sem_post(sem_full_threads);
		//	}
			sem_post(sem_mutex_threads);
		}

		lineno += 1;
	}

	//This is the last thing that the thread does before exiting.
	//Signal that the request has been process.
	//Do this by logically freeing request queue slot.
	sem_wait(sem_mutex);

	sdptr->index = -1;	

	sem_post(sem_mutex);

	fclose(fp);

	printf("Exiting thread for keyword %s.\n", keyword);
	pthread_exit(0);
}

void sanityCheck(char* filename, char* mem_name, char* sem_name_suffix)
{
	if( strlen(filename) > SANE_LEN) {
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

//waits for all threads to finish execution before exiting.
void sigHandler(int signum)
{
	int i;

	//acts as a busy loop until all requests are processed.
	for ( i = 0; i < NUM_THREADS; ++i)
	{
		sem_wait(sem_mutex);
		if (shm_start->request_queue[i].index != -1)
		{
			i = 0;
		}
		sem_post(sem_mutex);
	}

	done = 1;
}

int main( int argc, char *argv[])
{
	//signal handling.
	struct sigaction action;
	memset(&action, 0, sizeof(struct sigaction));
	action.sa_handler = sigHandler;
	sigaction(SIGINT, &action, NULL);

	int current_thread = 0;
	pthread_t workers[NUM_THREADS];

	char* shm_name = argv[1];
	filename = argv[2];
	sem_name = argv[3];

	//Do all inputs satisfy the conditons required?
	sanityCheck( filename, shm_name, sem_name);

	//char lock_name[strlen(sem_name) + strlen("_lock")];
	//strcpy(lock_name, sem_name);
	//strcat(lock_name, "_lock");
	//lock = createOrOpenSemaphore(lock_name, CREATE_SEM, 0);

	char mutex_name[strlen(sem_name) + strlen("_mutex")];
	strcpy(mutex_name, sem_name);
	strcat(mutex_name, "_mutex");
	sem_mutex = createOrOpenSemaphore( mutex_name, CREATE_SEM, 1);

	char empty_name[strlen(sem_name) + strlen("_empty")];
	strcpy(empty_name, sem_name);
	strcat(empty_name, "_empty");
	sem_empty = createOrOpenSemaphore(empty_name, CREATE_SEM, NUM_THREADS);

	char full_name[strlen(sem_name) + strlen("_full")];
	strcpy(full_name, sem_name);
	strcat(full_name, "_full");
	sem_full = createOrOpenSemaphore(full_name, CREATE_SEM, NUM_THREADS);

	int shm_size = sizeof(struct shared_data);

	//create shared memory.
	shm_start = createSharedMemSeg(shm_name, shm_size);

	//initialize shared memory data.	
	sem_wait(sem_mutex);
	for ( int i = 0; i < NUM_THREADS; ++i)
	{
		shm_start->state_queue[i] = FREE;

		shm_start->request_queue[i].index = -1;

		shm_start->in[i] = 0;

		for (int j = 0; j < BUFFSIZE; ++j)
		{
			shm_start->result_queue[i][j] = -1;
		}
	}
	sem_post(sem_mutex);

//	sem_mutex = createOrOpenSemaphore(mutex_name, CREATE_SEM, 1);
//	sem_empty = createOrOpenSemaphore(empty_name, CREATE_SEM, NUM_THREADS);
//	sem_full = createOrOpenSemaphore(full_name, CREATE_SEM, 0);
	
	while (!done){
		sem_wait(sem_full);
		sem_wait(sem_mutex);

		pthread_t pt_id;	

		for ( int i = 0; i < NUM_THREADS; ++i)
		{	
			if ( shm_start->request_queue[i].index != -1)
			{	
			    pt_id = createThread( &shm_start->request_queue[i]);
				//pthread_join(pt_id, NULL);
			}
		}	

		sem_post(sem_mutex);
		sem_post(sem_empty);
	}	
	
	sem_close(sem_mutex);
	sem_close(sem_full);
	sem_close(sem_empty);



	printf("\n___________________\n\n");

	printf("Moriturus te saluto\n");


}
