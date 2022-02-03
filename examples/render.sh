UXNASM=../uxnasm
UXNSEQ=../uxnseq

$UXNASM sing.tal sing.rom
$UXNSEQ sing.lil

$UXNASM -g seq.tal seq.rom
$UXNSEQ seq.lil

$UXNASM -g rng.tal rng.rom
$UXNSEQ rng.lil

$UXNASM -g coord.tal coord.rom
$UXNSEQ coord.lil
