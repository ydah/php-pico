--TEST--
casts comparisons and bitwise operators vector 008
--FILE--
<?php
$a = 105; $b = 57; $text = '75.75';
echo ($a & $b), ':', ($a | $b), ':', ($a ^ $b), ':', ($a < $b ? 1 : 0), ':', ($a >= $b ? 1 : 0), ':', (int)$text, ':', (string)75, ':', ((bool)$text ? 1 : 0), ':', ((bool)'0' ? 1 : 0);
--EXPECT--
41:121:80:0:1:75:75:1:0
