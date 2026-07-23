--TEST--
casts comparisons and bitwise operators vector 003
--FILE--
<?php
$a = 40; $b = 22; $text = '30.75';
echo ($a & $b), ':', ($a | $b), ':', ($a ^ $b), ':', ($a < $b ? 1 : 0), ':', ($a >= $b ? 1 : 0), ':', (int)$text, ':', (string)30, ':', ((bool)$text ? 1 : 0), ':', ((bool)'0' ? 1 : 0);
--EXPECT--
0:62:62:0:1:30:30:1:0
