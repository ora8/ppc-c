{* EXPECT: UAt: the string has fewer characters *}
program T;
import std;
begin
  WriteLn(UAt('äb', 3));
end.
