--TEST--
casts comparisons and bitwise operators vector 022
--FILE--
<?php
$a = 33; $b = 29; $text = '201.75';
echo ($a & $b), ':', ($a | $b), ':', ($a ^ $b), ':', ($a < $b ? 1 : 0), ':', ($a >= $b ? 1 : 0), ':', (int)$text, ':', (string)201, ':', ((bool)$text ? 1 : 0), ':', ((bool)'0' ? 1 : 0);
--EXPECT--
1:61:60:0:1:201:201:1:0
