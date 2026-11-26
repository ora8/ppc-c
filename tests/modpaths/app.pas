program App;
{* modules from other directories: ppc --module-paths modpaths/lib1:modpaths/lib2 modpaths/app.pas *}
import Shapes, Units, Version;
begin
  WriteLn('area ', Area(Square(3)):0:1);            {* Shapes (lib1), which imports Helper next to it *}
  WriteLn('length ', ToCm(2.0):0:1, ' cm');        {* Units (lib2, in the brace syntax) *}
  WriteLn('version from ', Where);                  {* in lib1 and lib2: the first path wins *}
end.
