/* File:    game.c
 * Purpose: Run the Game of Life on multiple threads using OpemMP.
 * Compile: make -f make_game_omp
 * Run:     ./game_omp <grid size> <number of generations> <number of threads>
 * Input:   None
 * Output:  Resultant generation, number of alive cells, and time spent doing calculations.
 *
 * Notes:
 *  1.  Time given in seconds.
 *  2.  Limited number of threads allowed.
 *  3.  The bigger the grid, the bigger the space required in memory.
 */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <omp.h>

// Maximum number of threads allowed (>0)
#define MAX_THREADS 200

// Whether or not to print matrices (1/0)
#define PRINT_OUT 0

// Cell definitions (must be integers and char*)
#define ALIVE 		1
#define DEAD		0
#define ALIVE_CHAR	"▉"
#define DEAD_CHAR	" "

// Datatype for passing arguments
typedef struct Args
{
    int 		**from;
    int 		**to;
    int 		t_number;
    long long	size;
    long long	start;
    int 		threads;
} Args;

// Global variable
int alert_extra_work = 0;

// Functions prototypes
int 		**new_matrix(long long s);
void		delete_matrix(int **m, long long s);
int 		random_number();
int 		read_neighbor(int **m, long long s, long long i, long long j);
void 		process_generation(Args *arg);
void 		print_matrix(int **m, long long s);
long long	cells_alive(int **m, long long size);
double 		GetTime();

// Main funtion
int main(int argc, char* argv[])
{
	srand((unsigned int)time(NULL)); // Random number seeder

	char* usage_msg = "Usage: ./game_pthread size generations threads\n\n\tsize - matrix size (> 0)\n\tgenerations - number of generations to compute\n\tthreads - number of threads (> 0)\n\n";

	// Check the arguments
	if ( argc < 4 )
	{
		printf(usage_msg);

		return EXIT_FAILURE;
	}

	// Get the arguments
	long long	size = atoi(argv[1]);
	int			generations = atoi(argv[2]),
				thread_count = atoi(argv[3]);

	// If no valid arguments were provided
	if ( size == 0 || generations == 0 || thread_count == 0 )
	{
		printf(usage_msg);

		return EXIT_FAILURE;
	}

	// Program variables
	int			**matrix = NULL,
				**next_gen = NULL;
	long long	i,
				j;
	double		begin_serial,
 				end_serial,
 				begin_parallel,
 				end_parallel;

 	// Timestamp when serial part starts
 	begin_serial = GetTime();

 	// Calculate the correct number of threads
	if ( thread_count > MAX_THREADS )
	{
	 	thread_count = MAX_THREADS;

	 	printf("\n[!] Number of threads lowered to %d (maximum threads limit).\n", MAX_THREADS);
	}
	 
	if ( thread_count > size )
	{
	 	thread_count = size;

		printf("\n[!] Number of threads lowered to %lld (matrix rows number).\n", size);
	}
	
	// Define the number of threads
	omp_set_num_threads(thread_count);

	printf("\n> Using %d thread(s) to calculate...\n", thread_count);

	printf("\nGenerating matrix %lldx%lld... ", size, size);

	// Create the matrix in the memory
	matrix = new_matrix(size);

	printf("Done!\n\n");

	printf("Filling out the matrix... ");

    // Randomly fill the matrix
    for ( i=0; i<size; i++ )
    	for ( j=0; j<size; j++ )
    		matrix[i][j] = random_number();

    printf("Done!\n");
    printf("\nProcessing generations... ");

    // Print out the matrix
    if ( PRINT_OUT )
    {
    	printf("\n\nGrid %lldx%lld:\n\n", size, size);
    	print_matrix(matrix, size);
    	printf("\n");
    }

    /************* BEGIN PARALLEL ****************/
 	
 	// Timestamp when parallel part starts
 	begin_parallel = GetTime();

 	// Thread variables
 	int 	t_number;
	Args 	*arg;

 	// Process the generations
    for ( i=0; i<generations; i++ )
    {
    	// Create a new matrix
	    next_gen = new_matrix(size);

	    // Create the threads and run them
		#pragma omp parallel private(t_number, arg)
		{
			t_number = omp_get_thread_num();
			arg = malloc(sizeof(Args));

			arg->from = matrix;
			arg->to = next_gen;
			arg->t_number = t_number;
			arg->size = size;
			arg->start = t_number;
			arg->threads = thread_count;

			process_generation(arg);
		}

    	// Copies next generation to current matrix
	    delete_matrix(matrix, size);
	    matrix = next_gen;
	    next_gen = NULL;

    	// Print it out
    	if ( PRINT_OUT )
	    {
	    	printf("Generation #%d:\n\n", (int) i+1);
	    	print_matrix(matrix, size);
	    	printf("\n");
	    }
    }
    
    // Timestamp when parallel part ends
 	end_parallel = GetTime();
 	
 	/************** END PARALLEL *****************/

    printf("Done!\n");
    printf("\n-> Alive cells at the generation #%d: %lld\n\n", generations, cells_alive(matrix, size));

    // Delete the matrices from the memory
    delete_matrix(matrix, size);

    // Timestamp when serial part ends
 	end_serial = GetTime();

 	// Calculate each time spent
 	double time_parallel = end_parallel - begin_parallel;
 	double time_serial = end_serial - begin_serial;
 	// Show statistics about execution time
 	printf("____________________________________________________\n\n");
 	printf("Execution time (by part):\n\n");
 	printf("- Serial:\t%.3f seconds\n", (double) time_serial-time_parallel);
 	printf("- Parallel:\t%.3f seconds\n", (double) time_parallel);
 	printf("- Total:\t%.3f seconds\n", (double) time_serial);

	// End of the program
	return EXIT_SUCCESS;
}

