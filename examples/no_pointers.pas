program NoPointers;
{ Typical pointer tasks of classic Pascal - in ppc without ^, @, New, Dispose
  and without nil:
    1. linked list                -> heap objects with an 'optional' successor
    2. binary search tree         -> optional subtrees, unpacking with 'is some'
    3. changing values "by pointer" -> var parameters
    4. reference to an object     -> 'var' (reference without ownership)
    5. a result that may be missing -> 'optional Integer' (like Option<i32>) }
type
  { ---------------------------------------------------- 1. list }
  TNode = class
    Value: Integer;
    Next: optional TNode;             { owns the successor; none = end of the list }
    constructor Create(W: Integer);
  end;

  TList = class
    Head: optional TNode;
    Size: Integer;
    procedure PushFront(W: Integer);
    procedure Reverse;
    function Sum: Integer;
    function Position(W: Integer): optional Integer;
    procedure Show;
  end;

  { ---------------------------------------------------- 2. tree }
  TTree = class
    Value: Integer;
    Left, Right: optional TTree;      { owns the subtrees }
    constructor Create(W: Integer);
    procedure InsertValue(W: Integer);
    function Contains(W: Integer): Boolean;
    procedure InOrder;
    function Height: Integer;
  end;

  { ---------------------------------------------------- 4. reference }
  TPoint = class
    X, Y: Integer;
    constructor Create(AX, AY: Integer);
  end;

{ ======================================================= TNode / TList }
constructor TNode.Create(W: Integer); begin inherited Create; Value := W; end;

procedure TList.PushFront(W: Integer);
vao
  k: TNode := TNode.Create(W);
begin
  
  k.Next := Take(Head);                 { the old chain moves behind the new node }
  Head := k;                            { Move: k owns nothing afterwards }
  Inc(Size);
end;

procedure TList.Reverse;              { classic pointer relinking, only with moves }
vao
  rest, fresh: optional TNode := none;
begin
  fresh := none;
  rest := Take(Head);
  while Take(rest) is some k do begin   { k owns the front node }
    rest := Take(k.Next);
    k.Next := fresh;                      { put it in front of the new list }
    fresh := k;
  end;
  Head := fresh;
end;

function TList.Sum: Integer;
var p: optional TNode := none;       { borrow for walking through }
begin
  Result := 0;
  p := Head;
  while p is some k do begin            { like: while let Some(k) = p }
    Result := Result + k.Value;
    p := k.Next;
  end;
end;

function TList.Position(W: Integer): optional Integer;
var p: optional TNode := none;
    val i: Integer := default;
begin
  Result := none;
  p := Head;
  i := 1;
  while p is some k do begin
    if k.Value = W then Exit(i);         { Integer becomes 'some i' automatically }
    p := k.Next;
    Inc(i);
  end;
end;

procedure TList.Show;
var p: optional TNode := none;
begin
  p := Head;
  while p is some k do begin Write(k.Value, ' '); p := k.Next; end;
  WriteLn('(', Size, ' Items, Sum ', Sum, ')');
end;


{ ======================================================= TTree }
constructor TTree.Create(W: Integer); begin inherited Create; Value := W; end;

procedure TTree.InsertValue(W: Integer);
begin
  if W < Value then begin
    if Left is some l then l.InsertValue(W) else Left := TTree.Create(W);
  end else if W > Value then begin
    if Right is some r then r.InsertValue(W) else Right := TTree.Create(W);
  end;
end;

function TTree.Contains(W: Integer): Boolean;
begin
  Result := W = Value;
  if (W < Value) and (Left is some l) then Result := l.Contains(W)
  else if (W > Value) and (Right is some r) then Result := r.Contains(W);
end;

procedure TTree.InOrder;
begin
  if Left is some l then l.InOrder;
  Write(Value, ' ');
  if Right is some r then r.InOrder;
end;

function TTree.Height: Integer;
val hl, hr: Integer := default;
begin
  hl := 0; hr := 0;
  if Left is some l then hl := l.Height;
  if Right is some r then hr := r.Height;
  if hl > hr then Result := hl + 1 else Result := hr + 1;
end;

{ ======================================================= 3. var parameters }
procedure Swap(var A, B: Integer);   { instead of Swap(pa, pb: ^Integer) }
val t: Integer := default;
begin t := A; A := B; B := t; end;

{ ======================================================= 4. reference }
constructor TPoint.Create(AX, AY: Integer); begin inherited Create; X := AX; Y := AY; end;

procedure Shift(P: TPoint; DX: Integer);   { gets a reference, not a ^TPoint }
begin P.X := P.X + DX; end;

vao
  list := TList.Create;             { vao: owns the object }
  tree := TTree.Create(50);
  pt := TPoint.Create(3, 4);
var
  ref: TPoint := pt;                 { reference to it (instead of @pt) }
val
  a, b, i: Integer := default;
begin
  WriteLn('--- 1. linked list ---');
  
  for i := 1 to 5 do list.PushFront(i * 10);
  list.Show;
  list.Reverse;
  list.Show;

  WriteLn('--- 5. optional Integer ---');
  case list.Position(30) of            { like: match list.position(30) }
    some pos: WriteLn('30 is at position ', pos);
    none: WriteLn('30 not found');
  end;
  case list.Position(99) of
    some pos: WriteLn('99 is at position ', pos);
    none: WriteLn('99 not found');
  end;
  WriteLn('Position of 50 or 0: ', list.Position(50).ValueOr(0));

  WriteLn('--- 2. binary search tree ---');
  
  tree.InsertValue(30); tree.InsertValue(70); tree.InsertValue(20);
  tree.InsertValue(40); tree.InsertValue(60); tree.InsertValue(80); tree.InsertValue(35);
  Write('sorted: '); tree.InOrder; WriteLn;
  WriteLn('contains 40: ', tree.Contains(40), ', contains 45: ', tree.Contains(45), ', Height: ', tree.Height);

  WriteLn('--- 3. var parameters instead of pointers ---');
  a := 1; b := 2;
  Swap(a, b);
  WriteLn('a=', a, ' b=', b);

  WriteLn('--- 4. reference to an object ---');
  Shift(ref, 10);                  { like @pt, but checked for safety }
  WriteLn('pt = (', pt.X, ',', pt.Y, ')');
end.
