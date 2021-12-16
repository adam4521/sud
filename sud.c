/********************************************************************************
*** Sudoku solver written in K&R C.
*** Copyright 2021 Adam Jaworski.
*** MIT License
********************************************************************************/


/* Standard library header files needed on all but the most ancient systems. */
#include <stdio.h>
#include <stdlib.h>

#define ROWS 9
#define COLS 9
#define R_ROWS 3               /* region rows */
#define R_COLS 3               /* region columns */
#define MAX_VAL 9
#define O_CELL 1022            /* 0b1111111110 open cell with nothing in it */
#define H_LINE " ------- ------- ------- "   /* horizontal line used when printing output */
#define SOLVED 1               /* bit 0 in a cell is a flag to indicate if a it is solved */

/********************************************************************************
*** Possible values in a cell are indicated as a bit field within an integer value type.
*** so the cell value 0b1111111110 indicates
***                     987654321U the solutions 1-9 are available and the cell is unsolved(U).
*** whereas the value 0b1000010000 indicates
***                     9----4---U the solutions 9 and 4 are possible for this unsolved cell,
*** and the value     0b0000001001 indicates
***                     ------3--S that the cell is solved(S) with the solution 3. 
********************************************************************************/


/* the data structure that holds the 'state' of a solved or unsolved sudoku game */
struct grid { int cells[ROWS][COLS]; int solved_counter; };


/********************************************************************************
*** Initialise a solution grid with all values available in all cells.
********************************************************************************/
struct grid *
grid_zero(g)
  struct grid *g;
{
  int i,j;
  for (i=0; i<ROWS; i++) {
    for (j=0; j<COLS; j++) {
      g->cells[i][j] = O_CELL;
    }
  }
  g->solved_counter = 0;
  return g;
}


/********************************************************************************
*** Make an exact copy of game state from one grid to another.
********************************************************************************/
int
copy_grid(sg, dg)
  struct grid *sg;
  struct grid *dg;
{
  int i,j;
  for (i=0; i<ROWS; i++) {
    for (j=0; j<COLS; j++) {
      dg->cells[i][j] = sg->cells[i][j];
    }
  }
  dg->solved_counter = sg->solved_counter;
}



/********************************************************************************
*** Setter and getter functions for individual cells.
********************************************************************************/
int
set_value(value)
  int value;
{
  return 1<<value | SOLVED;
}

int
get_value(cell)
  int cell;
{
  int i;
  i=0;   /* returns 0 if unsolved */
  if (cell & SOLVED) {
    /* solved */
    for (i=1; i<=MAX_VAL; i++) {
      if (cell & 1<<i) break;
    }
  }
  return i;
}
 

/********************************************************************************
*** Set solved flag if the cell is solved (has only one possible value).
********************************************************************************/
int
mark_if_solved(cell)
  int cell;
{
  int poss;
  int i;
  poss = 0;
  for (i=1; i<=MAX_VAL; i++) {
    if (cell & (1<<i)) poss++;
  }
  if (poss == 1) cell = cell | SOLVED;
  return cell;
}

 
/********************************************************************************
*** Where we have a solved cell, this helper function will clear this value from
*** connected cells in the current row, column or region of the source cell.
*** Returns the number of cell changes it made, or -1 if the grid is invalid
*** (ie at least one cell is wiped out with no possible solution).
********************************************************************************/
int
reduce(g, row, column)
  struct grid *g;
  int row,column;
{
  int source_cell;
  int mask;
  int changed;
  int i,j;
  int *cell, cell_prev;

  source_cell = g->cells[row][column];
  
  /* return immediately if the source cell is not already solved */
  if ((source_cell & SOLVED) == 0) {
    return 0;
  }

  mask = ~(source_cell & O_CELL);  /* could be simply ~source_cell */
  changed = 0;
  for (i=0; i<ROWS; i++) {
    for (j=0; j<COLS; j++) {

      /* pointer to a target cell */
      cell = &g->cells[i][j];

      /* skip over the source cell */
      if (((i==row) && (j==column))) continue;

      /* change unsolved cell possibles if they are on the same row, column or region 
      ** as the source cell AND contain the source value as a possible value */
      if (((i==row) || 
           (j==column) ||
           ((row/R_ROWS == i/R_ROWS) && (column/R_COLS == j/R_COLS))) 
           && (*cell & mask)) {
        cell_prev = *cell;
        *cell = mark_if_solved(*cell & mask);

        /* if we cleared any possibles, increment the changed counter */
        if (*cell != cell_prev) {
          changed++; 
          /* and if we have solved a cell, increment the solved cells counter */
          if (*cell & SOLVED) {
            g->solved_counter++;
          }
        }

        /* if a cell is invalid (has no possible solutions) exit function straightaway */
	if ((*cell == 0) || (*cell == 1)) {
          /* -1 signals to calling function that this grid is invalid */
          return -1;
        }
      } 
    } 
  }
  /* return the number of possibles we cleared */
  return changed;
}