// Function that generate a matrix dinamycally
int **new_matrix(long long s)
{
	// Create the matrix in the memory
	long long	i;
	int 		**m;

	m = (int**) malloc(s * sizeof(int*));
	
	for ( i=0; i<s; i++ )
        m[i] = (int*) malloc(s * sizeof(int));

    return m;
}

// Function that removes a matrix from the memory
void delete_matrix(int **m, long long s)
{
	if ( m != NULL )
	{
		int	i;

		for ( i=0; i<s; i++ )
		{
			// Delete row in the matrix
			free(m[i]);
			m[i] = NULL;
		}

		// Delete the whole matrix
		free(m);
		m = NULL;
	}
}

// Function that generate and return a random number 0 or 1
int random_number()
{
    if (rand() % 2 == 0)
    	return ALIVE;
    else
    	return DEAD;
}

// Function that reads a neighbor
int read_neighbor(int **m, long long s, long long i, long long j)
{
	if ( i < 0 )
		i = s - 1;
	else if ( i == s )
		i = 0;

	if ( j < 0 )
		j = s - 1;
	else if ( j == s )
		j = 0;

	return m[i][j];
}

// Function that process the next generation
void process_generation(Args *arg)
{
	long long	i,
				j,
				s = arg->size,
				alive_neighbors = 0;
	int			**from = arg->from,
				**to = arg->to,
				t_number = arg->t_number;

	// If it has only one thread
	if ( arg->threads == 1 )
	{
		// Go through the matrix
		for ( i=0; i<s; i++ )
		{
			for ( j=0; j<s; j++ )
			{
				// Calculate the number of neighbors alive
				alive_neighbors = 
					read_neighbor(from, s, i-1,	j-1)	+ // Northwest
	                read_neighbor(from, s, i-1,	j)		+ // North
	                read_neighbor(from, s, i-1,	j+1)	+ // Northeast
	                read_neighbor(from, s, i,	j-1)	+ // West
	                read_neighbor(from, s, i,	j+1)	+ // East
	                read_neighbor(from, s, i+1,	j-1)	+ // Southwest
	                read_neighbor(from, s, i+1,	j)		+ // South
	                read_neighbor(from, s, i+1,	j+1);	  // Southwest

	            //printf("\n> Alive neighbors of [%lld][%lld]: %lld\n", i, j, alive_neighbors);

	            // Apply the rules
	            
	            if ( from[i][j] == ALIVE )
	            {
	                if ( alive_neighbors == 2 || alive_neighbors == 3 )
	                    to[i][j] = ALIVE;
	                else
	                    to[i][j] = DEAD;
	            }
	            else
	            {
	                if ( alive_neighbors == 3 )
	                    to[i][j] = ALIVE;
	                else
	                    to[i][j] = DEAD;
	            }
			}
		}
	}
	// More than one thread
	else
	{
		// Calculate the thread's boundaries
 		int rows = s / arg->threads;
 		int start = rows * arg->start;
 		int end = start + rows;
 		int dif = s - ( arg->threads * rows );
 		
 		// If it is the first thread and the matrix size is odd
 		if ( !alert_extra_work && t_number == 0 && s % 2 != 0 && dif > 0)
 		{
 			end += dif;
 			alert_extra_work = 1;
			
			printf("\n\n[!] Thread #%d was elected to calculate %d extra line(s) (odd matrix size).\n\n", t_number+1, dif);
 		}

 		//printf("\n>> Hello from thread %d\n", t_number+1);
		//printf("\tProcessing rows %d-%d\n", start+1, end);

 		// Go through the matrix
 		for ( i=start; i<end; i++ )
		{
			for ( j=0; j<s; j++ )
			{
				// Calculate the number of neighbors alive
				alive_neighbors = 
					read_neighbor(from, s, i-1,	j-1)	+ // Northwest
	                read_neighbor(from, s, i-1,	j)		+ // North
	                read_neighbor(from, s, i-1,	j+1)	+ // Northeast
	                read_neighbor(from, s, i,	j-1)	+ // West
	                read_neighbor(from, s, i,	j+1)	+ // East
	                read_neighbor(from, s, i+1,	j-1)	+ // Southwest
	                read_neighbor(from, s, i+1,	j)		+ // South
	                read_neighbor(from, s, i+1,	j+1);	  // Southwest

	            //printf("\n> Alive neighbors of [%lld][%lld]: %lld\n", i, j, alive_neighbors);

	            // Apply the rules
	            
	            if ( from[i][j] == ALIVE )
	            {
	                if ( alive_neighbors == 2 || alive_neighbors == 3 )
	                    to[i][j] = ALIVE;
	                else
	                    to[i][j] = DEAD;
	            }
	            else
	            {
	                if ( alive_neighbors == 3 )
	                    to[i][j] = ALIVE;
	                else
	                    to[i][j] = DEAD;
	            }
			}
		}
	}

	// Delete unnecessary structure from the memory
	free(arg);
	arg = NULL;
}

// Function that print out a matrix
void print_matrix(int **m, long long s)
{
	long long	i,
				j;

	for ( i=0; i<s; i++ )
    {
    	for ( j=0; j<s; j++ )
    		if ( m[i][j] == ALIVE )
    			printf(ALIVE_CHAR);
    		else
    			printf(DEAD_CHAR);

    	printf("\n");
    }
}

// Function that calculates the amount of alive cells in a grid
long long cells_alive(int **m, long long s)
{
	long long	i,
				j,
				alives = 0;

	for ( i=0; i<s; i++ )
		for ( j=0; j<s; j++ )
			if ( m[i][j] == ALIVE )
				alives++;

	return alives;
}

// Function that gets the current timestamp
double GetTime()
{
	return (double) clock() / CLOCKS_PER_SEC;
}
