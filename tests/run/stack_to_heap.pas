program StackToHeap;
{* moving a stack object to the heap: vao h := k, Result := k, vao parameter (like Box::new) *}
type
  TK = class
    N: Integer;
    Part: optional TK;
    constructor Create(AN: Integer);
    destructor Destroy; override;
  end;
  TList = class
    Head: optional TK;
    procedure Add(vao k: TK);
  end;
constructor TK.Create(AN: Integer); begin inherited Create; N := AN; end;
destructor TK.Destroy; begin WriteLn('  Destroy ', N); inherited Destroy; end;
procedure TList.Add(vao k: TK); begin k.Part := Take(Head); Head := k; end;

vao global: optional TK := none;

function Build(n: Integer): TK;
val k := TK.Create(n);                 {* build it on the stack ... *}
begin
  k.Part := TK.Create(n + 100);
  Result := k;                          {* ... and move it to the heap *}
end;

procedure P(flag: Boolean);
val a := TK.Create(1);
val b := TK.Create(2);
vao l := TList.Create;
begin
  vao h := a;                           {* a moves to the heap *}
  begin
    WriteLn('h = ', h.N);
    global := h;
  end;
  if flag then l.Add(b);                {* b moved on one path only *}
  WriteLn('P end');
end;

begin
  P(True);
  P(False);
  vao x := Build(5);
  begin
    if x.Part is some t then WriteLn('Build: ', x.N, ' / ', t.N);
  end;
  if global is some g then WriteLn('global: ', g.N);
end.
