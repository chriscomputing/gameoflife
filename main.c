#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>
#include <time.h>
#include <omp.h>

#define NROWS 10
#define NCOLS 10
#define MAX_GENS 10
#define GPS_RES 1
#define SPAWN_CHANCE 45

long nrows = NROWS;               // Number of rows
long ncols = NCOLS;               // Number of columns
long max_gens = MAX_GENS;         // Number of maximum generations (iterations)
long gps_res = GPS_RES;           // Update frequency of data outputs (every n-th iteration)
long spawn_chance = SPAWN_CHANCE; // Spawn chance in %

int _correct_long_parse(long val, char *endptr)
{
    if (errno != 0 || *endptr != '\0' || val < INT_MIN || val > INT_MAX)
    {
        return 0; // Indicate failure
    }
    return 1;
}

void _allocate_board(int ***board)
{
    // Allocate columns
    *board = (int **)malloc(nrows * sizeof(int *));

    // Allocate rows
    for (int i = 0; i < nrows; i++)
    {
        (*board)[i] = (int *)malloc(ncols * sizeof(int));
    }
}

void _initialize_board(int ***board)
{
    srand(time(NULL));
    for (int i = 0; i < nrows; i++)
    {
        for (int j = 0; j < ncols; j++)
        {
            (*board)[i][j] = (rand() % 100 < SPAWN_CHANCE) ? 1 : 0;
        }
    }
}

long _alive_cells(int ***board)
{
    long alive = 0;
    for (int i = 0; i < nrows; i++)
    {
        for (int j = 0; j < ncols; j++)
        {
            if ((*board)[i][j] == 1)
            {
                alive += 1;
            }
        }
    }

    return alive;
}

void _time_report(double *times)
{
    // Sum up sim time
    double sum = 0;
    for (long i = 0; i < max_gens; i++)
    {
        sum += times[i];
    }

    // Get avg time per iteration
    double avg = sum / max_gens;

    printf("Total simulation time: %.3f seconds\n"
           "Average time per step: %.3f seconds\n"
           "|========================================================================|\n",
           sum, avg);
}

void _report_game(int ***board, long t)
{
    long alive = _alive_cells(board);
    long cells = nrows * ncols;
    long dead = cells - alive;

    printf("Total cells: %ld\n"
           "Cells alive: %ld (%.3f %%)\n"
           "Cells dead: %ld (%.3f %%)\n"
           "Generation: %ld\n"
           "|========================================================================|\n",
           cells, alive, ((double)alive / (double)cells) * 100, dead, ((double)dead / (double)cells) * 100, t);
}

void run(int ***board)
{
    double times[max_gens];

    for (long t = 0; t < max_gens; t++)
    {
        if (t % gps_res == 0) // Print report to stdout
        {
            _report_game(board, t);
        }
        // Run a single iteration
        int start = omp_get_wtime();
        for (int i = 0; i < nrows; i++)
        {
            for (int j = 0; j < ncols; j++)
            {
                /**
                 * Collect neighbors
                 * [0][1][2]
                 * [3][x][4]
                 * [5][6][7]
                 */
                int neighbors[8] = {
                    (i - 1 > 0 && j - 1 > 0) ? (*board)[i - 1][j - 1] : 0,         // Top left
                    (j - 1 > 0) ? (*board)[i][j - 1] : 0,                          // Top center
                    (i + 1 < nrows && j - 1 > 0) ? (*board)[i + 1][j - 1] : 0,     // Top right
                    (i - 1 > 0) ? (*board)[i - 1][j] : 0,                          // Left center
                    (i + 1 < nrows) ? (*board)[i + 1][j] : 0,                      // Right center
                    (i - 1 > 0 && j + 1 < ncols) ? (*board)[i - 1][j + 1] : 0,     // Left bottom
                    (j + 1 < ncols) ? (*board)[i][j + 1] : 0,                      // Center bottom
                    (i + 1 < nrows && j + 1 < ncols) ? (*board)[i + 1][j + 1] : 0, // Center right
                };
                // TODO: If-branches are bad for GPUs, use ternary instead?
                int counter = 0;
                for (int n = 0; n < 8; n++)
                {
                    // Increase counter by one, if neighbour is alive
                    if (neighbors[n] == 1)
                    {
                        counter += 1;
                    }
                }

                // Determine state at t+1 for current cell
                if (counter < 2 && (*board)[i][j] == 1) // Rule 1: Cells with fewer than 2 live neighbors die
                {
                    (*board)[i][j] = 0;
                }
                else if (counter > 3 && (*board)[i][j] == 1) // Rule 3: Cells with more than 3 live neighbors die
                {
                    (*board)[i][j] = 0;
                }
                else if ((*board)[i][j] == 0 && counter == 3) // Rule 4: Dead cells with 3 live neighbors come to life
                {
                    (*board)[i][j] = 1;
                }
                // All other combinations of state and neighbor count result in no change to board
            }
        }
        double endtime = omp_get_wtime();
        times[t] = endtime - start;
    }
    _report_game(board, max_gens);
    _time_report(times);
}

void _deallocate_board(int **board)
{
    for (int i = 0; i < nrows; i++)
    {
        free(board[i]);
    }

    free(board);
}

// Expected cli: ./gol <nrows> <ncols> <max_gens> <gps_res>
int main(int argc, char const *argv[])
{
    // Check for args, keep defaults if none given
    if (argc > 1)
    {
        if (argc == 6)
        {
            long res;
            char *endptr;
            errno = 0;
            // Assign nrows
            res = strtol(argv[1], &endptr, 10);
            if (_correct_long_parse(res, endptr))
            {
                nrows = res;
            }
            // Assign ncols
            res = strtol(argv[2], &endptr, 10);
            if (_correct_long_parse(res, endptr))
            {
                ncols = res;
            }
            // Assign max_gens
            res = strtol(argv[3], &endptr, 10);
            if (_correct_long_parse(res, endptr))
            {
                max_gens = res;
            }
            // Assign gps_res
            res = strtol(argv[4], &endptr, 10);
            if (_correct_long_parse(res, endptr))
            {
                gps_res = res;
            }
            res = strtol(argv[5], &endptr, 10);
            if (_correct_long_parse(res, endptr))
            {
                spawn_chance = res;
            }
        }
        else
        { // incorrect call
            printf("Run this programm either with \n ./gol or \n ./gol <nrows> <ncols> <max_gens> <gps_res> <spawn_chance>\n");
            return -1;
        }
    }

    printf("|========================================================================|\n");
    printf("Welcome to Conway's Game of Life!\n");
    int **board;
    printf("Creating world...\n");
    _allocate_board(&board);
    printf("Populating world...\n");
    _initialize_board(&board);
    printf("|========================================================================|\n");
    run(&board);

    _deallocate_board(board);
    return 0;
}
