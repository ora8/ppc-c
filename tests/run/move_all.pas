program MoveAll;
{ moving objects between val (stack), vao (heap) and var (borrow) }
type
  TK = class
    N: Integer;
    Part: optional TK;
    constructor Create(AN: Integer);
    destructor Destroy; override;
    function Clone: TK; const; auto;
  end;
  TL = class(TK) end;
constructor TK.Create(AN: Integer); begin inherited Create; N := AN; end;
destructor TK.Destroy; begin WriteLn('  Destroy ', N); inherited Destroy; end;
function Make(n: Integer): TK; begin Result := TK.Create(n); end;

procedure ShowAll;
vao h := TK.Create(1);
val s := TK.Create(2);
begin
  h.Part := TK.Create(10);
  val a := h;                          { vao -> val: the heap object moves onto the stack }
  begin
    if a.Part is some t then WriteLn('a = ', a.N, ' / ', t.N);
    vao b := a;                         { val -> vao: onto the heap again }
    begin
      WriteLn('b = ', b.N);
    end;
  end;
  val c := Make(3);                    { function result directly onto the stack }
  val d := c;                          { val -> val }
  var r: TK := d;                      { val -> var: borrow }
  begin
    WriteLn('r = ', r.N, ', s = ', s.N);
    val e := r.Clone;                  { var -> val: only as a copy }
    begin
      WriteLn('e = ', e.N);
    end;
  end;
end;

begin
  ShowAll;
end.
