--TEST--
casts comparisons and bitwise operators vector 000
--FILE--
<?php
$a = 1; $b = 1; $text = '3.75';
echo ($a & $b), ':', ($a | $b), ':', ($a ^ $b), ':', ($a < $b ? 1 : 0), ':', ($a >= $b ? 1 : 0), ':', (int)$text, ':', (string)3, ':', ((bool)$text ? 1 : 0), ':', ((bool)'0' ? 1 : 0);
--EXPECT--
1:1:0:0:1:3:3:1:0
