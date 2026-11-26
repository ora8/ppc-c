program generic_owned_values;
import ppc_std;
type
  TItem = class
    N: Integer;
    constructor Create(AN: Integer);
    destructor Destroy; override;
  end;
  TWords = array of String;
  TOrder = record
    Name: String;
    Item: optional TItem;
  end;
constructor TItem.Create(AN: Integer); begin N := AN; end;
destructor TItem.Destroy; begin WriteLn('free item ', N); end;

function MakeOrder(Name: String; n: Integer): TOrder;
begin
  Result.Name := Name;
  Result.Item := TItem.Create(n);
end;

procedure Show(o: TOrder);
begin
  Write(o.Name);
  if o.Item is some it then Write('#', it.N);
  WriteLn;
end;

procedure Consume<T>(vao x: T);
begin
  WriteLn('consumed');
end;

function Wrap<T>(vao x: T): optional T;
begin
  Result := x;
end;

procedure Keep(vao o: TOrder);
begin
  Show(o);
end;

val
  o := MakeOrder('own', 7);
  words: array of String := ('x', 'y');
begin
  vao st := TStack<TOrder>.Create;
  vao m := TMap<String, TOrder>.Create;
  vao lists := TList<TWords>.Create;
  begin
    st.Push(MakeOrder('s1', 1));
    st.Push(MakeOrder('s2', 2));
    if st.Peek is some p then Show(p);
    if st.Pop is some q then Show(q);
    WriteLn(st.Count);
    m.Put('k1', MakeOrder('m1', 11));
    m.Put('k2', MakeOrder('m2', 12));
    m.Put('k1', MakeOrder('m1b', 13));          {* replaces: frees item 11 *}
    if m.Get('k1') is some g then Show(g);
    if m.Get('zz') is some z then Show(z) else WriteLn('no zz');
    if m.Remove('k2') is some r then Show(r);
    WriteLn(m.Count, ' ', m.Contains('k2'));
    Consume(MakeOrder('c', 21));
    if Wrap(MakeOrder('w', 22)) is some w then Show(w);
    Keep(o);
    WriteLn('[', o.Name, ']');                  {* moved: empty *}
    lists.Add(words);
    lists.Add(Split('a,b,c', ','));
    if lists.Get(1) is some ws then WriteLn(Length(ws), ' ', Join(ws, '+'));
    WriteLn(Length(words));
  end;
  WriteLn('end');
end.
