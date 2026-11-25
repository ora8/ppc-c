{ EXPECT: "M(Integer)" is overloaded twice with the same parameters }
program T;
type TA = class procedure M(i: Integer); overload; procedure M(i: Integer); overload; end;
begin end.
