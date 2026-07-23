--TEST--
casts comparisons and bitwise operators vector 021
--FILE--
<?php
$a = 20; $b = 22; $text = '192.75';
echo ($a & $b), ':', ($a | $b), ':', ($a ^ $b), ':', ($a < $b ? 1 : 0), ':', ($a >= $b ? 1 : 0), ':', (int)$text, ':', (string)192, ':', ((bool)$text ? 1 : 0), ':', ((bool)'0' ? 1 : 0);
--EXPECT--
20:22:2:1:0:192:192:1:0
