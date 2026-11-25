{ EXPECT: cannot store borrowed reference 'x' in owning location 'Result.First' }
program T;
type TPair<A, B> = record First: optional A; Second: optional B; end; function MakePair<A, B>(x: A; y: B): TPair<A, B>; begin Result.First := x; Result.Second := y; end;
begin end.
