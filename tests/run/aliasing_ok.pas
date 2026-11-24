program AliasingOk;
{ borrowing rules like in Rust: what is allowed }
type
  TNode = class
    Value: Integer;
    Left, Right: optional TNode;
    constructor Create(W: Integer);
    procedure Increment;
    function Info: String; const;
  end;
constructor TNode.Create(W: Integer); begin inherited Create; Value := W; end;
procedure TNode.Increment; begin Inc(Value); end;
function TNode.Info: String; const; begin Result := IntToStr(Value); end;

vao
  a := TNode.Create(1);
begin
  a.Left := TNode.Create(10);
  a.Right := TNode.Create(20);
  var
    l1: TNode const := a;      { several read-only borrows at the same time }
    l2: TNode const := a;
  begin
    WriteLn('two readers: ', l1.Info, ' ', l2.Info, ' ', a.Info);   { reading is compatible with readers }
  end;
  var m: TNode := a;            { one changeable borrow }
  begin
    m.Increment;
    m.Increment;
  end;
  WriteLn('after m: ', a.Info);            { m is over, a can be used freely again }
  var links: optional TNode := a.Left;   { borrow of a field ... }
  begin
    if a.Right is some r then r.Increment; { ... another field may be changed }
    if links is some x then begin x.Increment; WriteLn('left: ', x.Info); end;
    if a.Right is some r then WriteLn('right: ', r.Info);
  end;
end.
