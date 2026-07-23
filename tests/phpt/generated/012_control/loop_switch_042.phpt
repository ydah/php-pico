--TEST--
loop continue and switch vector 042
--FILE--
<?php
$total = 0; $seed = 211;
for ($i = 0; $i < 5; $i++) {
    if ($i % 3 === 1) { continue; }
    $total += $seed + $i;
}
switch (($seed + 5) % 4) {
case 0: $label = 'zero'; break;
case 1: $label = 'one'; break;
case 2: $label = 'two'; break;
default: $label = 'three';
}
echo $total, ':', $label;
--EXPECT--
638:zero
