{* EXPECT: 'exports' is only possible in a library *}
program T; function F: Integer; begin Result := 1; end; exports F; begin end.
