/// ******* data *******

var nodes = [{id:0}, {id:1}, {id:2}, {id:3}, {id:4}, {id:5}, {id:6}, {id:7}];
var links1 = [{source: 0, target: 1}, {source: 0, target: 2}, {source: 0, target: 3}, {source: 1, target: 3},
             {source: 2, target: 3}, {source: 3, target:4}];
var links2 = [{source: 5, target: 6}, {source: 6, target: 7}];
var links = [].concat(links1, links2);

/// ****** algorithm *******

var marks = {};
var ways = {};
var clusters = 0;

for (var i in links){
    var link = links[i];
    var n1 = nodes[link.source];
    var n2 = nodes[link.target];
    link.source = n1;
    link.target = n2;
    
    if (ways[n1.id]) 
        ways[n1.id].push(n2);
    else
        ways[n1.id] = [n2];
        
    if (ways[n2.id]) 
        ways[n2.id].push(n1);
    else
        ways[n2.id] = [n1];
}

/// clusterization

for (var i in nodes){
    var node = nodes[i];
    if (marks[node.id]) continue;
    explore_node(node, true);
}

function explore_node(n, is_new){
    if (marks[n.id] !== undefined) return;
    console.log("exploring: ", n);
    if (is_new) clusters++;
    marks[n.id] = clusters;
    var adjasent = ways[n.id];
    console.log(adjasent);
    if (!adjasent) return;
    
    for (var j in adjasent){
        explore_node(adjasent[j], false);
    }
}

marks;