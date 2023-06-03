#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <omp.h>


typedef struct {
    char sentences[100][1000]; //Que holdes= 100 sentence. sentence max length 1000 character. 
    int head;
    int tail;
    int count;
    omp_lock_t lock; // This allows individual instances of the Que to be protected by separate locks, ensuring exclusive access to the struct's data.
} Que;

void enqueue(Que* q, char* sentence) {
    strcpy_s(q->sentences[q->tail], 1000, sentence);
    q->tail++;
    if (q->tail == 100) {
        printf("Queue is full. Cannot enqueue more elements.\n");
    }
    q->count++;
}

char* dequeue(Que* q) {
    if (q->count > 0) {
        char* sentence = q->sentences[q->head];
        q->head++; //new head is the next
        q->count--;
        return sentence;
    }
    else {
        printf("Queue is empty. Cannot dequeue elements.\n");
        return NULL;
    }
}



char* trim_sentence(char* sentence) {
    int i, j;

    // Remove leading white space
    for (i = 0; sentence[i] == ' ' || sentence[i] == '\t'; i++);
    for (j = 0; sentence[i]; i++) {
        sentence[j++] = sentence[i];
    }
    sentence[j] = '\0';

    // Remove trailing white space
    for (i = 0; sentence[i] != '\0'; i++) {
        if (sentence[i] != ' ' && sentence[i] != '\t') {
            j = i;
        }
    }
    sentence[j + 1] = '\0';


    return sentence;
}

int main() {

    Que queues[4];//Array of Que structures

    for (int i = 0; i < 4; i++) {
        queues[i].head = 0;
        queues[i].tail = 0;
        queues[i].count = 0;
        omp_init_lock(&queues[i].lock); // initialize a lock variable before it can be used with omp_set_lock or omp_unset_lock
    }

    FILE* file;
    errno_t err = fopen_s(&file, "F:\\odev\\file.txt", "r");
    if (err != 0) {
        printf("Failed to open the file.\n");
        return 1;
    }

    int done = 0;
    int total_words = 0;
    int total_chars = 0;

#pragma omp parallel num_threads(8)
    {
        int th_id = omp_get_thread_num();
            // Producer threads
            char line[1000];
            while (1) {
                if (fgets(line, 1000, file)) {
                    char* next;
                    char* sentence = strtok_s(line, ".", &next); //thread-safe strtok version
                    /*static pointer in strtok causes the function to point to the wrong string if called at different times.
                    strtok_s has a context pointer keeps track of the tokenizer for each specific use.*/
                    while (sentence) {
                        int consumer_id = rand() % 4;
                        size_t sentence_length = 0;
                        if (strnlen_s(sentence, 1000) > 1) {
                            omp_set_lock(&queues[consumer_id].lock);//p:269/*Locking the struct instance queues[consumer_id] allows as to enforce mutual exclusion for this instance*/
                            enqueue(&queues[consumer_id], sentence); //performs the enqueue operation on that specific consumer's queue. 
                            omp_unset_lock(&queues[consumer_id].lock);
                            printf("Producer %d: %s\n", th_id, trim_sentence(sentence));

                        }
                        sentence = strtok_s(NULL, ".", &next);
                    }
                }
                else {
                    line[0] = '\0';
                }
                //If a line is empty, indicating the end of the file, the producer thread increments the done variable using an atomic operation(#pragma omp atomic) and breaks out of the loop.
                if (line[0] == '\0') {// the atomic directive has the potential to be the fastest method of obtaining mutual exclusion. page:270
                #pragma omp atomic //necessary to prevent race conditions if multiple producers tried to read or modify the value of "done" at the same time/
                    done++;
                    break;
                }
            }
        
        #pragma omp barrier

    }
#pragma omp parallel num_threads(4)
    {
        // threads can  start dequeuing while other threads are still enqueueing since no barrier is used.No need for barrier because queues are intialized before the enqueueing.
       // Consumer threads
        int consumer_id = omp_get_thread_num(); //rest of the 12 threads are consumers
        while (1) {
            char* deQsentence;
            int word_count = 0;
            int char_count = 1;

            omp_set_lock(&queues[consumer_id].lock);
            if (queues[consumer_id].count > 0) {
                deQsentence = dequeue(&queues[consumer_id]); //dequeue a sentence from consumer's queue
            }
            else {
                deQsentence = NULL;
            }
            omp_unset_lock(&queues[consumer_id].lock);

            if (deQsentence != NULL && deQsentence[0] != '\0') {//Check If there are sentences in the queue (dequeued sentence is not NULL)
                char sentence_copy[1000];
                strcpy_s(sentence_copy, 1000, deQsentence);
                char* next;
                char* str = strtok_s(sentence_copy, " ", &next);
                while (str) {
                    word_count++;
                    char_count += strlen(str);
                    str = strtok_s(NULL, " ", &next);
                }
                printf("Consumer %d: %s (Word count: %d, Character count: %d)\n", consumer_id, trim_sentence(deQsentence), word_count, char_count);
                // Add to total counts
                #pragma omp atomic//necessary to prevent race conditions if multiple producers tried to read or modify the value of "totla_words" at the same time
                total_words += word_count;
                #pragma omp atomic
                total_chars += char_count;
            }
            else {//Synchronization handling: As long as the done variable is not equal to 8, which means that all producer threads have finished enqueueing, the consumer thread will continue looping and checking for sentences in the queue.
                if (done == 8) {//If there are no sentences in the queue and the done variable is equal to 8, indicating that all producers have finished, the consumer threads break out of the loop.
                    break;
                }
            }
        }
    }

    printf("Total word count: %d\n", total_words);
    printf("Total character count: %d\n", total_chars);
    for (int i = 0; i < 4; i++) {
        omp_destroy_lock(&queues[i].lock);
    }

    fclose(file);
    return 0;
}