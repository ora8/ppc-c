{* EXPECT: identifier not found "__ReadFile" *}
program T;
import ppc_std;
begin
  WriteLn(__ReadFile('x'));
end.
