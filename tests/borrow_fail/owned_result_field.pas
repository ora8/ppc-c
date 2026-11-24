{ EXPECT: the result of 'Mach' owns objects and would never be freed here: store it in a variable first ('val r := Mach(...)') }
program T;
type TK = class end; TPaar = record L: optional TK; Info: Integer; end; function Mach: TPaar; begin Result.Info := 1; end;
begin WriteLn(Mach.Info); end.
