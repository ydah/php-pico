--TEST--
casts comparisons and bitwise operators vector 010
--FILE--
<?php
$a = 4; $b = 8; $text = '93.75';
echo ($a & $b), ':', ($a | $b), ':', ($a ^ $b), ':', ($a < $b ? 1 : 0), ':', ($a >= $b ? 1 : 0), ':', (int)$text, ':', (string)93, ':', ((bool)$text ? 1 : 0), ':', ((bool)'0' ? 1 : 0);
--EXPECT--
0:12:12:1:0:93:93:1:0
