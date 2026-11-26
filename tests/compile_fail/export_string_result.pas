{* EXPECT: its result is a "String": a String result (who would free it?) is not possible yet *}
library T; function F: String; begin Result := 'x'; end; exports F; begin end.
