program nested_scope;
begin
    val I := 5;
    begin
        val I: Integer := 6;
        begin
            WriteLn(I);
        end;
        WriteLn(I);
    end;
end.