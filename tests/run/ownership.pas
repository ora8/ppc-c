program Ownership;
type
  TItem = class
    Value: Integer;
    Next: optional TItem;         { besitzt das naechste Element (none am Ende) }
    constructor Create(V: Integer);
    destructor Destroy; override;
  end;
  TList = class
    Head: optional TItem;
    Count: Integer;
    procedure Push(owned Item: TItem);   { uebernimmt den Besitz }
    function Find(V: Integer): borrowed optional TItem;
    function PopFront: optional TItem;    { gibt Besitz an Aufrufer }
    destructor Destroy; override;
  end;

constructor TItem.Create(V: Integer);
begin inherited Create; Value := V; end;

destructor TItem.Destroy;
begin
  Next.Free;                     { Kette rekursiv freigeben }
  inherited Destroy;
end;

procedure TList.Push(owned Item: TItem);
begin
  Item.Next := Take(Head);       { Head -> none, Besitz wandert zu Item.Next }
  Head := Item;                  { Move des Parameters ins Feld }
  Inc(Count);
end;

function TList.Find(V: Integer): borrowed optional TItem;
val cur: borrowed optional TItem;
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
  Head.Free;
  inherited Destroy;
end;

procedure PrintList(L: TList);   { Wertparameter = Ausleihe }
val it: borrowed optional TItem;
begin
  it := L.Head;
  while it is some x do begin Write(x.Value, ' '); it := x.Next; end;
  WriteLn('(', L.Count, ' Elemente)');
end;

type
  TVec = class                  { wird unten als Stackobjekt benutzt }
    X, Y: Real;
    constructor Create(AX, AY: Real);
    function Len: Real;
  end;
constructor TVec.Create(AX, AY: Real); begin inherited Create; X := AX; Y := AY; end;
function TVec.Len: Real; begin Result := Sqrt(X * X + Y * Y); end;

procedure Scale(V: TVec; F: Real);   { Ausleihe genuegt, um die Felder zu aendern }
begin V.X := V.X * F; V.Y := V.Y * F; end;

var
  list: TList;                   { Heapobjekte: Besitzer, von Hand freigeben }
  item: TItem;
  popped: optional TItem;
val
  found: borrowed optional TItem;
  i: Integer;
  pv: borrowed TVec;
  v: TVec;                       { Stackobjekt }
begin
  list := TList.Create;
  for i := 1 to 5 do list.Push(TItem.Create(i * 10));
  item := TItem.Create(99);
  list.Push(item);               { item ist danach verschoben }
  item := TItem.Create(7);       { neu belegen ist erlaubt }
  list.Push(item);
  PrintList(list);
  found := list.Find(30);
  if found is some f then WriteLn('gefunden: ', f.Value);
  popped := list.PopFront;
  if popped is some q then WriteLn('entnommen: ', q.Value);
  popped.Free;
  PrintList(list);
  list.Free;

  v.Create(3, 4);
  pv := v;                       { Ausleihe eines Stackobjekts }
  Scale(pv, 2);
  WriteLn('Laenge: ', pv.Len:0:1);
end.
