program Inference;
type
  TFoo = class
    V: Integer;
    constructor Create(AV: Integer);
    destructor Destroy; override;
  end;
constructor TFoo.Create(AV: Integer); begin inherited Create; V := AV; end;
destructor TFoo.Destroy; begin WriteLn('  Destroy TFoo ', V); inherited Destroy; end;

procedure Demo;
vao
  I := 5;                     {* owned heap value, type Integer from the initializer *}
val
  N := 3.5;                   {* value, type Real *}
  S := 'Hello';               {* type String *}
begin
  I := I * 2;
  WriteLn('I = ', I, ', N = ', N:0:1, ', S = ', S);
end;

val
  i := 0;
begin
  Demo;
  begin
    WriteLn('without declarations: a normal begin block');
  end;
  val
    j: Integer := default;
    k := 10;
    vao s := TFoo.Create(1);                  {* owner in the block *}
  vao
    h := TFoo.Create(2);       
  begin
    j := i + k;
    
    WriteLn('in the block: j = ', j, ', s.V = ', s.V, ', h.V = ', h.V);
  end;                        {* s and h are freed here *}
  WriteLn('after the block');
  while i < 2 do
    val q := i * 100;
    begin
      WriteLn('loop: q = ', q);
      Inc(i);
    end;
end.
