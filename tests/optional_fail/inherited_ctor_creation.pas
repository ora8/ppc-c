{ EXPECT: 'TB' cannot be created with the inherited constructor 'TA.Create': it does not initialize field 'F' of 'TB' }
program T; type TFoo = class V: Integer; constructor Create(AV: Integer); end; constructor TFoo.Create(AV: Integer); begin inherited Create; V := AV; end;
type TA = class constructor Create; end;
  TB = class(TA) F: TFoo; constructor Make; destructor Destroy; override; end;
constructor TA.Create; begin inherited Create; end;
constructor TB.Make; begin inherited Create; F := TFoo.Create(1); end;
destructor TB.Destroy; begin F.Free; inherited Destroy; end;
vao b: TB := TB.Create;
begin  b.Free; end.
