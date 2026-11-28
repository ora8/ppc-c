{* EXPECT: TW.Execute runs in its own thread and must not use the global variable 'log' (through 'Note'): threads do not share variables (give the data to the thread object) *}
program T;
import std;
type TW = class(TThread) N: Integer; procedure Execute; override; end;
vao log := TList<String>.Create;
procedure Note(s: String); begin log.Add(s); end;
procedure TW.Execute; begin Note(IntToStr(N)); end;
begin end.
