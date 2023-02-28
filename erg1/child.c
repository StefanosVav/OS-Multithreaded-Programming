#include <stdio.h> 
#include <stdlib.h> 
#include <string.h> 
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h> 
#include <semaphore.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#define SEM_LOCK_NAME "lock"
#define SEM_CONS_NAME "cons"
#define SEM_PROD_NAME "prod"
#define MAX_SIZE 100

int main (int argc, char **argv) { 

    //////////////////////////////////////////////////////////////////////////////////////////
    // PART 1:  GET ARGUMENTS FROM EXECV AND WRITE THEM TO A NEW TEST.TXT FILE
    //////////////////////////////////////////////////////////////////////////////////////////

    int N = atoi(argv[1]);
    int nlines = atoi(argv[2]);
    int shmid = atoi(argv[3]);
    
    FILE *fp;
    fp = fopen("test.txt", "a");
    if (fp == NULL)
        printf("Could not open file");
    fprintf(fp, "CHILD %ld created with arguments: Requests per kid: %d, Lines to choose from: %d, shmid: %d\n", (long) getpid(), N, nlines, shmid);
    fclose(fp);

    //////////////////////////////////////////////////////////////////////////////////////////
    // PART 2:  OPEN NAMED SEMAPHORES
    //////////////////////////////////////////////////////////////////////////////////////////

    sem_t *semlock = sem_open(SEM_LOCK_NAME, O_RDWR);
    if (semlock == SEM_FAILED) {
        perror("sem_open error on lock");
        exit(EXIT_FAILURE);
    }
    sem_t *semcons = sem_open(SEM_CONS_NAME, O_RDWR);
    if (semcons == SEM_FAILED) {
        perror("sem_open error on consumer");
        exit(EXIT_FAILURE);
    }
    sem_t *semprod = sem_open(SEM_PROD_NAME, O_RDWR);
    if (semprod == SEM_FAILED) {
        perror("sem_open error on producer");
        exit(EXIT_FAILURE);
    }

    //////////////////////////////////////////////////////////////////////////////////////////
    // PART 3:  FOR N REQUESTS, WAIT TO ACQUIRE LOCK_SEM, WAIT TO ACQUIRE CONS_SEM,
    //          SEND RANDOM LINE REQUEST, POST PROD_SEM. WAIT AGAIN TO ACQUIRE CONS_SEM,
    //          PRINT LINE RECEIVED THROUGH SHARED MEMORY, POST PROD_SEM AND LOCK_SEM.
    //////////////////////////////////////////////////////////////////////////////////////////

    clock_t t;                          //to calculate the time that the parent process needed to respond to a request 
    char *shm;
    int i;
    double time_taken[N];
    for (i = 0; i < N; i++) {

        if (sem_wait(semlock) < 0) {                //Multiple Children wait here trying to acquire lock semaphore
            perror("sem_wait failed on lock in child");
            continue;
        }
        printf("\nCHILD %ld Acquired LOCK semaphore to make its %dth Request\n", (long) getpid(), i+1);

        if (sem_wait(semcons) < 0) {
            perror("sem_wait failed on lock in child");
            continue;
        }
        //printf("CHILD %ld acquired CONS semaphore\n", (long) getpid());

        int r = abs( rand()*getpid() ) % nlines + 1; 
        char NumLine[10];                                           //Random Number of Line that child requests from parent
        sprintf(NumLine, "%d", r);
        
        if ((shm = shmat(shmid, NULL, 0)) == (char *) -1) {         //Attach shared memory segment to process
            perror("shmat");
            exit(EXIT_FAILURE);
        }
        strcpy(shm, NumLine);                                       //Write Random Line Number to Shared Memory

        if (shmdt(shm) == -1) {                                     //Detach shared memory segment from process
            perror("shmdt");
            return 1;
        }   
        printf("CHILD %ld Requests line no.%d\n", (long) getpid(), r);

        t = clock();                                                //Timer Start Between Request and Response
        if (sem_post(semprod) < 0) {
            perror("sem_post error on child");
        }

        if (sem_wait(semcons) < 0) {
            perror("sem_wait failed on lock in child");
            continue;
        }
        t = clock() - t;                                            //Timer Stop
        time_taken[i] = ((double)t)/CLOCKS_PER_SEC;                 //Time Taken in seconds

        //printf("CHILD %ld acquired CONS semaphore\n", (long) getpid());

        if ((shm = shmat(shmid, NULL, 0)) == (char *) -1) {          //Attach shared memory segment to process
            perror("shmat");
            exit(EXIT_FAILURE);
        }

        printf("CHILD %ld Receives line no.%d from parent : %s\n", (long) getpid(), r, shm);
        memset(shm,0,MAX_SIZE);                                     //Empty Shared Memory

        if (shmdt(shm) == -1) {                                     //Detach shared memory segment from process
            perror("shmdt");
            return 1;
        }   

        if (sem_post(semcons) < 0) {
            perror("sem_post error on child");
        }

        if (sem_post(semlock) < 0) {
            perror("sem_post error on child");
        }

    }

    //////////////////////////////////////////////////////////////////////////////////////////
    // PART 4:  CLOSE SEMAPHORES, CALCULATE AVG TIME BETWEEN REQUEST AND RESPONSE AND
    //          WRITE IT TO THE TEST.TXT FILE
    //////////////////////////////////////////////////////////////////////////////////////////

    if (sem_close(semlock) < 0)
        perror("sem_close failed");
    if (sem_close(semprod) < 0)
        perror("sem_close failed");
    if (sem_close(semcons) < 0)
        perror("sem_close failed");

    double total = 0;
    for(int i = 0; i < N; i++){
        total += time_taken[i];
    }
    double avgtime = total / (double)N;

    fp = fopen("test.txt", "a");
    if (fp == NULL)
        printf("Could not open file");
    fprintf(fp, "CHILD %ld about to be deleted. Average time between Request and Response: %lf seconds.\n", (long) getpid(), avgtime);
    fclose(fp);
        
    return 0;
}