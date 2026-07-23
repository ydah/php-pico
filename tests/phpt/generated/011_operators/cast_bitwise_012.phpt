--TEST--
casts comparisons and bitwise operators vector 012
--FILE--
<?php
$a = 30; $b = 22; $text = '111.75';
echo ($a & $b), ':', ($a | $b), ':', ($a ^ $b), ':', ($a < $b ? 1 : 0), ':', ($a >= $b ? 1 : 0), ':', (int)$text, ':', (string)111, ':', ((bool)$text ? 1 : 0), ':', ((bool)'0' ? 1 : 0);
--EXPECT--
22:30:8:0:1:111:111:1:0
