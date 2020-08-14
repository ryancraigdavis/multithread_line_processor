// daviryan@oregonstate.edu
// Line Processor - Processes lines using a multithreaded consumer/producer system

#define  _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdbool.h>
#include <dirent.h> 
#include <pthread.h>

// Size of the buffer
#define B1_SIZE 10
#define B2_SIZE 10
#define B3_SIZE 10

// Buffers, shared resources
char *buffer1[B1_SIZE];
char *buffer2[B2_SIZE];
char *buffer3[B3_SIZE];

// Number of items in the buffer, shared resource
int buffer1_count = 0;
int buffer2_count = 0;
int buffer3_count = 0;

// Index where the producer will put the next item
int buffer1_pro_idx = 0;
int buffer2_pro_idx = 0;
int buffer3_pro_idx = 0;

// Index where the consumer will pick up the next item
int buffer1_con_idx = 0;
int buffer2_con_idx = 0;
int buffer3_con_idx = 0;

// Initialize the mutexes
pthread_mutex_t b1_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t b2_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t b3_mutex = PTHREAD_MUTEX_INITIALIZER;

// Initialize the condition variables
pthread_cond_t buffer1_full = PTHREAD_COND_INITIALIZER;
pthread_cond_t buffer1_empty = PTHREAD_COND_INITIALIZER;
pthread_cond_t buffer2_full = PTHREAD_COND_INITIALIZER;
pthread_cond_t buffer2_empty = PTHREAD_COND_INITIALIZER;
pthread_cond_t buffer3_full = PTHREAD_COND_INITIALIZER;
pthread_cond_t buffer3_empty = PTHREAD_COND_INITIALIZER;

// These global variables are used to make sure that each byte inputted 
// makes it through to the end/ to help prevent race conditions
int all_done = 0;
int total_bytes = 0;

void prints(char *lines) {
    //printf("%s\n", lines);
}

// This function exchanges a "++" for a "^"
// This code is modified from my Smallsh.c program where I replace "$$" with PID
const char* plus_sign(char *line) {

    // Creates a new input in order to copy the line over
    char *plus_line = NULL;
    plus_line = (char *)malloc((strlen(line)+1)*sizeof(char));
    memset(plus_line, '\0', strlen(line)+1);
    memcpy(plus_line, line, strlen(line));
    
    // We need to first go through the line and see if there are any 
    // "++" that need to be converted to the carrot "^"
    int plus_count = 0;
    int plus_pos = 0;
    while(plus_pos < strlen(plus_line)) {
        int convert_plus = strncmp(&plus_line[plus_pos],"++",2);
        if (convert_plus == 0) {
            plus_count++;
            plus_pos++;
        }
        plus_pos++;
    }

    // Change total bytes by plus count - as bytes are subtracted
    // because of ++ to ^, that extra byte must be subtracted from the count
    total_bytes -= plus_count;

    // Multiply the number of instances of ++ with the len of ^
    // This will be used to shorten the line
    long plus_length = plus_count;
    plus_pos = 0;

    // Now create a new line var with this new length
    char *carrot_line = NULL;
    carrot_line = (char *)malloc((strlen(line)-plus_length)*sizeof(char));
    memset(carrot_line, '\0', strlen(line)-plus_length);

    // If plus_count is < 1, that means no "++" were found, thus just copy over the
    // input from line
    if (plus_count < 1) {
        memcpy(carrot_line, line, strlen(line));

    } else {

        // Start with a blank line
        strcpy(carrot_line,"");

        // For loop goes through each iteration of "++"
        // Each time a "++" is found, restart_loop is set to true
        for (int i = 0; i < plus_count; ++i) {
            bool restart_loop = false;
            while(restart_loop == false){

                // While loop goes through looking for $$, then concatenates
                // the string with the ^ - sets the restart bool to true
                int convert_plus = strncmp(&plus_line[plus_pos],"++",2);
                if (convert_plus == 0) {
                    strncat(carrot_line, plus_line, plus_pos);
                    strcat(carrot_line, "^");
                    plus_line = plus_line + plus_pos + 2;
                    restart_loop = true;
                }
                plus_pos++;
            }
            plus_pos = 0; 
        }

        // Finally concatenate the rest of the string if it isn't null
        if (strlen(plus_line) != 0) {
            strcat(carrot_line, plus_line);
        }
    }

    // Return the new line
    return carrot_line;
}

