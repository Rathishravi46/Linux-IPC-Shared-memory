// sem.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <sys/wait.h>

#define TEXT_SZ 2048  // Shared memory size

// Shared memory structure
struct shared_use_st {
    int written;                  // 0 = empty, 1 = full
    char some_text[TEXT_SZ];      // Data buffer
};

int main() {
    int shmid;
    void *shared_memory = (void *)0;
    struct shared_use_st *shared_stuff;

    // Create shared memory
    shmid = shmget((key_t)1234, sizeof(struct shared_use_st), 0666 | IPC_CREAT);
    if (shmid == -1) {
        fprintf(stderr, "shmget failed\n");
        exit(EXIT_FAILURE);
    }

    printf("Shared memory id = %d\n", shmid);

    // Attach to shared memory
    shared_memory = shmat(shmid, (void *)0, 0);
    if (shared_memory == (void *)-1) {
        fprintf(stderr, "shmat failed\n");
        exit(EXIT_FAILURE);
    }

    printf("Memory attached at %p\n", shared_memory);
    shared_stuff = (struct shared_use_st *)shared_memory;
    shared_stuff->written = 0;

    pid_t pid = fork();

    if (pid < 0) {
        fprintf(stderr, "Fork failed\n");
        exit(EXIT_FAILURE);
    }

    // -----------------------------
    // Child process (Consumer)
    // -----------------------------
    if (pid == 0) {
        while (1) {
            // Wait until producer writes something
            while (shared_stuff->written == 0) {
                sleep(1);
            }

            printf("Consumer received: %s", shared_stuff->some_text);

            // If producer sends "end", exit loop
            if (strncmp(shared_stuff->some_text, "end", 3) == 0) {
                break;
            }

            // Reset written flag so producer can write again
            shared_stuff->written = 0;
        }

        // Detach shared memory
        if (shmdt(shared_memory) == -1) {
            fprintf(stderr, "shmdt failed\n");
            exit(EXIT_FAILURE);
        }

        exit(EXIT_SUCCESS);
    }

    // -----------------------------
    // Parent process (Producer)
    // -----------------------------
    else {
        char buffer[TEXT_SZ];

        while (1) {
            printf("Enter Some Text: ");
            fgets(buffer, TEXT_SZ, stdin);

            // Write to shared memory
            strncpy(shared_stuff->some_text, buffer, TEXT_SZ);
            shared_stuff->written = 1;

            // Echo back what was written
            printf("Producer sent: %s", shared_stuff->some_text);

            // Exit if "end" entered
            if (strncmp(buffer, "end", 3) == 0) {
                break;
            }

            // Wait until consumer reads it
            while (shared_stuff->written == 1) {
                sleep(1);
            }
        }

        // Wait for consumer to finish
        wait(NULL);

        // Detach shared memory
        if (shmdt(shared_memory) == -1) {
            fprintf(stderr, "shmdt failed\n");
            exit(EXIT_FAILURE);
        }

        // Remove shared memory segment
        if (shmctl(shmid, IPC_RMID, 0) == -1) {
            fprintf(stderr, "shmctl failed\n");
            exit(EXIT_FAILURE);
        }

        exit(EXIT_SUCCESS);
    }
}