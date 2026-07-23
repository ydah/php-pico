--TEST--
casts comparisons and bitwise operators vector 015
--FILE--
<?php
$a = 69; $b = 43; $text = '138.75';
echo ($a & $b), ':', ($a | $b), ':', ($a ^ $b), ':', ($a < $b ? 1 : 0), ':', ($a >= $b ? 1 : 0), ':', (int)$text, ':', (string)138, ':', ((bool)$text ? 1 : 0), ':', ((bool)'0' ? 1 : 0);
--EXPECT--
1:111:110:0:1:138:138:1:0
