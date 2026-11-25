program overloads;
type
  TShape = class
    constructor Create; overload;
    constructor Create(AName: String); overload;
    function Describe: String; overload; virtual;
    function Describe(Prefix: String): String; overload;
    Name: String;
  end;
  TCircle = class(TShape)
    R: Real;
    function Describe: String; override;
    function Describe(Times: Integer): String; overload;
  end;
  TVec = record
    X, Y: Real;
    procedure Add(d: Real); overload;
    procedure Add(v: TVec); overload;
  end;
function DupeStr(s: String; n: Integer): String;
begin Result := ''; for val i := 1 to n do Result := Result + s; end;
constructor TShape.Create; begin Name := 'shape'; end;
constructor TShape.Create(AName: String); begin Name := AName; end;
function TShape.Describe: String; begin Result := 'a ' + Name; end;
function TShape.Describe(Prefix: String): String; begin Result := Prefix + Describe; end;
function TCircle.Describe: String; begin Result := 'circle ' + Name; end;
function TCircle.Describe(Times: Integer): String; begin Result := DupeStr(Describe, Times); end;
procedure TVec.Add(d: Real); begin X := X + d; Y := Y + d; end;
procedure TVec.Add(v: TVec); begin X := X + v.X; Y := Y + v.Y; end;

function Show(i: Integer): String; overload; forward;
function Show(s: String): String; overload;
begin Result := 'string ' + s; end;
function Show(r: Real): String; overload;
begin Result := 'real ' + FloatToStr(r); end;
function Show(i: Integer): String;
begin Result := 'integer ' + IntToStr(i); end;
function Show(a, b: Integer): String; overload;
begin Result := 'two ' + IntToStr(a + b); end;
function Show: String; overload;
begin Result := 'nothing'; end;
procedure Bump(var i: Integer); overload; begin Inc(i); end;
procedure Bump(var s: String); overload; begin s := s + '+'; end;

val
  i := 41;
  s := 'xy';
  v: TVec := (X: 1; Y: 2);
  w: TVec := (X: 10; Y: 20);
begin
  WriteLn(Show(5));
  WriteLn(Show('hi'));
  WriteLn(Show(2.5));
  WriteLn(Show('c'));
  WriteLn(Show(1, 2));
  WriteLn(Show);
  Bump(i); Bump(s);
  WriteLn(i, ' ', s);
  v.Add(1);
  v.Add(w);
  WriteLn(v.X:0:0, ' ', v.Y:0:0);
  vao a := TShape.Create;
  vao b := TShape.Create('box');
  vao c := TCircle.Create('ring');
  begin
    WriteLn(a.Describe, ' / ', b.Describe('-> '));
    WriteLn(c.Describe, ' / ', c.Describe('* '), ' / ', c.Describe(2));
  end;
end.
