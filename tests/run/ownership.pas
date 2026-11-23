program Ownership;
type
  TItem = class
    Value: Integer;
    Next: nilable TItem;         { besitzt das naechste Element (nil am Ende) }
    constructor Create(V: Integer);
    destructor Destroy; override;
  end;
  TList = class
    Head: nilable TItem;
    Count: Integer;
    procedure Push(owned Item: TItem);   { uebernimmt den Besitz }
    function Find(V: Integer): borrowed nilable TItem;
    function PopFront: nilable TItem;    { gibt Besitz an Aufrufer }
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
  Item.Next := Take(Head);       { Head -> nil, Besitz wandert zu Item.Next }
  Head := Item;                  { Move des Parameters ins Feld }
  Inc(Count);
end;

function TList.Find(V: Integer): borrowed nilable TItem;
var cur: borrowed nilable TItem;
begin
  Result := nil;
  cur := Head;
  while cur <> nil do begin
    if cur.Value = V then begin Result := cur; Exit; end;
    cur := cur.Next;
  end;
end;

function TList.PopFront: nilable TItem;
begin
  Result := Take(Head);
  if Result <> nil then begin
    Head := Take(Result.Next);
    Dec(Count);
  end;
end;

destructor TList.Destroy;
begin
  Head.Free;
  inherited Destroy;
end;

procedure PrintList(L: TList);   { Wertparameter = Ausleihe }
var it: borrowed nilable TItem;
begin
  it := L.Head;
  while it <> nil do begin Write(it.Value, ' '); it := it.Next; end;
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

heap
  list: TList;                   { Heapobjekte: Besitzer, von Hand freigeben }
  item: TItem;
  popped: nilable TItem;
var
  found: borrowed nilable TItem;
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
  if found <> nil then WriteLn('gefunden: ', found.Value);
  popped := list.PopFront;
  if popped <> nil then WriteLn('entnommen: ', popped.Value);
  popped.Free;
  PrintList(list);
  list.Free;

  v.Create(3, 4);
  pv := v;                       { Ausleihe eines Stackobjekts }
  Scale(pv, 2);
  WriteLn('Laenge: ', pv.Len:0:1);
end.
