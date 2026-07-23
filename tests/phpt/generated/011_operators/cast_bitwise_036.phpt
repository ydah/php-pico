--TEST--
casts comparisons and bitwise operators vector 036
--FILE--
<?php
$a = 88; $b = 1; $text = '327.75';
echo ($a & $b), ':', ($a | $b), ':', ($a ^ $b), ':', ($a < $b ? 1 : 0), ':', ($a >= $b ? 1 : 0), ':', (int)$text, ':', (string)327, ':', ((bool)$text ? 1 : 0), ':', ((bool)'0' ? 1 : 0);
--EXPECT--
0:89:89:0:1:327:327:1:0
