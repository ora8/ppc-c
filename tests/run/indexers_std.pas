program indexers_std;
import ppc_std;
type TItem = class N: Integer; constructor Create(AN: Integer); destructor Destroy; override; end;
constructor TItem.Create(AN: Integer); begin N := AN; end;
destructor TItem.Destroy; begin WriteLn('free ', N); end;
begin
  vao l := TList<TItem>.Create;
  vao m := TMap<String, Integer>.Create;
  vao w := TList<String>.Create;
  begin
    l.Add(TItem.Create(1));
    l.Add(TItem.Create(2));
    if l[1] is some x then WriteLn('l[1] = ', x.N);
    l[0] := TItem.Create(10);
    if l[0] is some x then WriteLn('l[0] = ', x.N);
    m['a'] := 1;
    m['b'] := 2;
    m['a'] := 3;
    if m['b'] is some v then WriteLn('m[b] = ', v);
    WriteLn(m['zz'].HasValue, ' ', m.Count, ' ', m['a'].ValueOr(0));
    w.Add('x');
    if w[0] is some s then WriteLn(s);
  end;
end.
