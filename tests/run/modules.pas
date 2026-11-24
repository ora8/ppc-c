program Module;
{ eigene Module: 'import Geometrie' laedt Geometrie.pas aus demselben Verzeichnis }
import ppc_std, Geometrie;

val
  a: TPunkt := (X: 0; Y: 0);
  b: TPunkt := (X: 3; Y: 4);
vao figuren := TListe<TFigur>.Create;
begin
  WriteLn('Abstand: ', a.Abstand(b):0:1);
  figuren.Add(TKreis.Create(1));
  figuren.Add(TKreis.Create(2));
  WriteLn('Flaeche: ', Gesamtflaeche(figuren):0:2);
  if figuren.Hole(0) is some f then WriteLn(f.Name, ' ', f.ClassName);
end.
