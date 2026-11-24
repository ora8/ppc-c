{ EXPECT: identifier not found "count" }
program T;
val Count: Integer := default;
begin Count := 1; count := 2; end.
