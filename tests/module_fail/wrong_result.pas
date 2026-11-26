{* EXPECT: incompatible types in assignment: got "Char" expected "Integer" *}
module wrong_result;
interface
function F: Integer;
implementation
function F: Integer; begin Result := 'x'; end;
end.
