{* EXPECT: Format: '%d' needs a whole number, but argument 1 is "String" *}
program T; val n := 3; s := 'ab'; f := '%d';
begin WriteLn(Format('%d', [s])); end.
