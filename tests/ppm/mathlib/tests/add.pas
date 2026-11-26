program AddTest;
{* the tests of a library are linked with it: its routines are 'external' *}
function Add(a, b: Integer): Integer; external;
begin
  WriteLn(Add(40, 2));
end.
