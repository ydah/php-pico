--TEST--
casts comparisons and bitwise operators vector 027
--FILE--
<?php
$a = 98; $b = 1; $text = '246.75';
echo ($a & $b), ':', ($a | $b), ':', ($a ^ $b), ':', ($a < $b ? 1 : 0), ':', ($a >= $b ? 1 : 0), ':', (int)$text, ':', (string)246, ':', ((bool)$text ? 1 : 0), ':', ((bool)'0' ? 1 : 0);
--EXPECT--
0:99:99:0:1:246:246:1:0