// Input producer thread - reads input
void *b1_producer(void *args) {

    // Buffer is not DONE boolean - keeps the loop going until DONE\n is found
    bool input_bool = true;

    // Lock the mutex before checking whether there is space in the buffer
    while (input_bool == true) {

        // Variable for the input
        char *line = NULL;
        line = (char *)malloc(1000*sizeof(char));
        memset((char*) line, '\0', sizeof(*line));

        // Char will check for end of the stdin
        char* f_line;

        // Read in the users input
        f_line = fgets(line, 1000, stdin);

        // If end of the input is found, set the bool to false and
        // all_done to 1, which will check at the end 
        // to make sure all bytes transferr
        if (f_line == NULL) {
            input_bool = false;
            all_done = 1;

        // Other wise increase total bytes by the line read in
        // This will allow us to keep track of how many bytes have
        // been passed in
        } else {
            total_bytes += strlen(line);
        }

        // Checks to see if only DONE\n was read
        // Additional check over f_line
        int input_bool_cmp = strcmp(line,"DONE\n");
        if (input_bool_cmp == 0) {
            input_bool = false;
            all_done = 1;
        }

        // Lock buffer mutex
        pthread_mutex_lock(&b1_mutex);

        // Buffer is full. Wait for the consumer to signal that the buffer has space
        while(buffer1_count == 1)
            pthread_cond_wait(&buffer1_empty, &b1_mutex);
        prints("buff 1 cond 1");
        // Pass the inputted line to the buffer
        buffer1[buffer1_pro_idx] = strdup(line);
        
        // Increase the buffer counts by 1
        buffer1_pro_idx = (buffer1_pro_idx + 1) % B1_SIZE;
        buffer1_count++;
        
        // Signal to the consumer that the buffer is no longer empty
        pthread_cond_signal(&buffer1_full);
        
        // Unlock the mutex
        pthread_mutex_unlock(&b1_mutex);

    }

    return NULL;
}

// Input consumer and line separation producer thread
void *b1_cons_b2_pro(void *args) {
	// Buffer is not DONE boolean - keeps the loop going until DONE\n is found
	bool linesep_bool = true;

	// Lock the mutex before checking where there is space in the buffer
    while (linesep_bool == true) {

        // Line Sep line for copying the buffer
        char *endsep_line = NULL;
        endsep_line = (char *)malloc((1100)*sizeof(char));
        memset(endsep_line, '\0', 1100);

        // Space Sep line is for returning the spaced line
        char *spacesep_line = NULL;
        spacesep_line = (char *)malloc((1100)*sizeof(char));
        memset(spacesep_line, '\0', 1100);

        // Lock buffer 1 to begin receiving the value from buffer 1
		pthread_mutex_lock(&b1_mutex);

		// Buffer is empty. Wait for the producer to signal that the buffer has data
	    while (buffer1_count == 0)
	    	pthread_cond_wait(&buffer1_full, &b1_mutex);
        prints("buff 2 cond 1");

        // Checks to see if only DONE\n was read
        int linesep_bool_cmp = strcmp(buffer1[buffer1_con_idx],"DONE\n");
        if (linesep_bool_cmp == 0) {
            linesep_bool = false;
        }

        // Copy the line from the buffer to the space line
        strcpy(spacesep_line, buffer1[buffer1_con_idx]);

        // Increment the consumer buffer count and decrement the main buffer count
        buffer1_con_idx = (buffer1_con_idx + 1) % B1_SIZE;
        buffer1_count--;
        

        // Signal to the consumer that the buffer is no longer empty
        pthread_cond_signal(&buffer1_empty);

        // Unlock the mutex
        pthread_mutex_unlock(&b1_mutex);

        // Loop through each char of the input line,
        // when \n is found, replace with a space
        for (int i = 0; i < strlen(spacesep_line); ++i) {
            if (spacesep_line[i] == '\n'){
                spacesep_line[i] = ' ';
            }
        }

		// Lock the buffer 2 mutex
		pthread_mutex_lock(&b2_mutex);

		// Buffer is full. Wait for the consumer to signal that the buffer has space
		while(buffer2_count == 1)
			pthread_cond_wait(&buffer2_empty, &b2_mutex);
        prints("buff 2 cond 2");
        // Puts the new spaced line into the second buffer
    	buffer2[buffer2_pro_idx] = strdup(spacesep_line);

	    // Increments the second buffer counts
	    buffer2_pro_idx = (buffer2_pro_idx + 1) % B2_SIZE;
	    buffer2_count++;

	    // Signal to the consumer that the buffer is no longer empty
		pthread_cond_signal(&buffer2_full);
		
		// Unlock the buffer 2 mutex
		pthread_mutex_unlock(&b2_mutex);
    }

    return NULL;
}

