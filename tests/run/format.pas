program format;
type TColor = (Red, Green, Blue);
val
  n := 3;
  name := 'Anna';
  price := 1234567.891;
  c := Green;
  fmt := 'dynamic %s=%d';
begin
  WriteLn(Format('%d items for %s', [n, name]));
  WriteLn(Format('[%5d] [%-5d] [%.3d] [%x] [%8.2f] [%e] [%n] [%g]', [42, 42, 7, 255, 3.14159, price, price, 0.5]));
  WriteLn(Format('[%-6s] [%6s] [%.2s] %s %s %s', ['ab', 'cd', 'xyz', True, 'c', c]));
  WriteLn(Format('%1:s %0:s %%', ['world', 'hello']));
  WriteLn(Format(fmt, ['k', 9]));
  WriteLn(Format('no args', []));
end.
