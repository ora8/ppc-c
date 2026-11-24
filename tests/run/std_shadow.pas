program StdShadow;
{ names of the program hide those of the library; the library keeps using its own }
import ppc_std;
type
  TStack = record Height: Integer; end;          { hides TStack<T> of ppc_std }

function IndexOf(x: Integer): String;              { hides IndexOf<T> }
begin Result := 'own IndexOf ' + IntToStr(x); end;

val
  s: TStack := (Height: 3);
  a: array of Integer := (3, 1, 2);
vao l := TList<Integer>.Create;
begin
  WriteLn(s.Height, ' ', IndexOf(5));
  Sort(a);
  WriteLn(a[0], a[1], a[2]);
  l.Add(4);
  WriteLn(l.Count);
end.