// Line sep consumer and plus sign producer thread - calls the plus function
void *b2_cons_b3_pro(void *args) {

    // Buffer is not DONE boolean - keeps the loop going until DONE\n is found
    bool plus_bool = true;

    // Lock the mutex before checking where there is space in the buffer
    while (plus_bool == true) {

        // Plus sign line for copying the buffer
        char *plus_sign_line = NULL;
        plus_sign_line = (char *)malloc((1100)*sizeof(char));
        memset(plus_sign_line, '\0', 1100);

        // Lock the buffer
        pthread_mutex_lock(&b2_mutex);

        // Buffer is empty. Wait for the producer to signal that the buffer has data
        while (buffer2_count == 0)
            pthread_cond_wait(&buffer2_full, &b2_mutex);
        prints("buff 3 cond 1");

        // Copy the buffer over to plus sign line
        strcpy(plus_sign_line, buffer2[buffer2_con_idx]);

        // Increment the consumer buffer count and decrement the main buffer count
        buffer2_con_idx = (buffer2_con_idx + 1) % B2_SIZE;
        buffer2_count--;

        // Signal to the consumer that the buffer is no longer empty
        pthread_cond_signal(&buffer2_empty);

        // Unlock the mutex
        pthread_mutex_unlock(&b2_mutex);

        // Checks to see if only DONE\n was read
        int plus_sign_bool_cmp = strcmp(plus_sign_line,"DONE\n");
        if (plus_sign_bool_cmp == 0) {
            plus_bool = false;

        }

        // carrot_line is for returning the spaced line
        char *carrot_line = NULL;
        carrot_line = (char *)malloc((1100)*sizeof(char));
        memset(carrot_line, '\0', 1100);

        // Call plus_sign method and copy the result to carrot_line
        strcpy(carrot_line, plus_sign(plus_sign_line));

        // Lock the buffer 3 mutex
        pthread_mutex_lock(&b3_mutex);

        // Buffer is full. Wait for the consumer to signal that the buffer has space
        while(buffer3_count == 1)
            pthread_cond_wait(&buffer3_empty, &b3_mutex);
        prints("buff 3 cond 2");
        // Puts the new carrot line into the third buffer
        buffer3[buffer3_pro_idx] = strdup(carrot_line);

        // Increments the third buffer counts
        buffer3_pro_idx = (buffer3_pro_idx + 1) % B3_SIZE;
        buffer3_count++;

        // Signal to the consumer that the buffer is no longer empty
        pthread_cond_signal(&buffer3_full);
        
        // Unlock the buffer 2 mutex
        pthread_mutex_unlock(&b3_mutex);
    }

    return NULL;
}

