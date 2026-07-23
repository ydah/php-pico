--TEST--
casts comparisons and bitwise operators vector 020
--FILE--
<?php
$a = 7; $b = 15; $text = '183.75';
echo ($a & $b), ':', ($a | $b), ':', ($a ^ $b), ':', ($a < $b ? 1 : 0), ':', ($a >= $b ? 1 : 0), ':', (int)$text, ':', (string)183, ':', ((bool)$text ? 1 : 0), ':', ((bool)'0' ? 1 : 0);
--EXPECT--
7:15:8:1:0:183:183:1:0
