{* EXPECT: the result of 'Make' owns objects and would never be freed here: store it in a variable first ('val r := Make(...)') *}
program T;
type TK = class end; TPair = record L: optional TK; Info: Integer; end; function Make: TPair; begin Result.Info := 1; end;
begin WriteLn(Make.Info); end.
