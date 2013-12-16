DrawingEngine.VAR_SIZE = 20;
DrawingEngine.PADDING = 5;
DrawingEngine.ARR_SIZE = 30;
DrawingEngine.C_SIZE = 12;

DrawingEngine.log_svg_elements = false;
DrawingEngine.log_hover = true;
DrawingEngine.allow_drag = true;

DrawingEngine.counter = 0;

var v_nodes;
var c_nodes;
var a_nodes;
var exp_a_nodes;
var s_links;
var q_link;
var two_dim_array_e;
var min_a_btns;

var nodeMouseDown = false;

function DrawingEngine(){
	DrawingEngine._self = this; // not sure if is needed

  this.width = window.innerWidth;
  this.height = window.innerHeight;
  // this.width = 1600;
  // this.height = 1200;
  // this.cola_obj = cola.d3adaptor().avoidOverlaps(true).size([this.width, this.height]);
  this.cola_obj = cola.d3adaptor().size([this.width, this.height]);
}

DrawingEngine.prototype.init_svg = function (){
  if (!this.svg) {
    this.svg = d3.select("#content").append("svg").attr("width", this.width).attr("height", this.height);
    this.svg.call(d3.behavior.zoom().on("zoom", (function(caller) {
        return function() {caller._apply_zooming.apply(caller, arguments);};
      })(this)));
    this.vis = this.svg.append('g');
    this.edgesLayer = this.vis.append("g");
    this.nodesLayer = this.vis.append("g");
    this.buttonsLayer = this.vis.append("g");
  }
};

DrawingEngine.prototype._apply_zooming = function(){
  if (nodeMouseDown) return;
  this.vis.attr("transform", "translate(" + d3.event.translate + ")" + " scale(" + d3.event.scale + ")");
};

DrawingEngine.prototype.draw = function(){
  this.cola_obj.nodes([].concat(data.constraint_nodes, shown_v)).links(cola_links).start(5);

  this._draw_single_variables();
  this._draw_array_nodes();
  this._draw_expanded_arrays();
  this._draw_constraint_nodes();
  this._draw_links();

  this.cola_obj.on("tick", (
    function(caller) {
      return function() {caller._update_drawing.apply(caller, arguments);};
    }
  )(this));
};

DrawingEngine.prototype._draw_single_variables = function(){
    v_nodes = this.nodesLayer.selectAll(".v_node")
    .data(shown_v.filter(function(v) {
      return (v.type !== "arr");
    }));

    v_nodes.enter().append("circle")
    .attr("class", "v_node")
    .attr("r", DrawingEngine.VAR_SIZE / 2)
    .each(function(d) {d.svg_element = this;})
    .on("mouseover", function (d) {
      DrawingEngine.highlight_var(d);
    })
    .on("mousedown", function () { nodeMouseDown = true; } )
    .on("mouseup", function() { nodeMouseDown = false; })
    .on("mouseleave", function (d) {
      DrawingEngine.unhighlight_all();
    })
    .append("title").text(function (d) { return d.name; });

    if (DrawingEngine.allow_drag)
      v_nodes.call(this.cola_obj.drag);

    v_nodes.exit().remove();
};

DrawingEngine.prototype._draw_constraint_nodes = function(){
  c_nodes = this.nodesLayer.selectAll(".c_node")
    .data(data.constraint_nodes);

  c_nodes.each(function(d) { d.svg_element = this; }); // because svg_elements becomes undefined for some reason

  c_nodes.enter().append("path")
    .attr("class", "c_node")
    .on("mouseover", function (d) {
      // console.log(d3.select(this));
      if (DrawingEngine.log_hover)
        console.log("%cc_node: ", "color: orange", d);
      DrawingEngine.highlight_cnode(d);

    })
    .on("mouseleave", function (d) {
      DrawingEngine.unhighlight_all();
    })
    .each(function(d) {d.svg_element = this;})
    .append("title").text(function (d) { return d.type; });

  c_nodes.exit().remove();
};

DrawingEngine.highlight_cnode = function(n){
  d3.select(n.svg_element).attr("style", function (d) {return "fill: gold";});

  for (var i in n.arr) {

    if (n.arr[i].svg_element && (n.arr[i].type === "svar"||
      (n.arr[i].type === "arr" && n.arr[i].isCollapsed) ||
      (n.arr[i].type === "array_element" && !n.arr[i].host.isCollapsed)))
      d3.select(n.arr[i].svg_element).attr("style", function (d) {return "fill: gold";});
    // else
    
      // console.log(n.arr[i]);
  }
  // console.log("new");
};

DrawingEngine.highlight_var = function(n){
  d3.select(n.svg_element).attr("style", function (d) {return "fill: gold";});

  for (var i in n.constraints){
    DrawingEngine.highlight_cnode(n.constraints[i].cnode);
  }
};

