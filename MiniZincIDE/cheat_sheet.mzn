                      %%%%%%%%%%%%%%%%%%%%%%%%
                      %                      %
                      % MiniZinc cheat sheet %
                      %                      %
                      %%%%%%%%%%%%%%%%%%%%%%%%

/******************
* Model structure *
******************/

include "globals.mzn";        % use globals library

int: n;                       % fixed integer parameter
var 1..n: x;                  % integer decision variable
array[1..n] of var 1..n: y;   % array of integer decision variables
constraint sum(y) <= x;       % constraint

solve satisfy;                % find satisfying solution

output ["Solution:\n", "x = ", show(x),  % output list of strings
        "y = \(y)"];                     % interpolation \(expression)

/***********************
* Basic types          *
***********************/

int: i;
3..5: j;         % integer

float: f;
3.0..5.0: g;     % floating point number

bool: b;         % boolean

set of int: s;   % set
set of 3..5: t;

/***********************
* Type-insts           *
***********************/

var int: x;           % declare variable 
                      % (also with float, bool, and set of int)
var 3..7: y;          % declare variable with domain (also 3.0..7.0)
var set of 10..20: s; % declare set variable (only fixed set of int!)

array[1..4,1..10] of var 0.0..100.0: f;
                      % declare 2d array of float variables

var opt 1..10: ox;    % declare optional int variable (can be 1..10 or
                      % absent)

/*********************
* Basic Constraints  *
*********************/
constraint x = y;
constraint x < y;
constraint x <= y;
constraint x > y;
constraint x >= y;
constraint x != y;    % not equals
   
/**********************
* Logical Connectives *
**********************/
                                   % conditionals
int: d = if i > 10 then a elseif i > 0 then b else c endif;
constraint if x < y then y < z else y > z endif;
   
constraint x < y \/ y != z;        % logical "or"
constraint x < y /\ y != z;        % logical "and"
constraint x < y -> y != z;        % logical implication
constraint not (x < y /\ y > z);   % logical negation

/******************
* Set Constraints *
******************/
   
constraint s subset t;             % non-strict subset relation
constraint s intersect t subset w; % intersection
constraint s union t subset w;     % union

/***************************
* Predicates and Functions *
***************************/
   
constraint all_different(x);       % predicate call
constraint mydiv(x,y) = 2;         % function call

/********************************
* Comprehensions and generators *
********************************/

array[int] of int: a = [ i | i in 1..10];
                      % create array [1,2,3,4,5,6,7,8,9,10]

array[int] of int: a = [ i | i in 1..10 where i mod 3=0];
                      % create array [3, 6, 9]

constraint forall (i,j in 1..n where i<j) (x[i] != x[j]);
                      % is the same as
constraint forall ([ x[i] != x[j] | i,j in 1..n where i<j ]);

/****************************
* Array index set coercions *
****************************/

array[20..30] of int: i = array1d(20..30, x);

array[int,int] of float: x =
  array2d(1..10, 1..10, [ 0.0 | i,j in 1.10 ]);

/********************************
* Search and search annotations *
********************************/

solve maximize sum(x);        % optimization
solve minimize sum(x);

solve ::int_search([x,y,z], input_order, indomain_min, complete) satisfy;
                      % search annotation
                      % variable selection (examples):
                      %   input_order, first_fail, max_regret, smallest
                      % value selection (examples):
                      %   indomain_min, indomain_max, indomain,
                      %   indomain_split, indomain_reverse_split
                      % similar for bool_search, set_search, float_search

solve ::seq_search([int_search(x, first_fail, indomain, complete),
                    int_search(y, input_order, indomain_min, complete)])
      satisfy;
                      % first search x, then y



/*********************
* Strings and output *
**********************/

                      % concatenation using ++
                      % join arrays using string ", "
                      % convert expression into string using show
string: s = "a" ++ join(", ",[show(x[i]) | i in 1..3]);

/****************************************
* User-defined predicates and functions *
****************************************/

predicate no_overlap(var int: start0, var int: duration0,
                     var int: start1, var int: duration1) =
  start0 + duration0 <= start1 \/ start1 + duration1 <= start0;

function var float: average(array[int] of var int: x) =
  sum(x) / length(x);

