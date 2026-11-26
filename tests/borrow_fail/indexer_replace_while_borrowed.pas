{* EXPECT: 'l' was used at line 4 while 'x' held a changeable borrow *}
program T;
import std; type TItem = class N: Integer; end;
begin vao l := TList<TItem>.Create; begin l.Add(TItem.Create); if l[0] is some x then begin l[0] := TItem.Create; WriteLn(x.N); end; end; end.
