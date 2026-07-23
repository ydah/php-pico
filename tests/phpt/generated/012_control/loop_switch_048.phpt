--TEST--
loop continue and switch vector 048
--FILE--
<?php
$total = 0; $seed = 241;
for ($i = 0; $i < 3; $i++) {
    if ($i % 3 === 1) { continue; }
    $total += $seed + $i;
}
switch (($seed + 3) % 4) {
case 0: $label = 'zero'; break;
case 1: $label = 'one'; break;
case 2: $label = 'two'; break;
default: $label = 'three';
}
echo $total, ':', $label;
--EXPECT--
484:zero
