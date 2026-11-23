program StackVsHeap;
{ Gleiche Daten, zwei Speichermodelle:
    TStackZaehler = object  -> Wert, liegt auf dem Stack
    THeapZaehler  = class   -> Referenz, liegt auf dem Heap }
type
  TStackZaehler = object
    Wert: Integer;
    procedure Erhoehe;
  end;

  THeapZaehler = class
    Wert: Integer;
    procedure Erhoehe;
  end;

procedure TStackZaehler.Erhoehe; begin Wert := Wert + 1; end;
procedure THeapZaehler.Erhoehe; begin Wert := Wert + 1; end;

{ Wertparameter: Stackobjekt wird KOPIERT, Klasse wird nur als Referenz uebergeben }
procedure ErhoeheStack(Z: TStackZaehler); begin Z.Erhoehe; end;
procedure ErhoeheHeap(Z: THeapZaehler);   begin Z.Erhoehe; end;

{ var-Parameter: auch das Stackobjekt wird direkt veraendert }
procedure ErhoeheStackVar(var Z: TStackZaehler); begin Z.Erhoehe; end;

procedure Lebensdauer;
var s: TStackZaehler;
    h: THeapZaehler;
begin
  s.Wert := 1;                         { sofort benutzbar, kein Create }
  h := THeapZaehler.Create;            { muss erzeugt werden }
  h.Wert := 1;
  WriteLn('  in Lebensdauer: s=', s.Wert, ' h=', h.Wert);
  h.Free;                              { muss freigegeben werden }
end;                                   { s verschwindet hier von selbst }

var
  s1, s2: TStackZaehler;
  h1: THeapZaehler;
  h2: borrowed THeapZaehler;
begin
  WriteLn('1) Erzeugen');
  s1.Wert := 0;                        { s1 existiert schon (Stack/Datensegment) }
  h1 := THeapZaehler.Create;           { h1 ist zuerst nil, Create holt Heap-Speicher }
  WriteLn('   SizeOf(TStackZaehler) = ', SizeOf(TStackZaehler), ' Bytes (das Objekt selbst)');
  WriteLn('   SizeOf(THeapZaehler)  = ', SizeOf(THeapZaehler), ' Bytes (nur ein Zeiger)');

  WriteLn('2) Zuweisung');
  s2 := s1;                            { Kopie der Daten }
  s2.Erhoehe;
  WriteLn('   Stack: s1=', s1.Wert, ' s2=', s2.Wert, '   -> zwei unabhaengige Objekte');
  h2 := h1;                            { Kopie der Referenz, gleiches Objekt }
  h2.Erhoehe;
  WriteLn('   Heap:  h1=', h1.Wert, ' h2=', h2.Wert, '   -> ein gemeinsames Objekt');

  WriteLn('3) Uebergabe als Wertparameter');
  ErhoeheStack(s1);
  WriteLn('   Stack: s1=', s1.Wert, '   (Prozedur hat eine Kopie veraendert)');
  ErhoeheHeap(h1);
  WriteLn('   Heap:  h1=', h1.Wert, '   (Prozedur hat das Original veraendert)');
  ErhoeheStackVar(s1);
  WriteLn('   Stack per var: s1=', s1.Wert);

  WriteLn('4) Lebensdauer');
  Lebensdauer;

  WriteLn('5) Freigeben');
  h1.Free;                             { nur das Heapobjekt braucht Free }
  WriteLn('   h1 freigegeben, s1/s2 brauchen nichts');
end.
