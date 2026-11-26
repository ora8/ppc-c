program TypePatterns;
{* type patterns: case on the class of an object, also in if ... is TCircle(c) and in case expressions *}
type
  TFigure = class
    X: Integer;
    constructor Create(AX: Integer);
    function Name: String; virtual; const;
  end;
  TCircle = class(TFigure) R: Real; constructor Create(AX: Integer; AR: Real); end;
  TSquare = class(TFigure) S: Integer; constructor Create(AX, AS: Integer); end;
  TCube = class(TSquare) end;
constructor TFigure.Create(AX: Integer); begin inherited Create; X := AX; end;
function TFigure.Name: String; const; begin Result := ClassName; end;
constructor TCircle.Create(AX: Integer; AR: Real); begin inherited Create(AX); R := AR; end;
constructor TSquare.Create(AX, AS: Integer); begin inherited Create(AX); S := AS; end;

procedure Show(f: TFigure);
begin
  case f of
    TCircle(c) when c.R > 10: WriteLn('big circle ', c.R:0:1);
    TCircle(c): WriteLn('circle ', c.R:0:1);
    TCube(q): WriteLn('cube ', q.S);
    TSquare(q): WriteLn('square ', q.S, ' at ', q.X);
  else WriteLn('figure ', f.Name);
  end;
end;

procedure Grow(var f: TFigure);
begin
  case f of
    TSquare(var q): q.S := q.S * 2;
    TCircle: WriteLn('circles do not grow');
  else
  end;
end;

function Area(f: TFigure): Real;
begin
  Result := case f of
    TCircle(c) when c.R > 10: -1;
    TCircle(c): 3 * c.R * c.R;
    TSquare(q): q.S * q.S;
  else 0
  end;
end;

function Describe(f: TFigure): String;
begin
  Result := case f of TCube: 'cube'; TSquare(q): 'square of ' + IntToStr(q.S); TFigure: 'figure' end;
end;

function Make(k: Integer): TFigure;
begin
  if k = 1 then Result := TCircle.Create(1, 2.5) else Result := TSquare.Create(2, 3);
end;

vao a: TFigure := TCircle.Create(1, 20.0);
vao b: TFigure := TSquare.Create(5, 4);
vao c: TFigure := TCube.Create(6, 2);
vao d: TFigure := TFigure.Create(7);
vao e: TFigure := TCircle.Create(8, 1.5);
begin
  Show(a); Show(b); Show(c); Show(d); Show(e);
  Grow(b); Show(b); Grow(a);
  case Make(1) of                     {* a fresh object: owned by the case, freed after it *}
    TCircle(k): WriteLn('made circle ', k.R:0:1);
  else WriteLn('made other');
  end;
  if b is TSquare(q) then WriteLn('b is a square of ', q.S) else WriteLn('b is no square');
  if a is TSquare(q) then WriteLn('a is a square') else WriteLn('a is no square');
  WriteLn(Area(a):0:1, ' ', Area(b):0:1, ' ', Area(c):0:1, ' ', Area(d):0:1, ' ', Area(e):0:2);
  WriteLn(Describe(a), ', ', Describe(b), ', ', Describe(c));
  case d of
    TFigure(f): WriteLn('every figure: ', f.X);
  end;
end.
