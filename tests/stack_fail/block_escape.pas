{ EXPECT: borrowed reference 'r' is used after its owner 's' went out of scope }
program T; type TFoo = class V: Integer; constructor Create; end; constructor TFoo.Create; begin inherited Create; end;
val r: borrowed optional TFoo;
begin val s: TFoo; begin s.Create; r := s; end; if r is some x then WriteLn(x.V); end.
