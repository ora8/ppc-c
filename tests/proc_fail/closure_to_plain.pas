{* EXPECT: this lambda uses variables of the routine around it (a closure) and cannot be stored as "procedure(Integer)" *}
program T;
type TH = procedure(x: Integer); procedure Use(h: TH); begin end; procedure P(n: Integer); begin Use(procedure(x: Integer) begin WriteLn(x + n); end); end;
begin end.
