{* EXPECT: 'l' was changed at line 9 while 'it' was borrowing it *}
program T;
import ppc_std;
type TItem = class N: Integer; end;
begin
  vao l := TList<TItem>.Create;
  begin
    l.Add(TItem.Create);
    for val it in l do begin l.Clear; WriteLn(it.N); end;
  end;
end.
