--TEST--
casts comparisons and bitwise operators vector 044
--FILE--
<?php
$a = 65; $b = 57; $text = '399.75';
echo ($a & $b), ':', ($a | $b), ':', ($a ^ $b), ':', ($a < $b ? 1 : 0), ':', ($a >= $b ? 1 : 0), ':', (int)$text, ':', (string)399, ':', ((bool)$text ? 1 : 0), ':', ((bool)'0' ? 1 : 0);
--EXPECT--
1:121:120:0:1:399:399:1:0
