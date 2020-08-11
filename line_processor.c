// daviryan@oregonstate.edu
// Line Processor - Processes lines using a multithreaded consumer/producer system

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdbool.h>

// Size of the buffer
#define B1_SIZE 10

// #define OUT_SIZE 10000

// Buffers, shared resources
char *buffer1[B1_SIZE];
// Number of items in the buffer, shared resource
int buffer1_count = 0;
// Index where the producer will put the next item
int buffer1_pro_idx = 0;
// Index where the consumer will pick up the next item
int buffer1_con_idx = 0;
// Output string that is used to hold the output buffers.
// char *output_string;

// Initialize the mutex
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

// Initialize the condition variables
pthread_cond_t buffer1_full = PTHREAD_COND_INITIALIZER;
pthread_cond_t buffer1_empty = PTHREAD_COND_INITIALIZER;


// This function reads in from stdin
const char* read_input() {

	// Variable for the input
    char *line = NULL;
    line = (char *)malloc(1000*sizeof(char));
    memset((char*) line, '\0', sizeof(*line));

    // Array to hold the line and the boolean 

    // Read in the users input
    fgets(line, 1000, stdin);
    buffer1[buffer1_pro_idx] = strdup(line);
    buffer1_pro_idx = (buffer1_pro_idx + 1) % B1_SIZE;
    buffer1_count++;
  
	return line;
}

// // This function exchanges a "\n" for a space
// const char* line_separator(char *line) {

// 	// Creates a new input in order to copy the line over
// 	char *space_line = NULL;
//     space_line = (char *)malloc((strlen(line))*sizeof(char));
//     memset(space_line, '\0', strlen(line));
    
//     // Copies the line w/o "\n" then appends a space
//     memcpy(space_line, line, strlen(line)-1);
//     strcat(space_line, " ");
  	
//   	// Return the new line
// 	return space_line;
// }

// // This function exchanges a "\n" for a space
// // This code is modified from my Smallsh.c program where I replace "$$" with PID
// const char* plus_sign(char *line) {

// 	// Creates a new input in order to copy the line over
// 	char *plus_line = NULL;
//     plus_line = (char *)malloc((strlen(line)+1)*sizeof(char));
//     memset(plus_line, '\0', strlen(line)+1);
//     memcpy(plus_line, line, strlen(line));
//     printf("%s\n",line);
    
//     // We need to first go through the line and see if there are any 
//     // "++" that need to be converted to the carrot "^"
//     int plus_count = 0;
//     int plus_pos = 0;
//     while(plus_pos < strlen(plus_line)) {
//         int convert_plus = strncmp(&plus_line[plus_pos],"++",2);
//         if (convert_plus == 0) {
//             plus_count++;
//             plus_pos++;
//         }
//         plus_pos++;
//     }

//     // Multiply the number of instances of ++ with the len of ^
//     // This will be used to shorten the line
//     long plus_length = plus_count;
//     plus_pos = 0;

//     // Now create a new line var with this new length
//     char *carrot_line = NULL;
//     carrot_line = (char *)malloc((strlen(line)-plus_length)*sizeof(char));
//     memset(carrot_line, '\0', strlen(line)-plus_length);

//     // If plus_count is < 1, that means no "++" were found, thus just copy over the
//     // input from line
//     if (plus_count < 1) {
//         memcpy(carrot_line, line, strlen(line));

//     } else {

//         // Start with a blank line
//         strcpy(carrot_line,"");

//         // For loop goes through each iteration of "++"
//         // Each time a "++" is found, restart_loop is set to true
//         for (int i = 0; i < plus_count; ++i) {
//         	bool restart_loop = false;
//             while(restart_loop == false){

//                 // While loop goes through looking for $$, then concatenates
//                 // the string with the ^ - sets the restart bool to true
//                 int convert_plus = strncmp(&plus_line[plus_pos],"++",2);
//                 if (convert_plus == 0) {
//                     strncat(carrot_line, plus_line, plus_pos);
//                     strcat(carrot_line, "^");
//                     plus_line = plus_line + plus_pos + 2;
//                     restart_loop = true;
//                 }
//                 plus_pos++;
//             }
//             plus_pos = 0; 
//         }

