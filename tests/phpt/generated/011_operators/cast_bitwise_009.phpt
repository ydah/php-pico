--TEST--
casts comparisons and bitwise operators vector 009
--FILE--
<?php
$a = 118; $b = 1; $text = '84.75';
echo ($a & $b), ':', ($a | $b), ':', ($a ^ $b), ':', ($a < $b ? 1 : 0), ':', ($a >= $b ? 1 : 0), ':', (int)$text, ':', (string)84, ':', ((bool)$text ? 1 : 0), ':', ((bool)'0' ? 1 : 0);
--EXPECT--
0:119:119:0:1:84:84:1:0
