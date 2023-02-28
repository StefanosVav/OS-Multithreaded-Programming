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

#define SEM_PERMS (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP)
#define MAX_SIZE 100
#define SEM_LOCK_NAME "lock"
#define SEM_CONS_NAME "cons"
#define SEM_PROD_NAME "prod"
 
void main(int argc, char *argv[] )  {   

    //////////////////////////////////////////////////////////////////////////////////////////
    // PART 1:  GET INPUT FROM COMMAND LINE
    //////////////////////////////////////////////////////////////////////////////////////////

    srand(time(NULL));
    char * X;                                   //X: name of .txt file
    int K, N;                                   //K: number of kids, N: number of requests per kid
	if(argc != 4){  
		printf("Error - Wrong number of input arguments (Expected 4).\n");  
        return;
	}  
	else{  
		X = argv[1];
		K = atoi(argv[2]);
		N = atoi(argv[3]);
    } 
    printf("Name of file: %s, Number of kids: %d, Number of Requests per kid: %d\n\n", X, K, N);
    
    //////////////////////////////////////////////////////////////////////////////////////////
    // PART 2:  CREATE SHARED MEMORY AND POSIX NAMED SEMAPHORES TO IMPLEMENT 
    //          REQUIRED MESSAGE TRANSFER BETWEEN CHILD AND PARENT
    //////////////////////////////////////////////////////////////////////////////////////////
    
	// Set Lock Semaphore:1, Cons Semaphore:1, Prod Semaphore:0
    sem_t *semlock = sem_open(SEM_LOCK_NAME, O_CREAT | O_EXCL, SEM_PERMS, 1);           //Ensures that 2 child processes will never use Shared Memory at the same time
    if (semlock == SEM_FAILED) {
        perror("sem_open error on lock");
        exit(EXIT_FAILURE);
    }
    sem_t *semcons = sem_open(SEM_CONS_NAME, O_CREAT | O_EXCL, SEM_PERMS, 1);           //Informs child process that parent has finished using Shared Memory
    if (semcons == SEM_FAILED) {
        perror("sem_open error on consumer");
        exit(EXIT_FAILURE);
    }
    sem_t *semprod = sem_open(SEM_PROD_NAME, O_CREAT | O_EXCL, SEM_PERMS, 0);           //Informs parent process that child has finished using Shared Memory
    if (semprod == SEM_FAILED) {
        perror("sem_open error on producer");
        exit(EXIT_FAILURE);
    }

    // Close the lock semaphore as we won't be using it in the parent process 
    if (sem_close(semlock) < 0) {
        perror("sem_close failed");
        sem_unlink(SEM_LOCK_NAME);
        exit(EXIT_FAILURE);
    }
    
    int shmid;                                          //Shared Memory ID
    int err = 0; 
    char *shm;                                          //Shared Memory Address

    shmid = shmget(IPC_PRIVATE, MAX_SIZE, 0666) ;       //Create shared memory segment
    if (shmid == -1){
        perror ("shmget"); 
        exit(EXIT_FAILURE);
    }
    else 
        printf("Allocated Shared Memory %d\n",shmid);


    //////////////////////////////////////////////////////////////////////////////////////////
    // PART 3:  CREATE CHILD PROCESSES USING FORK, EXECV AND PASS ARGUMENTS
    //////////////////////////////////////////////////////////////////////////////////////////

    FILE* file = fopen(X, "r");                         //Open text file and get number of lines
    if (file == NULL)
    {
        printf("Could not open file %s\n", X);
        return;
    }
    int n = 0;                                          //n = Number of lines in text file                         
    char c;
    while((c=fgetc(file))!=EOF) {
        if(c=='\n')
            n++;
    }
    fclose(file);
                         
    char nlines[20];
    sprintf(nlines, "%d", n);
    char ID[20];
    sprintf(ID, "%d", shmid);
	pid_t pids[K];
    int i;
    char *args[] = { "./child", argv[3], nlines, ID, NULL };

    for (i = 0; i < K; i++) {
        if ((pids[i] = fork()) < 0) {
            perror("fork failed");
            exit(EXIT_FAILURE);
        }

        if (pids[i] == 0) {
            if (execv(args[0], args) < 0) {
                perror("execv failed");
                exit(EXIT_FAILURE);
            }
            break;
        }
    }

    //////////////////////////////////////////////////////////////////////////////////////////
    // PART 4:  FOR K*N REQUESTS, WAIT FOR THE CHILD PROCESS TO SEND REQUEST (WAIT SEM_PROD),
    //          GET REQUESTED LINE FROM FILE, SEND IT BACK (AND POST SEM_CONS)
    //////////////////////////////////////////////////////////////////////////////////////////

    int totalRequests = K*N;    //Number of kids * Number of requests per kid
    int NumLine = 0;            //Number of line requested by child process

    for(i = 0; i < totalRequests; i++){

        if (sem_wait(semprod) < 0) {                    //Wait for child process to post producer semaphore
            perror("sem_wait failed on parent");
            continue;
        }

        //printf("PARENT acquired PROD semaphore\n");
        if ((shm = shmat(shmid, NULL, 0)) == (char *) -1) {     //Attach shared memory segment to process
            perror("shmat");
            exit(EXIT_FAILURE);
        }

        NumLine = atoi(shm);                                    //NumLine = Line Requested from Child

        //Open text file and get line requested from Child
        char line[MAX_SIZE];
        FILE* file = fopen(X, "r");
        if (file == NULL)
        {
            printf("Could not open file %s\n", X);
            return;
        }
        int j = 1;
        while (fgets(line, sizeof(line), file)) {

            if(j == NumLine){
                if (line[strlen(line)-1] == '\n')
                    line[strlen(line)-1] = '\0';

                break;
            }
            j++;
        }
        fclose(file); 

        printf("PARENT sends back line no.%d: %s\n", NumLine, line);
        strcpy(shm, line);                                      //Send back the requested line via shared memory

        if (shmdt(shm) == -1) {                                 //Detach shared memory segment from process
            perror("shmdt");
            return;
        } 

        if (sem_post(semcons) < 0) {
            perror("sem_post error on child");
        }
    }

    //////////////////////////////////////////////////////////////////////////////////////////
    // PART 5:  WAIT FOR CHILD PROCESSES TO FINISH, CLOSE AND UNLINK SEMAPHORES,
    //          REMOVE SHARED MEMORY SEGMENT.
    //////////////////////////////////////////////////////////////////////////////////////////

    for (i = 0; i < K; i++)
        if (waitpid(pids[i], NULL, 0) < 0)
            perror("waitpid failed");

    
    if (sem_close(semcons) < 0) {
        perror("sem_close failed");
        sem_unlink(SEM_CONS_NAME);
        exit(EXIT_FAILURE);
    }
    if (sem_close(semprod) < 0) {
        perror("sem_close failed");
        sem_unlink(SEM_PROD_NAME);
        exit(EXIT_FAILURE);
    }
    
    err = shmctl(shmid, IPC_RMID , 0);             // Remove segment
    if (err == -1) 
        perror ("shmctl");
    else 
        printf("\nDeleted Shared Memory\n");

    if (sem_unlink(SEM_CONS_NAME) < 0)
        perror("sem_unlink failed");
    if (sem_unlink(SEM_PROD_NAME) < 0)
        perror("sem_unlink failed");
    if (sem_unlink(SEM_LOCK_NAME) < 0)
        perror("sem_unlink failed");
    printf("Deleted Semaphores\n");
}