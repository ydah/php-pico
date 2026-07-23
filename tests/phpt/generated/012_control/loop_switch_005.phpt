--TEST--
loop continue and switch vector 005
--FILE--
<?php
$total = 0; $seed = 26;
for ($i = 0; $i < 8; $i++) {
    if ($i % 3 === 1) { continue; }
    $total += $seed + $i;
}
switch (($seed + 8) % 4) {
case 0: $label = 'zero'; break;
case 1: $label = 'one'; break;
case 2: $label = 'two'; break;
default: $label = 'three';
}
echo $total, ':', $label;
--EXPECT--
146:two