DrawingEngine.unhighlight_all = function(){
  d3.selectAll(".two_dim_array_e").attr("style", function (d) {return "fill: rgba(255, 255, 255, 1)";});
  d3.selectAll(".c_node").attr("style", function (d) {return "fill: rgba(255, 0, 0, 1)";});
  d3.selectAll(".a_node").attr("style", function (d) {return "fill: rgba(255, 255, 255, 1)";});
  d3.selectAll(".v_node").attr("style", function (d) {return "fill: rgba(255, 255, 255, 1)";});
};

DrawingEngine.prototype._draw_array_nodes = function() {
   if (DrawingEngine.log_svg_elements) console.group("A_NODES");
  a_nodes = this.nodesLayer.selectAll(".a_node")
    .data(shown_v.filter(function(v) {return (v.type === "arr" && v.isCollapsed === true);}), function (d) { return d.name; });

  var a_nodes_enter = a_nodes.enter();

  // console.log(shown_v.filter(function(v) {return (v.type === "arr" && v.isCollapsed === true);}));

  a_nodes.each(function(d) {
    d.svg_element = this;
  }); // because svg_elements becomes undefined for some reason

  a_nodes_enter.append("rect")
    .attr("class", "a_node")
    .attr("width", DrawingEngine.ARR_SIZE)
    .attr("height", DrawingEngine.ARR_SIZE)
    .each(function(d) {
      d.svg_element = this;
      if (DrawingEngine.log_svg_elements) console.log("%c+ a_node created:", "color: blue", d, "with svg: ", this);
      if (!this.aaa_id)
        this.aaa_id = ++DrawingEngine.counter;
    })
    .on("mouseover", function (d) {
      DrawingEngine.highlight_var(d);
    })
    .on("mouseleave", function (d) {
      DrawingEngine.unhighlight_all();
    })
    .attr("aaa_id", DrawingEngine.counter)
    .on("click", function (d) {expand_node(d);})
    .append("title").text(function (d) { return d.name; });

  a_nodes.exit().each(function (d){
    if (DrawingEngine.log_svg_elements) console.log("%c- a_node removed:", "color: brown", d, "with svg: ", this);
  })
  .remove();
   if (DrawingEngine.log_svg_elements) console.groupEnd();
};

DrawingEngine.prototype._draw_expanded_arrays = function() {
   if (DrawingEngine.log_svg_elements) console.group("EXPANDED_A_NODES");

  exp_a_nodes = d3.selectAll(".exp_a_node")
    .data(shown_v.filter(function(v) {return (v.type === "arr" && v.isCollapsed === false);}),
      function (v) { return v.name; }); /// key function to recognize created elements

  this.nodesLayer.selectAll(".exp_a_node").each(function(d) { d.svg_element = this; }); // because svg_elements becomes undefined for some reason

  exp_a_nodes.enter()
  .append("rect")
    .attr("class", "exp_a_node")
    .attr("width", function (d) {
      /// TODO: for other (higher than 2) dimentions as well
      /// TODO: do we need to recalculate width/height each time?
      if (d.dims === 2)
        return d.width = d.n[1] * (DrawingEngine.VAR_SIZE + DrawingEngine.PADDING) - DrawingEngine.PADDING;
      else if (d.dims === 1)
        return d.width = d.n[0] * (DrawingEngine.VAR_SIZE + DrawingEngine.PADDING) - DrawingEngine.PADDING;
    })
    .attr("height", function (d) {
      if (d.dims === 2)
        return d.height = d.n[0] * (DrawingEngine.VAR_SIZE + DrawingEngine.PADDING) - DrawingEngine.PADDING;
      else if (d.dims === 1)
        return d.height = DrawingEngine.VAR_SIZE;
    })
    .each(function(d) {
      if (DrawingEngine.log_svg_elements) console.log("%c+ expanded_a_node created:", "color: blue", d, "with svg: ", this);
      this.aaa_id = ++DrawingEngine.counter;
      d.svg_element = this;
    })
    .attr("aaa_id", DrawingEngine.counter)
    .append("title").text(function (d) { return d.name; });

  exp_a_nodes.exit().each(function (d){
    if (DrawingEngine.log_svg_elements) console.log("%c- exp_a_node removed:", "color: brown", d);
  }).remove();

  var temp_data = [];

  d3.selectAll(".exp_a_node").each(function(d) {
    var i, obj;
    if (d.dims === 2){
      console.log("array_vars: ", d.vars);
      for (i = 0; i < d.n[0]; i++){
        for (var j = 0; j < d.n[1]; j++){
          // obj = data.all_v[d.name + "[" + (i * d.n[1] + j + 1) + "]"];
          obj = d.vars[i * d.n[1] + j]; /// TODO: really?!
          obj.i = i; obj.j = j; obj.host = d;
          if (!obj.real_name)
            obj.real_name = d.name + "[" + (i + 1) + ", " + (j + 1) + "]";
          temp_data.push(obj);
          
        }
      }
    } else if (d.dims === 1) {
      for (i = 0; i < d.n[0]; i++){
        // obj = data.all_v[d.name + "[" + (i + 1) + "]"];
        obj = d.vars[i];
        obj.i = 0;
        obj.j = i;
        obj.host = d;
        if (!obj.real_name)
          obj.real_name = d.name + "[" + (i + 1) + "]";
        temp_data.push(obj);
      }
    }
  });

 /// ****************** all elements of 1/2-dim arrays ***************

  two_dim_array_e = this.nodesLayer.selectAll(".two_dim_array_e")
  .data(temp_data, function (d) { return d.name; });

  two_dim_array_e.enter().append('rect')
  .attr("class", "two_dim_array_e")
  .attr("width", DrawingEngine.VAR_SIZE)
  .attr("height", DrawingEngine.VAR_SIZE)
  .on("mouseover", function (d) {
      DrawingEngine.highlight_var(d);
    })
  .on("mouseleave", function (d) {
      DrawingEngine.unhighlight_all();
    })
  .each(function (d) {
    d.svg_element = this;
    d.type = "array_element";
  })
  .append("title").text(function (d) { return d.real_name; });

  two_dim_array_e.exit().remove();

  two_dim_array_e = this.nodesLayer.selectAll(".two_dim_array_e");

  /// **************************************************************

  /// ****************** buttons to collapse arrays *****************

  min_a_btns = this.nodesLayer.selectAll(".min_a_btn")
   .data(shown_v.filter(function(v) {return (v.type === "arr" && v.isCollapsed === false);}),
    function (v) { return v.name; });
  
  min_a_btns.enter()
   .append("rect")
   .attr("class", "min_a_btn")
   .attr("width", DrawingEngine.VAR_SIZE / 2)
   .attr("height", DrawingEngine.VAR_SIZE / 2)
   .attr("rx", "3px").attr("ry", "3px")
   .on("click", function (d) {collapse_node(d);});

  min_a_btns.exit().remove();

  min_a_btns = this.nodesLayer.selectAll(".min_a_btn");

   if (DrawingEngine.log_svg_elements) console.groupEnd();
  /// ***************************************************************

};

