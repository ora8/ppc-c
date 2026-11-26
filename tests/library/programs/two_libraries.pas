program TwoLibraries;
{* uses two Perfect Pascal libraries at once (built as shared objects or as static libraries) *}
type
  TShape = class
    Side: Integer;
  end;

function Add(a, b: Integer): Integer; external;
function GreetCount(name: String): Integer; external;

vao s := TShape.Create;      {* a class of the program itself: its method table is a third one *}
begin
  s.Side := 4;
  WriteLn(Add(s.Side, 3));
  WriteLn(GreetCount('Anna'));
  WriteLn(GreetCount('Bert'));
end.
