include "globals.mzn";

int : n = 5;
set of int : S = 1..n;

array [S,S] of var S : a;
array [S,S] of var S : b;
array [S,S] of var S : c;

var S : aa;
var S : bb;
var S : cc;

constraint forall (x,y in S) (alldifferent([a[x,y], b[x,y], c[x,y]]));

constraint forall (x,y in S) (aa != a[x,y]);
constraint forall (x,y in S) (bb != b[x,y]);
constraint forall (x,y in S) (cc != c[x,y]);

solve satisfy;

output [
show(a), show(aa),
show(b), show(bb),
show(c), show(cc)
];
