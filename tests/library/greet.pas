library Greet;
{* a second library with classes of its own: two Perfect Pascal libraries can be linked into one
   program (their internal names, e.g. the method tables, do not collide) *}
import std;

type
  TGreeter = class
    Count: Integer;
    function Greet(name: String): String;
  end;

function TGreeter.Greet(name: String): String;
begin
  Inc(Count);
  Result := 'Hello, ' + name + '!';
end;

vao greeter := TGreeter.Create;

function GreetCount(name: String): Integer;
begin
  WriteLn(greeter.Greet(name));
  Result := greeter.Count;
end;

exports GreetCount;

begin
end.
