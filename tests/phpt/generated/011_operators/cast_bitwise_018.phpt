--TEST--
casts comparisons and bitwise operators vector 018
--FILE--
<?php
$a = 108; $b = 1; $text = '165.75';
echo ($a & $b), ':', ($a | $b), ':', ($a ^ $b), ':', ($a < $b ? 1 : 0), ':', ($a >= $b ? 1 : 0), ':', (int)$text, ':', (string)165, ':', ((bool)$text ? 1 : 0), ':', ((bool)'0' ? 1 : 0);
--EXPECT--
0:109:109:0:1:165:165:1:0
