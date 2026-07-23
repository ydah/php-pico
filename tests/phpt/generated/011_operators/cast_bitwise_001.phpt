--TEST--
casts comparisons and bitwise operators vector 001
--FILE--
<?php
$a = 14; $b = 8; $text = '12.75';
echo ($a & $b), ':', ($a | $b), ':', ($a ^ $b), ':', ($a < $b ? 1 : 0), ':', ($a >= $b ? 1 : 0), ':', (int)$text, ':', (string)12, ':', ((bool)$text ? 1 : 0), ':', ((bool)'0' ? 1 : 0);
--EXPECT--
8:14:6:0:1:12:12:1:0
