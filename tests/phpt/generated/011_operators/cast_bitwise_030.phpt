--TEST--
casts comparisons and bitwise operators vector 030
--FILE--
<?php
$a = 10; $b = 22; $text = '273.75';
echo ($a & $b), ':', ($a | $b), ':', ($a ^ $b), ':', ($a < $b ? 1 : 0), ':', ($a >= $b ? 1 : 0), ':', (int)$text, ':', (string)273, ':', ((bool)$text ? 1 : 0), ':', ((bool)'0' ? 1 : 0);
--EXPECT--
2:30:28:1:0:273:273:1:0
