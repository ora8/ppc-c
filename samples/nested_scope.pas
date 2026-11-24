program nested_scope;

begin
    val I = 5;
    begin
        val I: Integer;
        begin
            I := 6;
            WriteLn(I);
        end;
        WriteLn(I);
    end;
end.