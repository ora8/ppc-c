{ EXPECT: duplicate method "M" (to give several methods the same name, mark each of them with 'overload') }
program T;
type TA = class procedure M(i: Integer); procedure M(s: String); end;
begin end.