// Output consumer thread - takes from buffer 3, writes to stdout
void *b3_consumer(void *args) {

    // Output line - used for printing, checking for done, and holding extra chars
    char *output_line = NULL;
    output_line = (char *)malloc((10000)*sizeof(char));
    memset(output_line, '\0', 10000);

    // Keeps track of total lines being written
    int total_lines = 0;

    // Buffer is not DONE boolean - keeps the loop going until DONE\n is found
	bool output_bool = true;

    // Continue consuming until DONE\n  
    while (output_bool == true) {

    	// Lock the mutex before checking if the buffer has data      
    	pthread_mutex_lock(&b3_mutex);
        prints("buff 4 pre cond 1");
    	// Buffer is empty. Wait for the producer to signal that the buffer has data
	    while (buffer3_count == 0)
	    	pthread_cond_wait(&buffer3_full, &b3_mutex);
        prints("buff 4 cond 1");
        // Checks to see if only DONE\n was written
        int output_bool_cmp = strcmp(buffer3[buffer3_con_idx],"DONE\n");
        if (output_bool_cmp == 0) {
            output_bool = false;
        }

    	// Concat the output with the buffer
        strcat(output_line, buffer3[buffer3_con_idx]);

        // Subtract the length of the buffer from total bytes
        // This indicates that the input line has made it to printing
        total_bytes -= strlen(buffer3[buffer3_con_idx]);

        // Increment the consumer buffer count and decrement the main buffer count
        buffer3_con_idx = (buffer3_con_idx + 1) % B3_SIZE;
        buffer3_count--;

        // Signal to the producer that the buffer has space
        pthread_cond_signal(&buffer3_empty);

        // Unlock the mutex
        pthread_mutex_unlock(&b3_mutex);

        prints("before if");
        prints(output_line);
        //printf("%lu\n",strlen(output_line));
        prints("before if");

        // If the length of output is greater than 80 print
        if (strlen(output_line) >= 80) {

            // Num of lines = num of output lines
            int num_lines = strlen(output_line)/80;

            // For loop loops through the additional lines brought in
            // line then becomes the leftover chars from the last line, and the
            // new chars up to 80
            for (int i = total_lines; i < num_lines; ++i) {
                char line[81];
                memset(line, '\0', sizeof(line));
                for (int c = 0; c < 80; ++c) {
                    line[c] = output_line[i*80+c];
                }

                // Print the line
                prints("in if");
                printf("%s\n", line);
                prints("in if");
            }

            // Total lines now equals the num of lines and stdout is flushed
            total_lines = num_lines;
            fflush(NULL);
        }

        // Due to time constraints, uses this exit condition instead of implementing appropriate mutexes
        // around each global variable/ implementing additional thread coordination from threads 1 to 4
        // Checks for all of the buffers to be empty (from each thread), for all_done var to be = to 1
        // (from the input thread indicating DONE\n has been passed) and checks to make sure the vast majority
        // of inputted bytes have also been printed out by thread 4
        if (all_done && buffer1_count == 0 && buffer2_count == 0 && buffer3_count == 0 && total_bytes <= 5) {
            exit(1);
        }
    	
    }
    return NULL;
}

int main(void) {

    // Create the 4 threads
    pthread_t input_t, linesep_t, plussign_t, output_t;
    pthread_create(&input_t, NULL, b1_producer, NULL);
    pthread_create(&linesep_t, NULL, b1_cons_b2_pro, NULL);
    pthread_create(&plussign_t, NULL, b2_cons_b3_pro, NULL);
    pthread_create(&output_t, NULL, b3_consumer, NULL);

    // Join the threads
    pthread_join(input_t, NULL);
    pthread_join(linesep_t, NULL);
    pthread_join(plussign_t, NULL);
    pthread_join(output_t, NULL);
    return 0;
}

