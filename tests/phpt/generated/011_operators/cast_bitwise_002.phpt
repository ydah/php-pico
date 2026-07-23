--TEST--
casts comparisons and bitwise operators vector 002
--FILE--
<?php
$a = 27; $b = 15; $text = '21.75';
echo ($a & $b), ':', ($a | $b), ':', ($a ^ $b), ':', ($a < $b ? 1 : 0), ':', ($a >= $b ? 1 : 0), ':', (int)$text, ':', (string)21, ':', ((bool)$text ? 1 : 0), ':', ((bool)'0' ? 1 : 0);
--EXPECT--
11:31:20:0:1:21:21:1:0