/********************************************************************************
*** Uses reduce() to remove possible values from unsolved cells.
*** Checks every cell in a grid.
********************************************************************************/
int
reduce_grid(g)
  struct grid *g;
{
  int i,j;
  int r,reductions;
  r = 0;
  reductions = 0;
  /* iterate through every cell */
  for (i=0; i<ROWS; i++) {
    for (j=0; j<COLS; j++) {
      /* the reduce() function removes solved cells from possible solutions in
      ** unsolved cells in the grid */
      r = reduce(g,i,j);
      if (r == -1) {
        /* return early if we have an unsolveable cell */
        return -1;
      }
      reductions = reductions + r;
    }
  }
  return reductions; 
}


/********************************************************************************
*** Chooses a cell with the lowest number of possible answers
*** to help optimise the computation (ie find the right answer earlier).
*** Checks every cell in the grid to find out how many possible answers each cell
*** has and picks the one with the lowest number of possibles.
*** Obvs. skips cells that are already solved
********************************************************************************/
int
choose_target_cell(g,r,c)
  struct grid *g;
  int *r,*c;
{
  int i,j,k;  /* current row and column, and k is the possible value iterator */
  int tr,tc;  /* row and column of best target found so far */
  int a,t;    /* a is number of possibles of current cell, t is the number of possibles
                 for best target so far */
  t=MAX_VAL;  /* initialise t to a high number */
  tr=0; tc=0;
  for (i=0; i<ROWS; i++) {
    for (j=0; j<COLS; j++) {
      if (g->cells[i][j] & SOLVED) {
        /* jump over this cell if it's already been solved */
        continue;
      }
      /* figure out how many answers currently possible for this cell */
      a=0;
      for (k=1; k<=MAX_VAL; k++) {
        if (1<<k & g->cells[i][j]) {
          a++;
        }
      }
      if (a<t) {
        /* if we've got a cell with a lower number of possible answers than we found
        ** so far, make this the target cell */
        t = a;
        tr = i;
        tc = j;
      }
    }
  }
  /* return the best target cell we found */
  *r = tr;
  *c = tc;
}


/********************************************************************************
*** Reads a grid of clues from standard input.
*** Expects numbers for each cell horizontally, space or hyphen (-) for unknown
*** and new line for next row.
********************************************************************************/
int
read_grid(g)
  struct grid *g;
{
  char in;
  int i,j;
  for (i=0; i<ROWS; i++) {
    j=0;
    while ((in = getchar())) {
      if (in == '\n') {
        /* advance to next row */
        break;
      } 
      else if ((in == ' ') || (in == '-') || (j>=COLS)) {
        /* advance to next column */
        j++;
      }
      else if ((in >= '1') && (in <= '9')) {
        /* enter clue into the correct column of the starting grid */
        /* g->cells[i][j] = set_value((int) (in - '0')); */
        g->cells[i][j] = set_value(in - '0');
      	g->solved_counter++;
        j++;
      }
      else {
      	/* something was wrong with the input, signal error */
      	return 0;
      }
    }
  }
  return 1;
}


