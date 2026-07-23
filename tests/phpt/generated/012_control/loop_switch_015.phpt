--TEST--
loop continue and switch vector 015
--FILE--
<?php
$total = 0; $seed = 76;
for ($i = 0; $i < 10; $i++) {
    if ($i % 3 === 1) { continue; }
    $total += $seed + $i;
}
switch (($seed + 10) % 4) {
case 0: $label = 'zero'; break;
case 1: $label = 'one'; break;
case 2: $label = 'two'; break;
default: $label = 'three';
}
echo $total, ':', $label;
--EXPECT--
565:two
