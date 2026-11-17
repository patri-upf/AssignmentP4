#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "parsePGM.h"

#define BUFF_SIZE 1024

unsigned int histogram[256];
pthread_mutex_t lock;

typedef struct { 
    char* path; 
    int offset;       
    // Offset from the beginning of the file (including header) 
    int bytesToRead; 
    // any other info you might want to pass 
} ThreadInfo; 

void* worker(void* arg) {
    ThreadInfo *th = (ThreadInfo*) arg;
    usigned char buffer[BUFF_SIZE];
    unsigned local_histogram[256] = {0};

    int fd = open(th->path, O_RDONLY);
   
    if (fd < 0) {
        perror("open");
        return NULL;
    }
    lseek(fd, th->offset, SEEK_SET);
    
    int bytes_left = th->bytesToRead;
    int to_read;
    while(bytes_left > 0) {
        if (BUFF_SIZE < bytes_left){
            to_read = BUFF_SIZE;
        } else {
            to_read = bytes_left;
        }
           
        int n = read(fd, buffer, to_read);
        
        if (n <= 0) {
            perror("read");
            break;
        }

        pthread_mutex_lock(&lock);
        for (int i = 0; i < n; i++) {
            histogram[i] += local[i];
        }
        bytes_left -= n;
        pthread_mutex_unlock(&lock);
    }
    close(fd);
    return NULL;
}

int main(int argc, char* argv[]) {
    if (argc != 4) {
        printf("Incorrect usage. Args needed: histogram pathToImage pathToHistogramOut workThreadsNumber\n");
        _exit(1);
    }

    int thread_numbers = atoi(argv[argc - 1]);

    pthread_mutex_init(&lock, NULL);

    if (thread_numbers < 1) {
        printf("The numbers of threads should be at least 1\n");
        _exit(1);
    }

    // Read the header
    int width, height;
    int maxval;
    int nBytesHeader = parse_pgm_header(argv[1], &width, &height, &maxval);
    
    if (maxval > 255){
        perror("Expecting 1 byte ints\n");
        _exit(1);
    }

    int nPixels = width * height;

    for (int i = 0; i <= maxval; i++) {
        histogram[i] = 0;
    }

    // Compute histogram. This is what you will need to do in threads.
    unsigned char* buffer = malloc(BUFF_SIZE);
    int fd = open(argv[1], O_RDONLY | O_TRUNC);
    lseek(fd, nBytesHeader, SEEK_SET); // Move the cursor to the start of the data segment
                                       // For the 
    for (int i = 0; i < nPixels; i += BUFF_SIZE) {
        nBytesRead = read(fd, buffer, BUFF_SIZE);
        if (nBytesRead <= 0) {
            printf("Unexpected error: the file ended before all pixels where read.\n");
            _exit(1);
        }
        for (int i = 0; i < nBytesRead; i++) {
            histogram[buffer[i]]++;
        }
    }
    close(fd);


    // Write histogram, so it can be loaded in python with np.loadtxt
    int fd_out = open(argv[2], O_WRONLY | O_CREAT, 0644);
    for (int i = 0; i <= maxval; i++) {
        char s[80];
        sprintf(s, "%d,%d\n", i, histogram[i]);
        write(fd_out, s, strlen(s));
    }
    close(fd_out);

}