DrawingEngine.prototype._draw_links = function() {
  s_links = this.edgesLayer.selectAll(".straight_link").data(links);
  s_links.enter().append("line").attr("class", "straight_link");

  s_links.exit().remove();
};

DrawingEngine.prototype._update_drawing = function(){

  v_nodes.attr("cx", function (d) { return d.x; })
        .attr("cy", function (d) { return d.y; });
        
  a_nodes.attr("x", function (d) { return d.x - DrawingEngine.ARR_SIZE/2; })
        .attr("y", function (d) { return d.y - DrawingEngine.ARR_SIZE/2; });
        
  exp_a_nodes.attr("x", function (d) { return (d.x - d.width/2); })
            .attr("y", function (d) { return d.y - d.height/2; });

  min_a_btns.attr("x", function (d) { return d.x + d.width / 2 - 5; })
   .attr("y", function (d) { return d.y - d.height / 2 - 5; });
            
  two_dim_array_e
    .attr("x", function (d) { d.x = d.host.x + (DrawingEngine.VAR_SIZE + DrawingEngine.PADDING) * d.j - d.host.w / 2 + DrawingEngine.VAR_SIZE/2; return d.x - DrawingEngine.PADDING;})
    .attr("y", function (d) { d.y = d.host.y + (DrawingEngine.VAR_SIZE + DrawingEngine.PADDING) * d.i - d.host.h / 2 + DrawingEngine.VAR_SIZE/2; return d.y - DrawingEngine.PADDING;});
    
  c_nodes.attr("d", function (d) {
    var h = 16;
    return "M " + d.x + " " + (d.y - h/2) + " l " + (h/2) + " " + (h) + " l " + (-h) + " " + ("0") + " z";
  });
  
  s_links.attr("x1", function (d) { return d.source.x; })
    .attr("y1", function (d) { return d.source.y; })
    .attr("x2", function (d) { return d.real_target.x; })
    .attr("y2", function (d) { return d.real_target.y; });
};

/// auxiliary functions

function hasClass(elem, className) {
    return new RegExp(' ' + className + ' ').test(' ' + elem.className + ' ');
}

function addClass(elem, className) {
    if (!hasClass(elem, className)) {
        elem.className += ' ' + className;
    }
}

function removeClass(elem, className) {
    var newClass = ' ' + elem.className.replace( /[\t\r\n]/g, ' ') + ' ';
    if (hasClass(elem, className)) {
        while (newClass.indexOf(' ' + className + ' ') >= 0 ) {
            newClass = newClass.replace(' ' + className + ' ', ' ');
        }
        elem.className = newClass.replace(/^\s+|\s+$/g, '');
    }
}