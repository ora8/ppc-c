{* EXPECT: a case expression cannot give objects or values that own them ("TK"): use a case statement *}
program T;
type TK = class end;
vao a := TK.Create;
vao b := TK.Create;
val i: Integer := 1;
begin
  var r: TK := case i of 1: a else b end;
  begin end;
end.