//         // Finally concatenate the rest of the string if it isn't null
//         if (strlen(plus_line) != 0) {
//             strcat(carrot_line, plus_line);
//         }
//     }

//   	// Return the new line
// 	return carrot_line;
// }

/*
 Function that the producer thread will run. Produce an item and put in the buffer only if there is space in the buffer. If the buffer is full, then wait until there is space in the buffer.
*/
void *b1_producer(void *args) {
	// Buffer is not DONE boolean - keeps the loop going until DONE\n is found
	bool input_bool = true;

    while (input_bool == true) {
		// Lock the mutex before checking where there is space in the buffer
		pthread_mutex_lock(&mutex);

		// Buffer is full. Wait for the consumer to signal that the buffer has space
		while(buffer1_count == B1_SIZE) {
			pthread_cond_wait(&buffer1_empty, &mutex);
		}
		
		char *input_line = NULL;
    	input_line = (char *)malloc((strlen(input_line)+1)*sizeof(char));
    	memset(input_line, '\0', strlen(input_line)+1);

    	strcpy(input_line, read_input());

    	int input_bool_cmp = strcmp(input_line,"DONE\n");
        if (input_bool_cmp == 0) {
            input_bool = false;
        }

		// Signal to the consumer that the buffer is no longer empty
		pthread_cond_signal(&buffer1_full);
		// Unlock the mutex
		pthread_mutex_unlock(&mutex);
    }
    return NULL;
}

// This function prints to stdout
const char* write_output() {

    // strcat(output_string, buffer1[buffer1_con_idx]);

    // char *out_line = NULL;
    // out_line = (char *)malloc(1000*sizeof(char));
    // memset((char*) out_line, '\0', sizeof(*out_line));

    // strcpy(out_line, buffer1[buffer1_con_idx]);

    // if (strlen(output_string) >= 80) {
    // 	char print_string[81];
    //     strncpy(print_string, output_string, 80);
    //     output_string = output_string + 80;
    //     fprintf(stdout,"%s",print_string);
    // }

    char *out_line = NULL;
    out_line = (char *)malloc(1000*sizeof(char));
    memset((char*) out_line, '\0', sizeof(*out_line));

    strcpy(out_line, buffer1[buffer1_con_idx]);
    fprintf(stdout, "%s",out_line);


    buffer1_con_idx = (buffer1_con_idx + 1) % B1_SIZE;
    buffer1_count--;
  
	return out_line;
}

/*
 Function that the consumer thread will run. Get  an item from the buffer if the buffer is not empty. If the buffer is empty then wait until there is data in the buffer.
*/
void *b3_consumer(void *args) {
    // Buffer is not DONE boolean - keeps the loop going until DONE\n is found
	bool output_bool = true;

    // Continue consuming until the END_MARKER is seen    
    while (output_bool == true) {

    	// Lock the mutex before checking if the buffer has data      
    	pthread_mutex_lock(&mutex);
	    while (buffer1_count == 0) {
	    	// Buffer is empty. Wait for the producer to signal that the buffer has data
	    	pthread_cond_wait(&buffer1_full, &mutex);
	    }

    	char *output_line = NULL;
    	output_line = (char *)malloc((strlen(output_line)+1)*sizeof(char));
    	memset(output_line, '\0', strlen(output_line)+1);

    	strcpy(output_line, write_output());

    	int output_bool_cmp = strcmp(output_line,"DONE\n");
        if (output_bool_cmp == 0) {
            output_bool = false;
        }
    	// Signal to the producer that the buffer has space
    	pthread_cond_signal(&buffer1_empty);
    	// Unlock the mutex
    	pthread_mutex_unlock(&mutex);
    }
    return NULL;
}

int main(void) {
	// output_string = (char *)malloc((OUT_SIZE+1)*sizeof(char));
	// memset(output_string, '\0', OUT_SIZE);


    // Create the input and output threads
    pthread_t input_t, output_t;
    pthread_create(&input_t, NULL, b1_producer, NULL);
    // Sleep for a few seconds to allow the producer to fill up the buffer. This has been put in to demonstrate the the producer blocks when the buffer is full. Real-world systems won't have this sleep    
    // sleep(5);
    pthread_create(&output_t, NULL, b3_consumer, NULL);
    pthread_join(input_t, NULL);
    pthread_join(output_t, NULL);
    return 0;
}

