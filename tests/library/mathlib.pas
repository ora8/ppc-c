library MathLib;
{* a shared library: 'ppc mathlib.pas' makes libMathLib.so and the C header MathLib.h *}
import std;

type
  TColor = (Red, Green, Blue);
  TPoint = record
    X, Y: Integer;
  end;
  TRect = record
    TopLeft, BottomRight: TPoint;
    Visible: Boolean;
  end;

val calls: Integer := 0;        {* library state: kept between calls *}
vao words := TList<String>.Create;   {* an object of the library: freed when it is unloaded *}

function Add(a, b: Integer): Integer;
begin
  Inc(calls);
  Result := a + b;
end;

function Area(r: Real): Real;
begin
  Inc(calls);
  Result := Pi * r * r;
end;

function Fib(n: Integer): Int64;
begin
  if n < 2 then Result := n else Result := Fib(n - 1) + Fib(n - 2);
end;

function CountChar(s: String; c: Char): Integer;    {* a String parameter: a C string *}
begin
  Result := 0;
  for val ch in s do
    if ch = c then Inc(Result);
end;

function Shout(s: String): Integer;    {* the library can print and use std *}
begin
  WriteLn(UpperCase(s), '!');
  words.Add(s);
  Result := words.Count;
end;

function Greeting(name: String): String;   {* a String result: C gets a copy and frees it *}
begin
  Result := 'Hello, ' + UpperCase(name) + '!';
end;

function Middle(r: TRect): TPoint;   {* records by value *}
begin
  Result.X := (r.TopLeft.X + r.BottomRight.X) div 2;
  Result.Y := (r.TopLeft.Y + r.BottomRight.Y) div 2;
end;

function Width(r: TRect const): Integer;   {* a const record: a pointer in C *}
begin
  Result := r.BottomRight.X - r.TopLeft.X;
end;

function Next(c: TColor): TColor;
begin
  if c = Blue then Result := Red else Result := Succ(c);
end;

function IsEven(n: Integer): Boolean;
begin
  Result := not Odd(n);
end;

function Calls: Integer;
begin
  Result := calls;
end;

procedure Hello;
begin
  WriteLn('hello from Perfect Pascal');
end;

exports
  Add, Area name 'mathlib_area', Fib, CountChar, Shout, Greeting, Middle, Width, Next, IsEven, Calls, Hello;

begin
  WriteLn('MathLib loaded');
end.
