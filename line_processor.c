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

// This function reads in from stdin
const char* read_input() {

    // Variable for the input
    char *line = NULL;
    line = (char *)malloc(1000*sizeof(char));
    memset((char*) line, '\0', sizeof(*line));

    // Read in the users input
    fgets(line, 1000, stdin);
  
    return line;
}

// This function exchanges a "\n" for a space
const char* line_separator(char *line) {

	// Declare the return string
	char *return_sep_line = NULL;
    return_sep_line = (char *)malloc((1100)*sizeof(char));
    memset(return_sep_line, '\0', 1100);

    // Copy from paramter and append " "
    strcpy(return_sep_line, line);
    strcat(return_sep_line, " ");
      	
  	// Return new line with space
	return return_sep_line;
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

// This function prints to stdout
const char* write_output(char *line) {

    // Declare the return string which will hold the reamining chars not printed
    char *return_str = NULL;
    return_str = (char *)malloc((1100)*sizeof(char));
    memset(return_str, '\0', 1100);

    // Temp string 1, copy from buffer
    char *temp_str = NULL;
    temp_str = (char *)malloc((1100)*sizeof(char));
    memset(temp_str, '\0', 1100);
    strcpy(temp_str, line);

    // Temp string 2, keep blank for now
    char *temp_str2 = NULL;
    temp_str2 = (char *)malloc((1100)*sizeof(char));
    memset(temp_str2, '\0', 1100);

    // Print string is for printing to output if conditions are satisfied
    char *print_str = NULL;
    print_str = (char *)malloc((1100)*sizeof(char));
    memset(print_str, '\0', 1100);

    // Start by declaring break_loop to false, this assumes that a string is too short
    bool break_loop = false;

    // First check to see if the string pass through buffer is DONE\n
    // if it is, skip to the end
    int output_first_bool_cmp = strcmp(return_str,"DONE\n");
    if (output_first_bool_cmp != 0) {

        // Loop through the buffer string until less than 80 chars are found
        // We will loop as many times as it takes to get to less than 80
        while(break_loop == false){

            // If less than 80 chars found, pass the result to the next buffer and break
            // out of the loop
            if (strlen(temp_str) < 80) {
                strcpy(return_str, temp_str);
                break_loop = true;

            // Else print the string
            } else {
                strncpy(print_str, temp_str, 80);
                fprintf(stdout, "%s",print_str);
                fflush(stdout);

                // This passes the chars after 80 back to temp_str
                // We must do this in case more than 160 chars are found
                strcpy(temp_str2, &temp_str[80]);
                memset(temp_str, '\0', 1100);
                strcpy(temp_str, temp_str2);
            }
        }
    }
    return return_str; 
}

// Input producer thread - calls the read_input function
void *b1_producer(void *args) {
    // Buffer is not DONE boolean - keeps the loop going until DONE\n is found
    bool input_bool = true;

    // Lock the mutex before checking where there is space in the buffer
    while (input_bool == true) {

        // Input line is for checking for DONE\n
        char *input_line = NULL;
        input_line = (char *)malloc((1100)*sizeof(char));
        memset(input_line, '\0', 1100);

        // Call read method and copy the result to input_line
        strcpy(input_line, read_input());

        // Checks to see if only DONE\n was read
        int input_bool_cmp = strcmp(input_line,"DONE\n");
        if (input_bool_cmp == 0) {
            input_bool = false;
        }

        pthread_mutex_lock(&b1_mutex);

        // Buffer is full. Wait for the consumer to signal that the buffer has space
        while(buffer1_count == 1)
            pthread_cond_wait(&buffer1_empty, &b1_mutex);
        
        buffer1[buffer1_pro_idx] = strdup(input_line);
    
        buffer1_pro_idx = (buffer1_pro_idx + 1) % B1_SIZE;
        buffer1_count++;
        
        // Signal to the consumer that the buffer is no longer empty
        pthread_cond_signal(&buffer1_full);
        
        // Unlock the mutex
        pthread_mutex_unlock(&b1_mutex);

    }

    return NULL;
}

// Input consumer and line separation producer thread - calls the line_separator function
void *b1_cons_b2_pro(void *args) {
	// Buffer is not DONE boolean - keeps the loop going until DONE\n is found
	bool linesep_bool = true;

	// Lock the mutex before checking where there is space in the buffer
    while (linesep_bool == true) {

		pthread_mutex_lock(&b1_mutex);

		// Buffer is empty. Wait for the producer to signal that the buffer has data
	    while (buffer1_count == 0)
	    	pthread_cond_wait(&buffer1_full, &b1_mutex);
		
		// Line Sep line for copying the buffer
		char *endsep_line = NULL;
    	endsep_line = (char *)malloc((1100)*sizeof(char));
    	memset(endsep_line, '\0', 1100);

        // Checks to see if only DONE\n was read
        int linesep_bool_cmp = strcmp(buffer1[buffer1_con_idx],"DONE\n");
        if (linesep_bool_cmp == 0) {
            linesep_bool = false;
        }

        // Space Sep line is for returning the spaced line
        char *spacesep_line = NULL;
        spacesep_line = (char *)malloc((1100)*sizeof(char));
        memset(spacesep_line, '\0', 1100);

        // If DONE\n was passed, don't remove \n from the line
        if (linesep_bool == true) {

            // Copy the buffer over but -1 character (carriage return)
            strncpy(endsep_line, buffer1[buffer1_con_idx], strlen(buffer1[buffer1_con_idx])-1);

            // Call linesep method and copy the result to input_line
            strcpy(spacesep_line, line_separator(endsep_line));
        } else {

            // Input was DONE\n - do not remove the carriage return, skip the line sep function
            strcpy(spacesep_line, buffer1[buffer1_con_idx]);
        }
        
        // Increment the consumer buffer count and decrement the main buffer count
        buffer1_con_idx = (buffer1_con_idx + 1) % B1_SIZE;
        buffer1_count--;

        // Signal to the consumer that the buffer is no longer empty
        pthread_cond_signal(&buffer1_empty);

        // Unlock the mutex
        pthread_mutex_unlock(&b1_mutex);

		// Lock the buffer 2 mutex
		pthread_mutex_lock(&b2_mutex);

		// Buffer is full. Wait for the consumer to signal that the buffer has space
		while(buffer2_count == 1)
			pthread_cond_wait(&buffer2_empty, &b2_mutex);

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

        pthread_mutex_lock(&b2_mutex);

        // Buffer is empty. Wait for the producer to signal that the buffer has data
        while (buffer2_count == 0)
            pthread_cond_wait(&buffer2_full, &b2_mutex);
        
        // Plus sign line for copying the buffer
        char *plus_sign_line = NULL;
        plus_sign_line = (char *)malloc((1100)*sizeof(char));
        memset(plus_sign_line, '\0', 1100);

        // Copy the buffer over to plus sign line
        strcpy(plus_sign_line, buffer2[buffer2_con_idx]);

        // Checks to see if only DONE\n was read
        int plus_sign_bool_cmp = strcmp(plus_sign_line,"DONE\n");
        if (plus_sign_bool_cmp == 0) {
            plus_bool = false;
        }

        // Increment the consumer buffer count and decrement the main buffer count
        buffer2_con_idx = (buffer2_con_idx + 1) % B2_SIZE;
        buffer2_count--;

        // carrot_line is for returning the spaced line
        char *carrot_line = NULL;
        carrot_line = (char *)malloc((1100)*sizeof(char));
        memset(carrot_line, '\0', 1100);

        // Call plus_sign method and copy the result to carrot_line
        strcpy(carrot_line, plus_sign(plus_sign_line));

        // Signal to the consumer that the buffer is no longer empty
        pthread_cond_signal(&buffer2_empty);

        // Unlock the mutex
        pthread_mutex_unlock(&b2_mutex);

        // Lock the buffer 3 mutex
        pthread_mutex_lock(&b3_mutex);

        // Buffer is full. Wait for the consumer to signal that the buffer has space
        while(buffer3_count == 1)
            pthread_cond_wait(&buffer3_empty, &b3_mutex);

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

// Output consumer thread - takes from buffer 3, calls the write_output function
void *b3_consumer(void *args) {

    // Output line - used for printing, checking for done, and holding extra chars
    char *output_line = NULL;
    output_line = (char *)malloc((1100)*sizeof(char));
    memset(output_line, '\0', 1100);

    // Buffer is not DONE boolean - keeps the loop going until DONE\n is found
	bool output_bool = true;

    // Continue consuming until DONE\n  
    while (output_bool == true) {

    	// Lock the mutex before checking if the buffer has data      
    	pthread_mutex_lock(&b3_mutex);

    	// Buffer is empty. Wait for the producer to signal that the buffer has data
	    while (buffer3_count == 0)
	    	pthread_cond_wait(&buffer3_full, &b3_mutex);

        // Intermediate Output line - copy for buffer
        char *temp_output_line = NULL;
        temp_output_line = (char *)malloc((1100)*sizeof(char));
        memset(temp_output_line, '\0', 1100);

        // Concatenate the intermediate output with the more global output which holds chars
        // Then reset output_line
        strcat(temp_output_line, output_line);
        memset(output_line, '\0', 1100);

    	// Concat the temp output with the buffer
    	strcat(temp_output_line, buffer3[buffer3_con_idx]);

        // Checks to see if only DONE\n was written
        int output_bool_cmp = strcmp(buffer3[buffer3_con_idx],"DONE\n");
        if (output_bool_cmp == 0) {
            output_bool = false;
        }

        // Increment the consumer buffer count and decrement the main buffer count
        buffer3_con_idx = (buffer3_con_idx + 1) % B3_SIZE;
        buffer3_count--;

        // Signal to the producer that the buffer has space
        pthread_cond_signal(&buffer3_empty);

        // Unlock the mutex
        pthread_mutex_unlock(&b3_mutex);

        // Only call write_output if output_bool is true
        if (output_bool == true) {
            
            // Call write output on the temp output (has the buffer) and return any
            // left over chars to be passed back in when the loop resets
            strcpy(output_line, write_output(temp_output_line));
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

