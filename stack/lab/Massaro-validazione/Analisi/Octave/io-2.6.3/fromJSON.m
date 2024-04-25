## Copyright (C) 2019-2020 Ketan M. Patel
##
## This program is free software: you can redistribute it and/or modify it
## under the terms of the GNU General Public License as published by
## the Free Software Foundation, either version 3 of the License, or
## (at your option) any later version.
##
## This program is distributed in the hope that it will be useful, but
## WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
## GNU General Public License for more details.
##
## You should have received a copy of the GNU General Public License
## along with this program.  If not, see
## <https://www.gnu.org/licenses/>.

## -*- texinfo -*-
## @deftypefn {} {@var{obj} =} fromJSON (@var{str}, @var{sarray})
## Convert a JSON string into a native Octave object.
##
## Input arguments:
##
## @itemize
## @item
## @var{str} is a JSON input string.
## @end item
##
## @item
## Optional @var{sarray} sets how JSON structures with member arrays are to be
## parsed.  The default value is TRUE.
##
## @end item
## @end itemize
##
## Output:
##
## @var{obj} is a native Octave object.  Numbers, logicals, and structs are
## converted directly.  Any number string >= +/-1e308 is converted to +/-Inf,
## and 'null' is converted to NaN.  Quoted or unrecognizable JSON fragments
## are returned as Octave strings.
##
## Where possible, nested JSON array will be returned as appropraite matrix
## or ND array.  String arrays, mixed-class and mixed-size JSON arrays will
## return as Octave cell arrays.
##
## For JSON structure object with member arrays, if @var{sarray} is TRUE and
## if possible, an Octave struct array is returned; otherwise, a scalar struct
## with member arrays is returned.
##
## It is recommended @var{sarray} be set to FALSE for parsing mixed-class
## structures such as config JSON or HTTP payload but left as TRUE for
## structured, numerical data arrays.
##
## JSON structure object with members "re" and "im" (or "real" and "imag"),
## exclusively, will be converted to a commensurate complex number array,
## provided object members are compatible in size and shape.
##
## @seealso{toJSON}
## @end deftypefn

## Author: Ketan M. Patel <kmpatel@roc-photonics.com>
## Created: 2019-06-01
## Updates:
## 2020-10-22 Refactored and documented (Ketan M. Patel)
## 2020-10-27 Updated for Octave 7 compatibility (Ketan M. Patel)

function [obj] = fromJSON ( str='', SARRAY=[] );

  if ~( isnumeric(SARRAY) || isbool(SARRAY) );
    _warn_("invalid SARRAY");
    SARRAY = [];
  endif

  % set defaults
  ischar(str)      || (str='');
  ~isempty(SARRAY) || (SARRAY=true);

  try
    str = regexprep(str,'^[\s,]*','');
  catch
    error("Invalid UTF-8 encoding of JSON string");
  end_try_catch

  if( isempty(str) );
    obj = [];
  else
    wstate = warning('off',"Octave:num-to-str");
    obj = _fromjson_(str,SARRAY);
    warning(wstate);
  endif

endfunction

function [obj] = _fromjson_ ( str, SARRAY );

    obj = {};
    do;
      [obj{end+1} str] = _str2obj_(str,SARRAY);
    until( isempty(str) );

    if( numel(obj) == 1 );
      obj = obj{1};
    elseif( ~isstruct(obj{1}) || SARRAY );
      obj = _ndarray_(obj);
    endif

endfunction

function [obj,remain] = _str2obj_ ( str, SARRAY );

    [btype, block, remain] = _get_block_(str);
    remain = regexprep(remain,'^[\s,]*','');     % keep this HERE, not in _get_block_

    switch( btype );
      case '['; obj = _to_array_  (block,SARRAY);
      case '{'; obj = _to_struct_ (block,SARRAY);
      case '"'; obj = _to_string_ (block);

      otherwise;  eval(['obj=[' _rep_(block, Inf) '];'], ...
                       ['_warn_("invalid frag", (obj=block));']);
    endswitch

endfunction

