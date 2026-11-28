{* EXPECT: use of moved value 'w' (ownership was transferred away) *}
program T;
import std;
type TW = class(TThread) N: Integer; procedure Execute; override; end;
procedure TW.Execute; begin Inc(N); end;
vao w := TW.Create;
begin
  vao r := Start(w);
  begin
    w.N := 5;
  end;
end.
