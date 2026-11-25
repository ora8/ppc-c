program std_more;
import ppc_std;
val
  parts: array of String := default;
  lines: array of String := default;
begin
  WriteLn('[', Trim('  hi there '#9), '] [', TrimLeft('  x '), '] [', TrimRight(' y  '), ']');
  WriteLn(StartsWith('hello', 'he'), ' ', EndsWith('hello', 'lo'), ' ', Contains('hello', 'ell'), ' ', Contains('hello', 'z'));
  WriteLn(PosFrom('l', 'hello', 4), ' ', ReplaceStr('a-b-c', '-', '+'), ' ', ReverseString('abc'), ' ', DupeString('ab', 3));
  parts := Split('a,b,,c', ',');
  WriteLn(Length(parts), ' ', Join(parts, '|'));
  WriteLn(PadLeft('7', 3, '0'), ' ', PadRight('x', 3, '.'), '| ', IntToHex(255, 4));
  if TryStrToInt(' 42 ') is some n then WriteLn(n + 1);
  WriteLn(TryStrToInt('4x').HasValue, ' ', StrToFloat('2.5') * 2:0:1);
  WriteLn(IsDigit('5'), IsLetter('q'), IsSpace(' '));
  WriteLn(Min(3, 7), ' ', Max('Anna', 'Bert'), ' ', EnsureRange(15, 0, 10), ' ', Pi:0:4);
  WriteLn(Floor(-2.5), ' ', Ceil(2.1), ' ', Power(2, 10):0:0, ' ', Log10(1000):0:1, ' ', Sign(-3.0), ' ', Hypot(3, 4):0:1);
  case WriteFile('ppc_std_more.tmp', 'one'#10'two'#13#10) of
    ok: WriteLn('written');
    error e: WriteLn(e);
  end;
  if AppendFile('ppc_std_more.tmp', 'three') is error e then WriteLn(e);
  val rl := ReadLines('ppc_std_more.tmp');
  begin
    if rl is ok ls then WriteLn(Length(ls), ' ', Join(ls, ','));
  end;
  if ReadFile('/nonexistent/x') is error e then WriteLn(e);
  WriteLn(FileExists('ppc_std_more.tmp'));
  if DeleteFile('ppc_std_more.tmp') is ok then WriteLn('deleted', ' ', FileExists('ppc_std_more.tmp'));
  val t0 := MilliSeconds;
  begin
    Sleep(20);
    WriteLn(MilliSeconds - t0 >= 20, ' ', UnixTime > 1700000000, ' ', Now.Year >= 2026, ' ', Length(DateTimeToStr(Now)));
  end;
  WriteLn(ParamCount, ' ', ParamStr(1), ' ', GetEnv('PPC_SURELY_UNSET_VAR'), '.');
end.
