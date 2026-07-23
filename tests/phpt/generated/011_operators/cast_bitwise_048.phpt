--TEST--
casts comparisons and bitwise operators vector 048
--FILE--
<?php
$a = 117; $b = 22; $text = '435.75';
echo ($a & $b), ':', ($a | $b), ':', ($a ^ $b), ':', ($a < $b ? 1 : 0), ':', ($a >= $b ? 1 : 0), ':', (int)$text, ':', (string)435, ':', ((bool)$text ? 1 : 0), ':', ((bool)'0' ? 1 : 0);
--EXPECT--
20:119:99:0:1:435:435:1:0
