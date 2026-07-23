--TEST--
casts comparisons and bitwise operators vector 031
--FILE--
<?php
$a = 23; $b = 29; $text = '282.75';
echo ($a & $b), ':', ($a | $b), ':', ($a ^ $b), ':', ($a < $b ? 1 : 0), ':', ($a >= $b ? 1 : 0), ':', (int)$text, ':', (string)282, ':', ((bool)$text ? 1 : 0), ':', ((bool)'0' ? 1 : 0);
--EXPECT--
21:31:10:1:0:282:282:1:0
