program CaseExpressions;
{* case as an expression: every arm gives a value, the case must handle every case (or have an 'else') *}
type
  TShape = (Circle(Radius: Real), Rect(W, H: Real), Named(Name: String; Sides: Integer), Empty);
  TColor = (Red, Green, Blue);
  TNode = class V: Integer; constructor Create(AV: Integer); end;
constructor TNode.Create(AV: Integer); begin inherited Create; V := AV; end;

function Area(s: TShape): Real;
begin
  Result := case s of
    Circle(r): 3.0 * r * r;
    Rect(w, h): w * h;
    Named(_, k): k;
    Empty: 0;
  end;
end;

function Kind(s: TShape): String;
begin
  Result := case s of
    Circle(r) when r > 10: 'big circle';
    Circle: 'circle';
    Rect(w, h) when w = h: 'square';
    Named(n, _): 'named ' + n;
  else 'other'
  end;
end;

function Divide(a, b: Integer): result Integer;
begin if b = 0 then Result := error('div by zero') else Result := ok(a div b); end;

function Find(k: Integer): optional TNode;
begin if k > 0 then Result := TNode.Create(k) else Result := none; end;

val o: optional Integer := 4;
val c: TColor := Green;
vao node := Find(3);
begin
  WriteLn(Area(Circle(1.0)):0:1, ' ', Area(Rect(2.0, 3.0)):0:1, ' ', Area(Named('x', 5)):0:1, ' ', Area(Empty):0:1);
  WriteLn(Kind(Circle(20.0)), ', ', Kind(Circle(1.0)), ', ', Kind(Rect(2.0, 2.0)), ', ', Kind(Rect(1.0, 2.0)), ', ', Kind(Named('hex', 6)));
  WriteLn(case Divide(7, 2) of ok v: v * 10; error e: -1 end);
  WriteLn(case Divide(7, 0) of ok v: 'ok ' + IntToStr(v); error e: 'failed: ' + e end);
  WriteLn(case o of some v when v > 3: 'big'; some v: 'small'; none: 'none' end);
  WriteLn(case node of some n: n.V; none: 0 end);
  WriteLn(case c of Red: 'red'; Green, Blue: 'green or blue' else '?' end);
  for val i := 1 to 5 do
    WriteLn(i, ' ', case i of 1: 'one'; 2..3 when Odd(i): 'odd'; 2..4: 'small' else 'many' end);
  val name := case c of Red: 'R' else 'notR' end;
  begin
    WriteLn(name, ' ', Length(name));
  end;
end.
