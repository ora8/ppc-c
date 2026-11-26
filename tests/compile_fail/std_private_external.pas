{* EXPECT: identifier not found "__ReadFile" *}
program T;
import std;
begin
  WriteLn(__ReadFile('x'));
end.
