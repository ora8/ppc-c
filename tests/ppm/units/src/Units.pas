module Units;
interface
function ToCm(inch: Real): Real;
implementation
function ToCm(inch: Real): Real;
begin
  Result := inch * 2.54;
end;
end.
