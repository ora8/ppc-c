program HeapValues;
{* values on the heap: declared with 'vao', used in the code like a normal value and
  freed automatically at the end of the scope (like Box<T> in Rust) *}
type
  TVector = record
    X, Y: Real;
    procedure Scale(F: Real);
  end;
  TField = array[1..5] of Integer;

procedure TVector.Scale(F: Real); begin X := X * F; Y := Y * F; end;

procedure Duplicate(var N: Integer); begin N := N * 2; end;
function SquareOf(N: Integer): Integer; begin Result := N * N; end;

procedure Local;
vao
  Counter: Integer := 100;                   {* heap value in a procedure *}
begin
  Inc(Counter);
  WriteLn('local: ', Counter);
end;                                         {* Counter is freed here *}

procedure Early;
vao
  Buffer: TField := default;
begin
  Buffer[1] := 7;
  WriteLn('early: ', Buffer[1]);
  Buffer.Free;                               {* optional: free it already here *}
  Buffer := (1, 2, 3, 4, 5);                 {* a whole new value: creates a new cell *}
  WriteLn('again: ', Buffer[1], ' ', Buffer[5]);
end;

vao
  I: Integer := 20;
  S: String := 'Hello';
  V: TVector := (X: 3; Y: 4);
  F: TField := default;
begin
  I := I + 1;
  Duplicate(I);                   {* var parameter: reference to the heap value *}
  WriteLn('I = ', I, ', SquareOf = ', SquareOf(I));

  S := S + ' Welt';
  WriteLn('S = ', S, ' (', Length(S), ' characters)');

  V.Scale(2);                  {* method of a record on the heap *}
  WriteLn('V = (', V.X:0:1, ', ', V.Y:0:1, ')');

  for val k := 1 to 5 do F[k] := k * k;
  WriteLn('F[5] = ', F[5]);

  Local;
  Early;
end.                              {* I, S, V and F are freed here *}
