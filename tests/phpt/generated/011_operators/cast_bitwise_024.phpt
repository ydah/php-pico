--TEST--
casts comparisons and bitwise operators vector 024
--FILE--
<?php
$a = 59; $b = 43; $text = '219.75';
echo ($a & $b), ':', ($a | $b), ':', ($a ^ $b), ':', ($a < $b ? 1 : 0), ':', ($a >= $b ? 1 : 0), ':', (int)$text, ':', (string)219, ':', ((bool)$text ? 1 : 0), ':', ((bool)'0' ? 1 : 0);
--EXPECT--
43:59:16:0:1:219:219:1:0
