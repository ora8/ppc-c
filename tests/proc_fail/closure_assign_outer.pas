{ EXPECT: a closure variable can only be assigned in the routine that declares it }
program T;
procedure P(n: Integer); val f: reference to procedure := procedure begin WriteLn(n); end; procedure Q; begin f := procedure begin WriteLn(1); end; end; begin f; end;
begin end.
