program sum_types_owned;
type
  TItem = class
    N: Integer;
    constructor Create(AN: Integer);
    destructor Destroy; override;
  end;
  TMaybe<T> = (Just(Value: T), Nothing);
  TTree = (Leaf, Node(Item: optional TItem; Label: String));
constructor TItem.Create(AN: Integer); begin N := AN; end;
destructor TItem.Destroy; begin WriteLn('free ', N); end;
function Half(n: Integer): TMaybe<Integer>;
begin
  if Odd(n) then Result := TMaybe<Integer>.Nothing else Result := TMaybe<Integer>.Just(n div 2);
end;
val
  m := Half(10);
  t := Node(TItem.Create(7), 'seven');
  u: TTree := Leaf;
begin
  if m is Just(v) then WriteLn('half ', v);
  case Half(3) of
    Just(v): WriteLn(v);
    Nothing: WriteLn('odd');
  end;
  if t is Node(it, l) then
    if it is some x then WriteLn(l, ' ', x.N);
  u := t;
  WriteLn(u is Leaf, ' ', t is Leaf);
  t := Node(TItem.Create(8), 'eight');
  t := Leaf;
  WriteLn('end');
end.
