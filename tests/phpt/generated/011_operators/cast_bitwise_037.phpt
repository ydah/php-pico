--TEST--
casts comparisons and bitwise operators vector 037
--FILE--
<?php
$a = 101; $b = 8; $text = '336.75';
echo ($a & $b), ':', ($a | $b), ':', ($a ^ $b), ':', ($a < $b ? 1 : 0), ':', ($a >= $b ? 1 : 0), ':', (int)$text, ':', (string)336, ':', ((bool)$text ? 1 : 0), ':', ((bool)'0' ? 1 : 0);
--EXPECT--
0:109:109:0:1:336:336:1:0
