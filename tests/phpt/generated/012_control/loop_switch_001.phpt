--TEST--
loop continue and switch vector 001
--FILE--
<?php
$total = 0; $seed = 6;
for ($i = 0; $i < 4; $i++) {
    if ($i % 3 === 1) { continue; }
    $total += $seed + $i;
}
switch (($seed + 4) % 4) {
case 0: $label = 'zero'; break;
case 1: $label = 'one'; break;
case 2: $label = 'two'; break;
default: $label = 'three';
}
echo $total, ':', $label;
--EXPECT--
23:two
