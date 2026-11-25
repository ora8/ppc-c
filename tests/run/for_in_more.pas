program for_in_more;
import ppc_std;
type TItem = class N: Integer; end;
function Total<T: Numeric>(a: array of T): T;
begin
  Result := 0;
  for val x in a do Result := Result + x;
end;
val
  s := 'abc';
  d: array of Integer := (1, 2, 3);
begin
  for var c in s do c := UpCase(c);
  WriteLn(s);
  WriteLn(Total(d));
  vao l := TList<TItem>.Create;
  begin
    l.Add(TItem.Create); l.Add(TItem.Create);
    for var it in l do it.N := 4;
    for val it in l do Write(it.N, ' ');
    WriteLn;
  end;
  for val i in d do
    for val j in d do
      if i < j then Write(i, j, ' ');
  WriteLn;
end.
