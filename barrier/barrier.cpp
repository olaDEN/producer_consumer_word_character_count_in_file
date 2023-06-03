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

    int done = 0;//flag that indicates whether the loop should terminate, and it should be shared among all threads.
    int total_words = 0;
    int total_chars = 0;

#pragma omp sections
    {
#pragma omp section
        {//producer section
#pragma omp parallel num_threads(8)
            {
                int th_id = omp_get_thread_num();
                // Producer threads
                char line[1000];
                while (1) {
                    if (fgets(line, 1000, file)) {
                        char* next;
                        char* sentence = strtok_s(line, ".", &next); //thread-safe strtok version
                        //static pointer in strtok causes the function to pointto the wrong string if called
                        //at different times. strtok_s has a context pointer keeps track of the state of the tokenizer for each specific use.
                        while (sentence) {
                            int consumer_id = rand() % 4;
                            size_t sentence_length = 0;
                            if (strnlen_s(sentence, 1000) > 1) {
                                omp_set_lock(&queues[consumer_id].lock);
                                enqueue(&queues[consumer_id], sentence); //enqueue sentence in a random consumer's queue
                                omp_unset_lock(&queues[consumer_id].lock);
                                printf("Producer %d: %s\n", th_id, trim_sentence(sentence));

                            }
                            sentence = strtok_s(NULL, ".", &next);
                        }
                    }
                    else {
                        line[0] = '\0';
                    }


                    if (line[0] == '\0') {
#pragma omp atomic
                        done++;

                        break;
                    }
                }
            }
        }
#pragma omp section
        {
#pragma omp parallel num_threads(4)
            {
                // Consumer threads
                int consumer_id = omp_get_thread_num();
                while (1) {
                    char* deQsentence;
                    int word_count = 0;
                    int char_count = 1; //include the end character (period)

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
#pragma omp atomic
                        total_words += word_count;
#pragma omp atomic
                        total_chars += char_count;
                    }
                    else {

                        if (done == 8) {
                            break;
                        }
                    }
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

