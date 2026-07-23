--TEST--
casts comparisons and bitwise operators vector 004
--FILE--
<?php
$a = 53; $b = 29; $text = '39.75';
echo ($a & $b), ':', ($a | $b), ':', ($a ^ $b), ':', ($a < $b ? 1 : 0), ':', ($a >= $b ? 1 : 0), ':', (int)$text, ':', (string)39, ':', ((bool)$text ? 1 : 0), ':', ((bool)'0' ? 1 : 0);
--EXPECT--
21:61:40:0:1:39:39:1:0
