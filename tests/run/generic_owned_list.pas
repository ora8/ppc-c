program generic_owned_list;
import std;
type
  TItem = class
    N: Integer;
    constructor Create(AN: Integer);
    destructor Destroy; override;
  end;
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

begin
  vao l := TList<TOrder>.Create;
  begin
    l.Add(MakeOrder('a', 1));
    l.Add(MakeOrder('b', 2));
    val o := MakeOrder('c', 3);
    begin
      l.Add(o);
    end;
    WriteLn(l.Count);
    if l.Get(1) is some x then WriteLn(x.Name);
    if l.Extract(0) is some y then
      if y.Item is some it then WriteLn('extracted ', y.Name, ' ', it.N);
    WriteLn(l.Count);
  end;
  WriteLn('end');
end.
