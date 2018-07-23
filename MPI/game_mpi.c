/* File:    game_mpi.c
 * Purpose: Run the Game of Life on multiple processes using MPI.
 * Compile: mpicc -o game_mpi game_mpi.c -std=gnu99
 * Run:     mpirun -np <number of processes> game_mpi  <board size> <generations>
 * Input:   None
 * Output:  Resultant generation, number of alive cells, and time spent doing calculations.
 *
 * Notes:
 *  1.  Time given in seconds.
 *  2.  Limited number of processes allowed.
 *  3.  The bigger the grid, the bigger the space required in memory.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <mpi.h>

// Cell definitions (must be integers and char*)
#define ALIVE 		1
#define DEAD		0
#define ALIVE_CHAR	"▉"
#define DEAD_CHAR	" "

// Data structure for 2-dim array
typedef struct twoD_array
{
	int rows;
	int cols;
	int ** elems;
} twoD_array_t;

// Function prototypes
twoD_array_t	*build_array(twoD_array_t * a, int rows, int cols);
void			free_array(twoD_array_t * a);
int				read_board(FILE* infile, twoD_array_t *board);
int				random_board(int size, int seed, twoD_array_t *board);
void			update_board(twoD_array_t *board, twoD_array_t *new_board);
void			print_board(FILE* outfile, twoD_array_t *board);
void			clear_border(twoD_array_t *board);
int				rows_per_process(int size);
int				local_rows(int proc_id, int size);
int				local_start_row(int proc_id, int size);
int				local_end_row(int proc_id, int size);
int				process_owning_row(int row, int size);
double			get_time();

// Global variables
int nprocs;
int myid;

// Message tags
#define INITIALIZE_TAG	1
#define EXCHANGE_TAG	2
#define PRINT_TAG		3

// Main function
int main( int argc, char* argv[] )
{
	int				steps = 0,
					print_interval = 0,
					size = 0,
					seed,
					return_val,
					max_return_val;

	twoD_array_t	board1,
					board2;
	twoD_array_t	*board = &board1;
	twoD_array_t	*new_board = &board2;

	double			start_time_serial,
					start_time_parallel,
					end_time;

	char			*usage_fmt = "Invalid arguments!\n\nUsage: mpirun -np <number of processes> %s <board size> <generations>\n\n",
					*end_ptr_for_strtol;

	if ( MPI_Init(&argc, &argv) != MPI_SUCCESS )
	{
		fprintf(stderr, "MPI initialization error!\n");

		exit(EXIT_FAILURE);
	}

	MPI_Comm_size(MPI_COMM_WORLD, &nprocs);
	MPI_Comm_rank(MPI_COMM_WORLD, &myid);

	start_time_serial = get_time();

	// Process arguments
	if ( argc < 3 )
	{
		if ( myid == 0 )
			fprintf(stderr, usage_fmt, argv[0]);

		MPI_Finalize();

		exit(EXIT_FAILURE);
	}

    // Randomly generated data
    seed = 0;
    size = atoi(argv[1]);
    steps = atoi(argv[2]);
    print_interval = 0;

    return_val = random_board(size, seed, board);
    MPI_Allreduce(&return_val, &max_return_val, 1, MPI_INT, MPI_MAX, MPI_COMM_WORLD);
    if ( max_return_val != 0 )
    {
        MPI_Finalize();

        exit(EXIT_FAILURE);
    }
 
    // Print initial information
    if ( myid == 0 )
        fprintf(stderr, "Processing board of size %dx%d, %d generations, with %d processes...\n", size, size, steps, nprocs);

    // Create a new board
    if ( build_array(new_board, local_rows(myid, size)+2, size+2) == NULL )
    {
        fprintf(stderr, "Unable to allocate space for board of size %d!\n", size);
        
        MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
    }
    // Clear the board
    clear_border(new_board);

    // Get initial time
    start_time_parallel = get_time();

    // Loop to update and print the board
    for ( int step=0; step<steps; ++step )
    {
        // Update results
        update_board(board, new_board);
        // Swap old and new boards
        twoD_array_t *temp = board;
        board = new_board;
        new_board = temp;
    }

    // Get end time
    end_time = get_time();

    // Print the time results
    if ( myid == 0 )
    {
        fprintf(stderr, "\n- Time serial:\t\t%.4f seconds\n", (double) (start_time_parallel - start_time_serial));
        fprintf(stderr, "- Time parallel:\t%.4f seconds\n", (double) (end_time - start_time_parallel));
    }

    // Free memory
    free_array(board);
    free_array(new_board);

    MPI_Finalize();

    return EXIT_SUCCESS;
}

// Function that build and array
twoD_array_t *build_array( twoD_array_t * a, int rows, int cols )
{
    int * temp;
    a->rows = rows;
    a->cols = cols;
    
    if ( (a->elems = malloc(rows * sizeof(int *))) == NULL )
        return NULL;
    
    if ( (temp = malloc(rows * cols * sizeof(int))) == NULL )
    {
        free (a->elems);
        
        return NULL;
    }

    for ( int row=0; row<rows; ++row, temp+=cols )
        a->elems[row] = temp;
    
    return a;
}

// Function that remove an array from memory
void free_array( twoD_array_t * a )
{
    free(a->elems[0]);
    free(a->elems);
}

// Function that clear unused cells setting zero value
void clear_border( twoD_array_t *board )
{
    for ( int c=0; c<board->cols; ++c )
    {
        board->elems[0][c] = 0;
        board->elems[board->rows-1][c] = 0;
    }

    for ( int r=0; r<board->rows; ++r )
    {
        board->elems[r][0] = 0;
        board->elems[r][board->cols-1] = 0;
    }
}

// Function that read initial configuration from file, and return 0 if OK
int read_board(FILE* infile, twoD_array_t *board) {
    int size, temp, start_row, end_row;

    if ( fscanf(infile, "%d", &size) != 1 )
    {
        if ( myid == 0 )
            fprintf(stderr, "Unable to read size of board!\n");

        return 1;
    }

    start_row = local_start_row(myid, size);
    end_row = local_end_row(myid, size);

    if ( build_array(board, local_rows(myid, size)+2, size+2) == NULL )
    {
        fprintf(stderr, "Unable to allocate space for board of size %d!\n", size);
        
        return 2;
    }

    for ( int i=1; i<=size; ++i )
    {
        for ( int j=1; j<=size; ++j )
        {
            if ( fscanf(infile, "%d", &temp) != 1 )
            {
                fprintf(stderr, "Unable to read values for board!\n");
                
                return 1;
            }

            if ( temp == 0 || temp == 1 )
            {
                if ( start_row <= i && i < end_row )
                    board->elems[i-start_row+1][j] = temp;
            }
            else
            {
                fprintf(stderr, "Unable to read values for board!\n");
                
                return 1;
            }
        }
    }

    clear_border(board);

    return 0;
}

// Function that generate random board, and return 0 if OK
int random_board( int size, int seed, twoD_array_t *board )
{
    int temp, start_row, end_row;

    start_row = local_start_row(myid, size);
    end_row = local_end_row(myid, size);

    if ( build_array(board, local_rows(myid, size)+2, size+2) == NULL )
    {
        fprintf(stderr, "Unable to allocate space for board of size %d!\n", size);
        
        return 2;
    }

    srand(seed);
    
    for ( int i=1; i<=size; ++i )
    {
        for ( int j=1; j<=size; ++j )
        {
            temp = (rand() < (RAND_MAX/2)) ? 0 : 1;

            if ( start_row <= i && i < end_row )
                board->elems[i-start_row+1][j] = temp;
        }
    }

    clear_border(board);

    return 0;
}

// Function that update a board configuration
void update_board( twoD_array_t *board, twoD_array_t *new_board )
{
    int size;
    MPI_Status status;
    MPI_Request req_recv_above, req_recv_below, req_send_above, req_send_below;

    size = board->cols-2;

    // Exchange information with neighbors
    if ( myid != 0 )
    {
        // Receive bottom row from neighbor above
        MPI_Irecv(&(board->elems[0][1]), size, MPI_INT, myid-1, EXCHANGE_TAG, MPI_COMM_WORLD, &req_recv_above);
    }

    if ( myid != (nprocs-1) )
    {
        // Receive top row from neighbor below
        MPI_Irecv(&(board->elems[local_rows(myid, size)+1][1]), size, MPI_INT, myid+1, EXCHANGE_TAG, MPI_COMM_WORLD, &req_recv_below);
    }

    if ( myid != 0 )
    {
        // Send top row to neighbor above
        MPI_Isend(&(board->elems[1][1]), size, MPI_INT, myid-1, EXCHANGE_TAG, MPI_COMM_WORLD, &req_send_above);
    }

    if ( myid != (nprocs-1) )
    {
        // Send bottom row to neighbor below
        MPI_Isend(&(board->elems[local_rows(myid, size)][1]), size, MPI_INT, myid+1, EXCHANGE_TAG, MPI_COMM_WORLD, &req_send_below);
    }

    // Wait for communication to complete
    if ( myid != 0 )
    {
        // Receive bottom row from neighbor above
        MPI_Wait(&req_recv_above, &status);
    }

    if ( myid != (nprocs-1) )
    {
        // Receive top row from neighbor below
        MPI_Wait(&req_recv_below, &status);
    }

    if ( myid != 0 )
    {
        // Send top row to neighbor above
        MPI_Wait(&req_send_above, &status);
    }

    if ( myid != (nprocs-1) )
    {
        // Send bottom row to neighbor below
        MPI_Wait(&req_send_below, &status);
    }

    // Update board
    for ( int i=1; i<=board->rows-2; ++i )
    {
        for ( int j=1; j<=board->cols-2; ++j )
        {
            int nbrs =
		                board->elems[i-1][j-1]	+
		                board->elems[i-1][j]	+
		                board->elems[i-1][j+1]	+
		                board->elems[i][j-1]	+
		                board->elems[i][j+1]	+
		                board->elems[i+1][j-1]	+
		                board->elems[i+1][j]	+
		                board->elems[i+1][j+1];

            if ( board->elems[i][j] == 1 )
            {
                if ( nbrs == 2 || nbrs == 3 )
                    new_board->elems[i][j] = ALIVE;
                else
                    new_board->elems[i][j] = DEAD;
            }
            else
            {
                if ( nbrs == 3 )
                    new_board->elems[i][j] = ALIVE;
                else
                    new_board->elems[i][j] = DEAD;
            }
        }
    }
}

// Function that print the current board configuration
void print_board(FILE* outfile, twoD_array_t *board)
{
    int size, temp, process_for_row;
    int *temprow;
    MPI_Status status;

    size = board->cols-2;

    // Data from process zero
    if ( myid == 0 )
    {
        temprow = malloc(size * sizeof(int));
        
        if ( temprow == NULL )
        {
            fprintf(stderr, "Unable to allocate space for printing board!\n");
            
            MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
        }

        for ( int i=1; i<=size; ++i )
        {
            process_for_row = process_owning_row(i, size);
            
            if ( process_for_row != 0 )
                MPI_Recv(temprow, size, MPI_INT, process_for_row, PRINT_TAG, MPI_COMM_WORLD, &status);
            
            for ( int j=1; j<=size; ++j )
            {
                if ( process_for_row == 0 )
                    temp = board->elems[i][j];
                else
                    temp = temprow[j-1];
                if ( temp == 0 )
                    fprintf(outfile, DEAD_CHAR);
                else
                    fprintf(outfile, ALIVE_CHAR);
            }

            fprintf(outfile, "\n");
        }

        free(temprow);
    }
    // Other processes send to procces zero
    else
    {
        for ( int i=1; i<=local_rows(myid, size); ++i )
            MPI_Send(&(board->elems[i][1]), size, MPI_INT, 0, PRINT_TAG, MPI_COMM_WORLD);
    }
}

// Function that get the number of rows per process
int rows_per_process( int size )
{
    return (size + (nprocs-1)) / nprocs;
}

// Function that get the number of rows for current process
int local_rows( int proc_id, int size )
{
    if ( proc_id == (nprocs-1) )
        return size - ((nprocs-1) * rows_per_process(size));
    else
        return rows_per_process(size);
}

// Function that return local start row (absolute row number)
int local_start_row( int proc_id, int size )
{
    return proc_id * rows_per_process(size) + 1;
}

// Function that return local end row (absolute row number)
int local_end_row( int proc_id, int size )
{
    if ( proc_id == (nprocs-1) )
        return size + 1;
    else
        return local_start_row(proc_id, size) + rows_per_process(size);
}

// Function that return ID of process owning row
int process_owning_row( int row, int size )
{
    return (row-1) / rows_per_process(size);
}

// Function to get time in seconds
double get_time()
{
	struct timeval tv;
	if ( gettimeofday(&tv, NULL) == 0 )
		return (double) tv.tv_sec + ((double) tv.tv_usec / (double) 1000000);
    else
        return 0.0;
}

