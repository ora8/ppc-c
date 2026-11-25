{ EXPECT: cannot modify a part of 'it', which is constant }
program T;
import ppc_std;
type TItem = class N: Integer; end;
begin
  vao l := TList<TItem>.Create;
  begin
    l.Add(TItem.Create);
    for val it in l do it.N := 3;
  end;
end.
