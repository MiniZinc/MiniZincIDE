function Tools(){}

/// to recursively parse strings with arrays 
Tools.parse_array_str = function(str){
  i = 0;
  start = 0;
  open_count = 0;
  closed_count = 0;
  var arr = [];
  for (var i = 0; i < str.length; i++){
    var has_inner_arrays = false;
    if (str[i] === "["){
      open_count++;
      if (open_count === 1)
        start = i;
      if (open_count > 1)
        has_inner_arrays = true;
      }
      else if (str[i] === "]")
        closed_count++;
      if ((closed_count !== 0) && (open_count === closed_count)){
          
        if (has_inner_arrays)
          arr.push(parse_array_str(str.substring(start + 1, i)));
        else {
          // arr.push(str.substring(start + 1, i).split(','));
          arr.push(str.substring(start + 1, i));
        }
                
        open_count = 0;
        closed_count = 0;
      }
  }
  return arr;
};

Tools.removeBraces = function(str){
    return str.replace(/[\]\[]{1,}/gi, "");
};
