{* EXPECT: a case expression that gives an object needs a new object in every arm *}
program T;
type TK = class end;
vao a := TK.Create;
vao b := TK.Create;
val i: Integer := 1;
begin
  var r: TK := case i of 1: a else b end;
  begin end;
end.