function [btype, block, remain] = _get_block_ (str);

    switch( btype=str(1) );
      case '[';       idx = [strfind(str,'[') -strfind(str,']')];
      case '{';       idx = [strfind(str,'{') -strfind(str,'}')];
      case {'"',"'"}; idx = regexp(str(2:end),['(?<!\\)' btype], 'once');
                      idx = [1 -(idx+1)];
                      btype = '"';

      otherwise;  [block,remain] = strtok(str,',');
                  return;
    endswitch

    % exclude escaped-brackets (e.g.'\[', '\{', '\"', etc)
    tmp = abs(idx); tmp(2:end)--;
    idx(str(tmp) == '\') = [];

    if( numel(idx) == 1 );                  % unterminated block
      close = numel(str)+1;
    elseif( idx(2) < 0 );                   % non-nested block
      close = -idx(2);
    else                                    % nested block
      idx   = idx([~,i] = sort(abs(idx)));  % sort by abs(idx)
      count = cumsum(sign(idx));
      close = -idx(find(count==0, 1));
      close || (close = numel(str)+1);     % if necessary, autoclose at end of str
    end

    close <= numel(str) || _warn_("unclosed block", str(1:min(20,end)));

    block  = str(2 : close-1);
    remain = str(close+1 : end);

endfunction

%========================= str2... =======================

function [mat] = _to_array_ ( str, SARRAY );

    % look for >2D array or assignment (unsafe)
    ok = ~( regexp(str,'^\s*\[\s*\[') || strfind(str,'=') );  % <== faster done seperately

    if( ok );
      mat = regexprep(_rep_(str, Inf), ',(?=\s*\[)',';');

      if( regexp(mat,'^[\s[]*{' ) );    % first array element is structure
        mat = _rep_(mat, struct);
        eval([_lazyJSON_ "mat=_specials_([" mat "]); ok=true;"], "ok=false;");
      else
        eval(["mat=[" mat "]; ok=~(ischar(mat)||iscell(mat));"],"ok=false;");
      endif
    endif

    if( ~ok );              % this can be very slow, avoid getting here if possible
      mat = _fromjson_(str,SARRAY);
    elseif isstruct(mat);   % double-check struct array for special members
      mat = structfun(@_specials_,mat,'uniformoutput',false);
    endif

endfunction


function [obj] = _to_struct_ ( str, SARRAY );

    frag = str(1:min(end,20)); % keep frag for warning msg
    vals = keys = {};

    try; while( str );       % may fail for incomplete JSON struct string
      [key         str] = regexp(str,'^\s*"?(.*?)"?\s*:\s*(.*)$','tokens','once'){:};
      [vals{end+1} str] = _str2obj_(str,SARRAY);
      keys{end+1}       = eval(['"' key '"']);  % huh!?  clean up key of esc-codes
    endwhile; catch;
      _warn_("malformed struct frag", ['{' strtrim(frag) '...']);
    end_try_catch

    try;
      SARRAY || isempty(vals) || error("don't");
      obj = struct([keys; _num2cell_(vals)]{:});   % may fail for mixed-size vals
    catch;
      for i = 1:numel(vals);
        obj.(keys{i}) = vals{i};
      endfor;
    end_try_catch

    obj = _specials_(obj);

endfunction


function [obj] = _to_string_ ( str );
    if( regexp(str,'^\s*@') );
      try    obj = str2func(str);
      catch  _warn_("invalid inline function", str);
      end_try_catch;
    else
      obj = eval(['"' str '"']);  % huh!?  used to eval escaped chars (e.g. '\"')
    endif
endfunction

%========================= helpers =======================

function str = _rep_ ( str, arg );

  if isstruct(arg);
    arg = {'{','struct(',':',',','}',')'};
  elseif isinf(arg)
    arg = {'null','NaN ','e308','*Inf','Infinity','Inf     '};  % keep empty spaces
  endif

  do
    str = strrep(str,arg{1:2},'overlaps',false);
    arg(1:2) = [];
  until isempty(arg);

endfunction

function [c] = _num2cell_ ( c );
  for i=1:numel(c);
    if( ischar( _c_=c{i} ) || isempty(_c_) );
      c{i} = {_c_};
    elseif( ~iscell(_c_) );
      c{i} = num2cell(_c_);
    endif
  endfor
endfunction

function [c] = _ndarray_ ( c );  % try to make ND array (either mat or cell)

  sz = size(e = c{1}); t = class(e);

  if ~( ~ischar(e) && sz && cellfun(@(v)isa(v,t)&&size(v)==sz, c) );
    return;
  elseif( isscalar(e) );     % try matrix conversion, ok to fail if not uniform class
    try c = [c{:}]; end;
  elseif( isvector(e) );     % cell array of mat or cell array
    c = reshape([c{:}], numel(e), numel(c)).';
  else                    % make ND array
    index.type = "()";
    index.subs = repmat({':'},1,ndims(e)+1);
    i = numel(c);

    do
      index.subs{end} = i;
      e = subsasgn(e,index,c{i});
    until (--i) == 1;
    c = e;

  endif

endfunction

function [s] = _specials_ ( s );   % convert special structures

  if( ~isstruct(s) ); return; endif;

  keys = __fieldnames__(s);

  if( ismember(keys,{'real','imag'}) ); _to_complex_([s.real],[s.imag]);
  elseif( ismember(keys,{'re','im'}) ); _to_complex_([s.re  ],[s.im  ]);
  endif

  function _to_complex_(re,im);
    if( isscalar(s) && size_equal(re,im) );
      s = complex(re,im);
    elseif( prod( sz=size(s) ) == numel(re) );
      s = reshape(complex(re,im),sz);
    endif
  endfunction

endfunction

% for max conversion speeed, gaurd against lazy JSON (unquoted keys) of common structures
function [str] = _lazyJSON_ ( );
  str = 'x="x";y="y";z="z";real=re="re";imag=im="im";r="r";rho="rho";theta="theta";';
endfunction


function out = _warn_ ( msg, frag='' );
  out = warning(["fromJSON.m: " msg ifelse(isempty(frag),'',[': "' frag '"']) "\n"]);
endfunction


%!test  ## input validation
%! assert(fromJSON(),[]); % ok, reference
%!warning <invalid SARRAY> fromJSON([],struct);
%!
%! bad = {4,{},@sin,struct(),'',false,0,[1,2,3]};
%! assert(all(cellfun(@(i)isempty(fromJSON(i)),bad)));  % all bad, return []

%!test  ## number
%! assert(fromJSON('4'),4)

%!test  ## number string
%! assert(fromJSON('"string"'),"string")

%!test  ## bool
%! assert(fromJSON('true'),true)

%!test  ## numerical array
%! assert(fromJSON('[1,2,3,4]'),1:4)
%! assert(fromJSON('[[1,2],[3,4]]'),[1 2;3 4])

%!test  ## infinity
%! assert(fromJSON('1e308'),   Inf)
%! assert(fromJSON('-1e308'), -Inf)
%! assert(fromJSON('1e999'),   Inf)
%! assert(fromJSON('-1e999'), -Inf)
%! assert(fromJSON('Inf'),     Inf)
%! assert(fromJSON('-Inf'),   -Inf)
%! assert(fromJSON('Infinity'),   Inf)
%! assert(fromJSON('-Infinity'), -Inf)
%! assert(fromJSON('[1,1e308,Inf,-Infinity]'),[1 Inf Inf -Inf])

%!test  ## null
%! assert(fromJSON( 'null' ),NaN)
%! assert(fromJSON('"null"'),"null")

%!test  ## empty array
%! assert(fromJSON('[]'),[]);

%!test  ## numerical matrix
%! assert(fromJSON('[[1,2],[3,4]]'),[1 2;3 4])

%!test  ## bool matrix
%! assert(fromJSON('[[true,false];[false,true]]'),!![1 0;0 1])

%!test  ## numeric/bool matrix
%! assert(fromJSON('[[true,3];[false,true]]'),[1 3;0 1])

%!test  ## numerical ND array
%! assert(fromJSON('[[[1,3,5],[2,4,6]],[[7,9,11],[8,10,12]]]'),reshape(1:12,2,3,2));

%!test  ## more N numerical ND array
%! json = "[[[[[1,3],[2,4]],[[5,7],[6,8]]],[[[11,13],[12,14]],[[15,17],[16,18]]]],[[[[21,23],[22,24]],[[25,27],[26,28]]],[[[31,33],[32,34]],[[35,37],[36,38]]]]]";
%! assert(fromJSON(json),reshape([1:8 11:18 21:28 31:38],2,2,2,2,2));

%!test  ## mixed-class array (to cell array)
%! assert(fromJSON('[["a",2,3],[4,"b",5]]'), {'a' 2 3; 4 'b' 5});

%!test  ## mismatch nested array
%! assert(fromJSON('[[1,2,3,4,5],[1,2]]'),{[1 2 3 4 5] [1 2]})

%!test  ## more mismatched nested array
%! assert(fromJSON('[1,2,3,[2,3]]'),{1,2,3,[2,3]})

%!test  ## array of numerical array and mixed-class array
%! assert(fromJSON('[[1,2,3,"a"],[2,3,4,4]]'),{{1 2 3 "a"},[2 3 4 4]})

%!test  ## more N numerical ND array
%! json = "[[[[[1,3],[2,4]],[[5,7],[6,8]]],[[[11,13],[12,14]],[[15,17],[16,18]]]],[[[[21,23],[22,24]],[[25,27],[26,28]]],[[[31,33],[32,34]],[[35,37],[36,38]]]]]";
%! json = regexprep(json,'(\d+)','"$1"'); % turn it input JSON array of strings
%! c    = cellfun(@num2str,num2cell(reshape([1:8 11:18 21:28 31:38],2,2,2,2,2)), 'uniformoutput', false);
%! assert(fromJSON(json),c);

%!test  ## JSON-like: with non-JSON, Octave, numerical notation (bonus feature)
%! assert(fromJSON('[Inf,-Inf,NaN,2i,pi,e]'),[Inf,-Inf,NaN,2*i,pi,e],1e-15);

%!test   ## beautified JSON
%! obj=fromJSON("\n[\n\t  [1,2,3,4],\n\t  [2,3,4,4]\n]    ");
%! assert(obj,[[1 2 3 4];[2 3 4 4]])

%!test   ##  incomplete array
%! warning('off','all');
%! assert(fromJSON("[1,2,3  "),[1,2,3]);

%!test   ##  more incomplete array
%! warning('off','all');
%! assert(fromJSON("[[1,2,3],[3"),{[1,2,3],[3]});

%!test  ## string with whitespaces
%! assert(fromJSON('"te\nss      df\t t"'),"te\nss      df\t t")

%!test  ## char array
%! assert(fromJSON('["a","b","c"]'),{'a','b','c'})

%!test  ## array of string
%! assert(fromJSON('["test","list","more"]'),{'test',"list","more"})

%!test  ## escaped quote
%! assert(fromJSON('"tes\"t"'),'tes"t');
%! assert(fromJSON('["te\"t","list","more"]'),{'te"t' 'list' 'more'})

%!test  ## struct
%! assert(fromJSON('{}',true), struct())
%! assert(fromJSON('{}',false),struct())

%!test  ## struct
%! assert(fromJSON('{"a":3,"b":5}',true),struct("a",3,"b",5))

%!test  ## lazy JSON struct
%! assert(fromJSON('{a:3,b:5}',true), struct("a",3,"b",5))

%!test   ## beautified JSON
%! assert(fromJSON("{\n\ta\t  :\t  3\n\t}"), struct("a",3))

%!test   ## lazy key with spaces
%! obj=fromJSON('{key with space: 4,"a":3}');
%! assert(obj,struct("a",3,"key with space",4))

%!test   ## duplicate key
%! assert(fromJSON('{a:3,"a":5}'),struct("a",5))

%!test   ## empty object key-val
%! obj=fromJSON('{a:3,,,    ,     "b"   :5}');
%! assert(obj,struct("a",3,"b",5))

%!test ## struct of vector
%! assert(fromJSON('{a:[1,2,3,4]}',            true), struct('a',{1 2 3 4}));
%! assert(fromJSON('[{a:1},{a:2},{a:3},{a:4}]',true), struct('a',{1 2 3 4}));
%! assert(fromJSON('{a:[1,2,3,4]}',           false), struct('a',[1 2 3 4]));

%!test ## struct of 2x2 array
%! assert(fromJSON('{a:[[1,3],[2,4]]}',          true), struct('a',{1 3;2 4}));
%! assert(fromJSON('[{a:1},{a:2}],[{a:3},{a:4}]',true), struct('a',{1 2;3 4}));
%! assert(fromJSON('{a:[[1,3],[2,4]]}',         false), struct('a',[1 3;2 4]));

%!test ## array of struct with SARRAY=false
%! assert(fromJSON('[{a:1},{a:2} , {a:3},{a:4}]',false), num2cell(struct('a',{1 2 3 4})));
%! assert(fromJSON('[{a:1},{a:2}],[{a:3},{a:4}]',false), num2cell(struct('a',{1 2;3 4})));

%!test  ## struct with mixed-size arrays (will not honor SARRAY=true)
%! assert(fromJSON('{"a":[1,2],"b":[3,4,5]}',true), struct("a",[1,2],"b",[3,4,5]))

%!test  ## struct with number and string (guard against turing string into char array)
%! assert(fromJSON('{"a":1,"b":"hello"}',true), struct("a",1,"b","hello"))

%!test  ## struct with empty array  (gaurd against returning empty struct array)
%! assert(fromJSON('{"a":3,"b":[]}',true), struct("a",3,"b",[]))

%!test  ## incomplete struct
%! warning('off','all');
%! assert(fromJSON('[[1,2,{a:3,b ],3,4,5]'), {{1,2,struct('a',3)},3,4,5});

%!test  ## **nested** struct with array
%! assert(fromJSON('{"a":{"b":[1,2,3]}}',true ), struct("a",num2cell(struct("b",{1,2,3}))))  % <== struct array
%! assert(fromJSON('{"a":{"b":[1,2,3]}}',false), struct("a",struct("b",[1,2,3])))            % <== struct with array b

%!test  ## struct with mixed class array
%! assert(fromJSON('{"b":[1,2,{c:4}]}',true),struct('b',{1,2,struct('c',4)}));
%! s.b = {1,2,struct("c",4)};
%! assert(fromJSON('{"b":[1,2,{c:4}]}',false),s)

%!test ## 2x2 array of struct
%! obj=fromJSON('[[{a:1},{a:3}],[{a:2},{a:4}]]');
%! assert(obj,struct('a',{1 3;2 4}));

%!test ## 2x2 array of struct of arrays NOTE: this is sketchy operation
%! obj=fromJSON('[[{"a":[1,1]},{"a":[3,3]}],[{"a":[2,2]},{"a":[4,4]}]]');
%! assert(obj,struct('a',{[1 1] [3 3];[2 2] [4 4]}));

%!test ## struct of ND array
%! obj=fromJSON('{a:[[[1,3],[2,4]],[[11,13],[12,14]]]}');
%! assert(obj,struct('a',num2cell(reshape([1:4 11:14],2,2,2))));

%!test ## ND array of struct
%! obj=fromJSON('[[[{a:1},{a:3}],[{a:2},{a:4}]],[[{a:11},{a:13}],[{a:12},{a:14}]]]');
%! assert(obj,struct('a',num2cell(reshape([1:4 11:14],2,2,2))));

%!test  ## mixed array with struct
%! assert(fromJSON('[2,{a:3,"b":5}]'),{2,struct("a",3,"b",5)})

%!test ## more mixed array with struct
%! assert(fromJSON('[{a:3,"b":5},{a:3}]'),{struct("a",3,"b",5),struct("a",3)})

%!test  ## complex number struct
%! assert(fromJSON('{re:3,im:5}'),    3+5i);
%! assert(fromJSON('[{re:3,im:5}]'),  3+5i);
%! assert(fromJSON('{real:4,imag:6}'),4+6i);

%!test  ## complex number struct with anomoly
%! assert(fromJSON('{im:3,re:5}'),           5+3i);
%! assert(fromJSON('{im:3,re:5,re:7,im:10}'),7+10i);

%!test  ## complex number struct array
%! assert(fromJSON('{re:[4,5],im:[6,7]}'),          [4+6i,5+7i]     );
%! assert(fromJSON('[{re:[4,5],im:[6,7]}]'),        [4+6i,5+7i]     );
%! assert(fromJSON('[{re:[4,5;0 2],im:[6,7;3 0]}]'),[4+6i,5+7i;3i 2]);

%!test  ## complex number struct in array
%! obj=fromJSON('[[{re:4,im:6},{re:1,im:0}],[{re:0,im:2}, {re:2,im:4}]]');
%! assert(obj,[4+6i 1;2i 2+4i]);

%!test  ## complex number struct mixed with number within array
%! obj=fromJSON('[[4,{re:1,im:0}],[{re:0,im:2}, {re:4,im:6}]]');
%! assert(obj,[4 1;2i 4+6i]);

%!test  ## complex number struct within a structure (nested)
%! assert(fromJSON('{"a": {re:1,im:5}}'),struct('a',1+5i));

%!test  ## complex number struct with structure array (nested)
%! obj = fromJSON('[{"a":{re:1,im:5}},{"a":{re:2,im:5}}]');
%! assert(obj,struct('a',{1+5i,2+5i}));

%!test  ## test apparent octave inline fn  (convert to inline)
%! assert(fromJSON('"@sin"'),@sin);
%! assert(func2str(fromJSON('"@(x)3*x"')),func2str(@(x)3*x));

%!test   ## exotic object in structure
%! assert(fromJSON('{"a":"[java.math.BigDecimal]"}'),struct('a','[java.math.BigDecimal]'));

%!test  ## JSON with confusing '[]{},' AND missing quotes (hard string parse test)
%! warning('off','all');
%! obj=fromJSON('[{a:"tes, {}: [ ] t"},"lkj{} sdf",im mi{}ing quotes]');
%! assert(obj,{struct('a',"tes, {}: [ ] t"), 'lkj{} sdf','im mi{}ing quotes'})

%!test  ## garbage (non-quoted, meaningless string)
%! warning('off','all');
%! assert(fromJSON('garbage'),'garbage')

%!test  ## garbage in array
%! warning('off','all');
%! assert(fromJSON('[1,garbage]'),{1,'garbage'})

%!test  ## garbage in struct
%! warning('off','all');
%! assert(fromJSON('{a:garbage}'),struct('a','garbage'))

%!test   ## exotic object (placeholder of class name)
%! assert(fromJSON('"[java.math.BigDecimal]"'),'[java.math.BigDecimal]');

%!test   ##  warnings
%!warning <invalid frag>   fromJSON('garbage');
%!warning <malformed>      fromJSON('{a:3,b}');
%!warning <unclosed>       fromJSON('{a:3,b:4');
%!warning <invalid frag>   fromJSON('@nofunc'); ## looks like fn, but is UNQUOTED string

%!test  %% jsondecode's Arrays with the same field names in the same order.
%! json = ['[', ...
%!   '{', ...
%!     '"x_id": "5ee28980fc9ab3",', ...
%!     '"index": 0,', ...
%!     '"guid": "b229d1de-f94a",', ...
%!     '"latitude": -17.124067,', ...
%!     '"longitude": -61.161831,', ...
%!     '"friends": [', ...
%!       '{', ...
%!         '"id": 0,', ...
%!         '"name": "Collins"', ...
%!       '},', ...
%!       '{', ...
%!         '"id": 1,', ...
%!         '"name": "Hays"', ...
%!       '},', ...
%!       '{', ...
%!         '"id": 2,', ...
%!         '"name": "Griffin"', ...
%!       '}', ...
%!     ']', ...
%!   '},', ...
%!   '{', ...
%!     '"x_id": "5ee28980dd7250",', ...
%!     '"index": 1,', ...
%!     '"guid": "39cee338-01fb",', ...
%!     '"latitude": 13.205994,', ...
%!     '"longitude": -37.276231,', ...
%!     '"friends": [', ...
%!       '{', ...
%!         '"id": 0,', ...
%!         '"name": "Osborn"', ...
%!       '},', ...
%!       '{', ...
%!         '"id": 1,', ...
%!         '"name": "Mcdowell"', ...
%!       '},', ...
%!       '{', ...
%!         '"id": 2,', ...
%!         '"name": "Jewel"', ...
%!       '}', ...
%!     ']', ...
%!   '},', ...
%!   '{', ...
%!     '"x_id": "5ee289802422ac",', ...
%!     '"index": 2,', ...
%!     '"guid": "3db8d55a-663e",', ...
%!     '"latitude": -35.453456,', ...
%!     '"longitude": 14.080287,', ...
%!     '"friends": [', ...
%!       '{', ...
%!         '"id": 0,', ...
%!         '"name": "Socorro"', ...
%!       '},', ...
%!       '{', ...
%!         '"id": 1,', ...
%!         '"name": "Darla"', ...
%!       '},', ...
%!       '{', ...
%!         '"id": 2,', ...
%!         '"name": "Leanne"', ...
%!       '}', ...
%!     ']', ...
%!   '}', ...
%! ']'];
%! var1 = struct ('id', {0 1 2}, 'name', {'Collins' 'Hays' 'Griffin'});
%! var2 = struct ('id', {0 1 2}, 'name', {'Osborn' 'Mcdowell' 'Jewel'});
%! var3 = struct ('id', {0 1 2}, 'name', {'Socorro' 'Darla' 'Leanne'});
%! exp  = struct (...
%!   'x_id', {'5ee28980fc9ab3' '5ee28980dd7250' '5ee289802422ac'}, ...
%!   'index', {0 1 2}, ...
%!   'guid', {'b229d1de-f94a' '39cee338-01fb' '3db8d55a-663e'}, ...
%!   'latitude', {-17.124067 13.205994 -35.453456}, ...
%!   'longitude', {-61.161831 -37.276231 14.080287}, ...
%!   'friends', {var1 var2 var3});
%! assert (fromJSON(json), exp);

