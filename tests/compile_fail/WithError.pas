module WithError;
interface
function F: Integer;
implementation
function F: Integer;
begin
  Result := 'not an Integer';
end;
end.
