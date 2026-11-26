{* EXPECT: Format: '%s' needs a string, but argument 1 is "Integer" *}
program T; val n := 3; s := 'ab'; f := '%d';
begin WriteLn(Format('%s', [n])); end.
