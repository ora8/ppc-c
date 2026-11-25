program closures_std;
import ppc_std;
procedure Demo(Limit: Integer; Desc: Boolean);
val
  a: array of Integer := (5, 1, 9, 3, 7);
  Calls := 0;
begin
  SortBy(a, function(x, y: Integer): Boolean begin Inc(Calls); if Desc then Result := x > y else Result := x < y; end);
  for val x in a do Write(x, ' ');
  WriteLn('(', Calls > 0, ')');
  val big := Filter(a, function(x: Integer): Boolean begin Result := x >= Limit; end);
  begin
    WriteLn(Length(big), ' >= ', Limit);
  end;
end;
begin
  Demo(5, True);
  Demo(4, False);
end.
