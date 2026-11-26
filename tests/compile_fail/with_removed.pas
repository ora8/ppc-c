{* EXPECT: the 'with' statement is not supported: name the variable *}
program T; type TP = record X: Integer; end; val p: TP := default;
begin with p do X := 1; end.
