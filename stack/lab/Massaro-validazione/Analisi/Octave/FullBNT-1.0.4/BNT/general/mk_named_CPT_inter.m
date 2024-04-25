function CPT2 = mk_named_CPT_inter(family_names, names, dag, CPT1, idx_2)
% MK_NAMED_CPT Permute the dimensions of a CPT so they agree with the internal numbering convention
% CPT2 = mk_named_CPT_inter(family_names, names, dag, CPT1, idx_2)
%
% Modified by L. Portinale 23/4/20
% to take into account dynamic CPT: added idx_2
% idx_2: array of indices in family_names representing slice 2 nodes
% (the child is not included since it is at slice 2 by definition of inter
% arc)
%
% Example:  A-->A#; A-->B#; A#-->B#; B-->B# (where X slice 1 node; X# slice
% 2 node)
% suppose we want to get the right CPT for node B# (node B@slice2)
%cpt2=mk_named_CPT_inter({'A', 'B', 'A', 'B'},names, dag, cpt1,[3];  this
%means that the third in the family list is a node of slice 2 (actually
%A#); by definition the last one is on slice 2 (B#)

n = length(family_names);
family_nums = zeros(1,n);
for i=1:n
  family_nums(i) = strmatch(family_names{i}, names); % was strmatch
end

%LP modified
for i=idx_2
    family_nums(i)=family_nums(i)+length(names); %every node in family_names{i} is at slice 2
end;
family_nums(end)=family_nums(end)+length(names); % child node is at slice 2
%

fam = family(dag, family_nums(end)); 
perm = zeros(1,n);
for i=1:n
  %  perm(i) = find(family_nums(i) == fam);
  perm(i) = find(fam(i) == family_nums);
end

CPT2 = permute(CPT1, perm);
