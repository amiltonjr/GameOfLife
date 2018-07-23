File:    game.c
Purpose: Run the Game of Life on a single thread.
Compile: make -f make_game
Run:     ./game <grid size> <number of generations>
Input:   None
Output:  Resultant generation, number of alive cells, and time spent doing calculations.

Notes:
 1.  Time given in seconds.
 2.  Limited number of threads allowed.
 3.  The bigger the grid, the bigger the space required in memory.

__________________________________________________________________________________________

File:    game.c
Purpose: Run the Game of Life on multiple threads using POSIX thread.
Compile: make -f make_game_pthread
Run:     ./game_pthread <grid size> <number of generations> <number of threads>
Input:   None
Output:  Resultant generation, number of alive cells, and time spent doing calculations.

Notes:
 1.  Time given in seconds.
 2.  Limited number of threads allowed.
 3.  The bigger the grid, the bigger the space required in memory.

__________________________________________________________________________________________

File:    game.c
Purpose: Run the Game of Life on multiple threads using OpemMP.
Compile: make -f make_game_omp
Run:     ./game_omp <grid size> <number of generations> <number of threads>
Input:   None
Output:  Resultant generation, number of alive cells, and time spent doing calculations.

Notes:
 1.  Time given in seconds.
 2.  Limited number of threads allowed.
 3.  The bigger the grid, the bigger the space required in memory.
 