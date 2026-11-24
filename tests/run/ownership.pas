program Ownership;
type
  TItem = class
    Value: Integer;
    Next: optional TItem;         { owns the next element (none at the end) }
    constructor Create(V: Integer);
    destructor Destroy; override;
  end;
  TList = class
    Head: optional TItem;
    Count: Integer;
    procedure Push(vao Item: TItem);   { takes over the ownership }
    function Find(V: Integer): var optional TItem;
    function PopFront: optional TItem;    { gives the ownership to the caller }
    destructor Destroy; override;
  end;

constructor TItem.Create(V: Integer);
begin inherited Create; Value := V; end;

destructor TItem.Destroy;
begin
  inherited Destroy;             { Next (and with it the whole chain) is freed automatically }
end;

procedure TList.Push(vao Item: TItem);
begin
  Item.Next := Take(Head);       { Head -> none, ownership moves to Item.Next }
  Head := Item;                  { move of the parameter into the field }
  Inc(Count);
end;

function TList.Find(V: Integer): var optional TItem;
var cur: optional TItem := none;
begin
  Result := none;
  cur := Head;
  while cur is some c do begin
    if c.Value = V then begin Result := c; Exit; end;
    cur := c.Next;
  end;
end;

function TList.PopFront: optional TItem;
begin
  Result := Take(Head);
  if Result is some r then begin
    Head := Take(r.Next);
    Dec(Count);
  end;
end;

destructor TList.Destroy;
begin
  inherited Destroy;
end;

procedure PrintList(L: TList);   { value parameter = borrow }
var it: optional TItem := none;
begin
  it := L.Head;
  while it is some x do begin Write(x.Value, ' '); it := x.Next; end;
  WriteLn('(', L.Count, ' Items)');
end;

type
  TVec = class                  
    X, Y: Real;
    constructor Create(AX, AY: Real);
    function Len: Real;
  end;
constructor TVec.Create(AX, AY: Real); begin inherited Create; X := AX; Y := AY; end;
function TVec.Len: Real; begin Result := Sqrt(X * X + Y * Y); end;

procedure Scale(V: TVec; F: Real);   { a borrow is enough to change the fields }
begin V.X := V.X * F; V.Y := V.Y * F; end;

vao
  list: TList := TList.Create;                   { owners }
  item: TItem := TItem.Create(99);
  popped: optional TItem := none;
var
  found: optional TItem := none;
val
  i: Integer := default;
vao
  v := TVec.Create(3, 4);
var
  pv: TVec := v;
begin
  
  for i := 1 to 5 do list.Push(TItem.Create(i * 10));
  
  list.Push(item);               { item is moved afterwards }
  item := TItem.Create(7);       { assigning again is allowed }
  list.Push(item);
  PrintList(list);
  found := list.Find(30);
  if found is some f then WriteLn('found: ', f.Value);
  popped := list.PopFront;
  if popped is some q then WriteLn('taken: ', q.Value);
  PrintList(list);

  
                         { borrow of v }
  Scale(pv, 2);
  WriteLn('Len: ', pv.Len:0:1);
end.
