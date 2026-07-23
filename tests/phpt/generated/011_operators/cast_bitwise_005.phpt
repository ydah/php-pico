--TEST--
casts comparisons and bitwise operators vector 005
--FILE--
<?php
$a = 66; $b = 36; $text = '48.75';
echo ($a & $b), ':', ($a | $b), ':', ($a ^ $b), ':', ($a < $b ? 1 : 0), ':', ($a >= $b ? 1 : 0), ':', (int)$text, ':', (string)48, ':', ((bool)$text ? 1 : 0), ':', ((bool)'0' ? 1 : 0);
--EXPECT--
0:102:102:0:1:48:48:1:0
