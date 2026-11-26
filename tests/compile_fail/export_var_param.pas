{* EXPECT: 'F' cannot be exported: its parameter 'x' is a 'var' parameter *}
library T; procedure F(var x: Integer); begin x := 1; end; exports F; begin end.
