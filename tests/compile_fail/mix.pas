{ EXPECT: cannot mix 'object' and 'class' in inheritance }
program T;
type A = object end; B = class(A) end;
begin end.
