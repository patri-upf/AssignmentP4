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
    unsigned char buffer[BUFF_SIZE];
    unsigned int local_histogram[256] = {0};

    int fd = open(th->path, O_RDONLY);
   
    if (fd < 0) {
        perror("open");
        return (void *)1;
    }

    if(lseek(fd, th->offset, SEEK_SET) < 0) {
        perror("lseek");
        close(fd);
        return (void *)1;
    }
    
    long bytes_left = th->bytesToRead;
    size_t to_read;
    while(bytes_left > 0) {
        if (BUFF_SIZE < bytes_left){
            to_read = BUFF_SIZE;
        } else {
            to_read = bytes_left;
        }
           
        size_t n = read(fd, buffer, to_read);
        
        if (n <= 0) {
            perror("read");
            close(fd);
            return (void *)1;
        }

        for (int i = 0; i < n; i++) {
            local_histogram[buffer[i]]++;
        }
        bytes_left -= n;
    }
    close(fd);

    pthread_mutex_lock(&lock);
    for (int i = 0; i < 256; i++) {
        histogram[i] += local_histogram[i];
    }
    pthread_mutex_unlock(&lock);

    return NULL;
}

int main(int argc, char* argv[]) {
    char s[128];
    if (argc != 4) {
        sprintf(s, "Incorrect usage. Args needed: histogram pathToImage pathToHistogramOut workThreadsNumber\n");
        write(STDERR_FILENO, s, strlen(s));
        _exit(1);
    }

    int thread_numbers = atoi(argv[3]);

    if (thread_numbers < 1) {
        sprintf(s, "The numbers of threads should be at least 1\n");
        write(STDERR_FILENO, s, strlen(s));
        _exit(1);
    }

    pthread_mutex_init(&lock, NULL);
    
    // Read the header
    int width, height;
    int maxval;
    int nBytesHeader = parse_pgm_header(argv[1], &width, &height, &maxval);
    
    if (maxval > 255){
        sprintf(s, "Expecting 1 byte ints\n");
        write(STDERR_FILENO, s, strlen(s));
        _exit(1);
    }

    if (nBytesHeader <= 0) {
        sprintf(s, "Could not parse PGM header\n");
        write(STDERR_FILENO, s, strlen(s));
        _exit(1);
    }

    long long int nPixels = (long long)width * height;

    for (int i = 0; i <= maxval; i++) {
        histogram[i] = 0;
    }

    // Compute histogram. This is what you will need to do in threads.
    int buf_piece = ((nPixels + thread_numbers - 1) / thread_numbers);
    pthread_t* threads = malloc(thread_numbers * sizeof(pthread_t));

    ThreadInfo* thread_info = malloc(thread_numbers * sizeof(ThreadInfo));
    long start = 0;
    long bytes = 0;
    for (long i = 0; i < thread_numbers; i++) {
        start = i * buf_piece;
        bytes = nPixels - start;
        if (bytes > buf_piece)
            bytes = buf_piece;
        if (bytes < 0) 
            bytes = 0;
        thread_info[i].path = argv[1];
        thread_info[i].offset = nBytesHeader + start;
        thread_info[i].bytesToRead = bytes;
        pthread_create(&threads[i], NULL, worker, &thread_info[i]);
    }

    for (int i = 0; i < thread_numbers; i++) {
        char s[124];
        void* ret;
        pthread_join(threads[i], &ret);
       
        if (ret != NULL){
            sprintf(s, "Thread %d failed\n", i);
            write(STDERR_FILENO, s, strlen(s));
        }
    }
    // Write histogram, so it can be loaded in python with np.loadtxt
    int fd_out = open(argv[2], O_WRONLY | O_CREAT | O_TRUNC, 0644);
    for (int i = 0; i <= maxval; i++) {
        char s[80];
        sprintf(s, "%d,%d\n", i, histogram[i]);
        write(fd_out, s, strlen(s));
    }
    close(fd_out);

}

