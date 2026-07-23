--TEST--
casts comparisons and bitwise operators vector 045
--FILE--
<?php
$a = 78; $b = 1; $text = '408.75';
echo ($a & $b), ':', ($a | $b), ':', ($a ^ $b), ':', ($a < $b ? 1 : 0), ':', ($a >= $b ? 1 : 0), ':', (int)$text, ':', (string)408, ':', ((bool)$text ? 1 : 0), ':', ((bool)'0' ? 1 : 0);
--EXPECT--
0:79:79:0:1:408:408:1:0