/********************************************************************************
*** Displays the contents of a sudoku grid 
********************************************************************************/
int
print_grid(g)
  struct grid *g;
{
  int i,j;
  int output_value;
  printf("%s\n", H_LINE);
  for (i=0; i<ROWS; i++) {
    printf("| ");

    for (j=0; j<COLS; j++) {
      output_value = get_value(g->cells[i][j]);
      printf(output_value == 0 ? "  " : "%d ", output_value);
      /* end-of-region separator */
      if (j%R_COLS == R_COLS-1) printf("| ");
    }
    printf("\n");

    /* extra space to separate regions */
    if (i%R_ROWS == R_ROWS-1) printf("%s\n", H_LINE);
 }
}


/********************************************************************************
*** Pointers to input and output grids are passed into the function.
*** Returns boolean success or failure.
*** Recursive process, creating local copies of the working grid
*** until the parent call either succeeds (solves all cells) or fails.
********************************************************************************/
int
try(ig, og)
  struct grid *ig;      /* the input grid to try solving */
  struct grid *og;      /* the output grid that we copy into if we succeed */
{
  struct grid wg;       /* a local working copy of the grid */
  int r,reductions;
  int tr,tc;            /* row and column for our working cell */
  int wc;               /* working cell value, for guesses */
  int k;                /* guess iterator */

  copy_grid(ig, &wg);   /* copy the input grid into a local working grid */

  /* the first thing to do is to reduce the possible cell values */
  /* in unsolved cells of the working grid, based on the input clues */
  reductions = 0;
  do {
    r = reduce_grid(&wg);
    /* test for failure */
    if (r == -1) {
      return 0;
    }
    reductions = reductions + r;
  }
  while (r>0);    /* rinse and repeat until we can reduce the grid no further */

  /* check if the sudoku game is solved (all cells solved) */
  /* and if we have solved it, copy the working grid into the output grid */
  /* and return success */ 
  if (wg.solved_counter == ROWS*COLS) {
    copy_grid(&wg, og);   /* copy the working grid into the output grid */
    return 1;
  }

  /* Ok, so we have a grid that is not solved, we're going to have to guess the */
  /* next move. Find a cell with not many options (simple optimisation). */
  choose_target_cell(&wg, &tr, &tc);

  /* Now loop through those possible options, updating the value of the target cell */
  /* each time. Call try() on each potential version of the working grid. */
  wc = wg.cells[tr][tc];

  /* pretend we have solved this cell */
  wg.solved_counter = wg.solved_counter + 1;

  /* and guess each of the possible solutions for it in turn */
  for (k=1; k<=MAX_VAL; k++) {
    if (wc & 1<<k) {
      /* possible: we modify the grid */
      wg.cells[tr][tc] = set_value(k);
      /* now call try with the modified grid */
      if (try(&wg, og)) {
        /* yipee, it worked, we got there! */
        return 1;
      }
    }
  }

  /* meh, if none of the options worked, return failure */
  return 0;
}


int
main()
{
  struct grid ig;       /* data for the input grid (start clues) */
  struct grid og;       /* data for the output grid (solution) */
  grid_zero(&ig);
  grid_zero(&og);

  if (!read_grid(&ig)) {
    printf("Failed to read: invalid input file.\n");
    exit(1);
  }

  /* print the input grid */
  print_grid(&ig);

  /* if try succeeds, we can print the output grid */
  if (try(&ig, &og)) {
    print_grid(&og);
    exit(0);
  }
 
  /* but if we reach here we failed to solve the sudoku */
  printf("Failed to solve.\n");
  exit(1);
}



