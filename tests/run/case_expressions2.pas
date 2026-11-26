program CaseExpressions2;
{ case expressions: nested, with guards, moves in arms, in a lambda }
type TShape = (Circle(Radius: Real), Named(Name: String), Empty);
  TK = class V: Integer; constructor Create(AV: Integer); end;
constructor TK.Create(AV: Integer); begin inherited Create; V := AV; end;
function Eat(vao k: TK): Integer; begin Result := k.V; end;
function Up(x: String): String; begin Result := UpperCase(x) + '!'; end;
function Label(s: TShape): String;
begin
  Result := case s of
    Named(n) when Length(n) > 3: Up(n) + Up(case Length(n) of 4: 'four' else 'long' end);
    Named(n): Up(n);
    Circle(r): FloatToStr(r);
    Empty: ''
  end;
end;
procedure P(s: TShape);
vao k := TK.Create(5);
begin
  WriteLn(case s of Circle(r) when r > 1: Eat(k); Circle: Eat(k) + 1 else 0 end);
end;
function Apply(f: reference to function(x: Integer): String; x: Integer): String; begin Result := f(x); end;
procedure L(limit: Integer);
begin
  WriteLn(Apply(function(x: Integer): String begin Result := case x of 0: 'zero' else (case x > limit of True: 'big' else 'small' end) end; end, 7));
end;
begin
  for val i := 1 to 3 do WriteLn(Label(Named('abcd')), ' ', Label(Named('ab')), ' ', Label(Named('abcdefg')), ' [', Label(Empty), ']');
  P(Circle(2.0)); P(Circle(0.5)); P(Empty);
  L(5); L(10);
end.
