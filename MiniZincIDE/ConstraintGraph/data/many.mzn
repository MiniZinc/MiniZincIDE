include "globals.mzn";

int : n = 5;
set of int : S = 1..n;

array [S,S] of var S : a;
array [S,S] of var S : b;
array [S,S] of var S : c;
array [S,S] of var S : d;
array [S,S] of var S : e;
array [S,S] of var S : f;
array [S,S] of var S : g;
array [S,S] of var S : h;
array [S,S] of var S : i;
array [S,S] of var S : j;

var S : aa;
var S : bb;
var S : cc;
var S : dd;
var S : ee;
var S : ff;
var S : gg;
var S : hh;
var S : ii;
var S : jj;


constraint forall (x,y in S) (alldifferent([a[x,y], b[x,y], c[x,y]]));
constraint forall (x,y in S) (alldifferent([d[x,y], e[x,y], f[x,y]]));
constraint forall (x,y in S) (alldifferent([g[x,y], h[x,y], i[x,y], j[x,y]]));

constraint forall (x,y in S) (aa != a[x,y]);
constraint forall (x,y in S) (bb != b[x,y]);
constraint forall (x,y in S) (cc != c[x,y]);
constraint forall (x,y in S) (dd != d[x,y]);
constraint forall (x,y in S) (ee != e[x,y]);
constraint forall (x,y in S) (ff != f[x,y]);
constraint forall (x,y in S) (gg != g[x,y]);
constraint forall (x,y in S) (hh != h[x,y]);
constraint forall (x,y in S) (ii != i[x,y]);
constraint forall (x,y in S) (jj != j[x,y]);

solve satisfy;

output [
show(a), show(aa),
show(b), show(bb),
show(c), show(cc),
show(d), show(dd),
show(e), show(ee),
show(f), show(ff),
show(g), show(gg),
show(h), show(hh),
show(i), show(ii),
show(j), show(jj)
];