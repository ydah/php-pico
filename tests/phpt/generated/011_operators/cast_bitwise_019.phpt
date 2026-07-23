--TEST--
casts comparisons and bitwise operators vector 019
--FILE--
<?php
$a = 121; $b = 8; $text = '174.75';
echo ($a & $b), ':', ($a | $b), ':', ($a ^ $b), ':', ($a < $b ? 1 : 0), ':', ($a >= $b ? 1 : 0), ':', (int)$text, ':', (string)174, ':', ((bool)$text ? 1 : 0), ':', ((bool)'0' ? 1 : 0);
--EXPECT--
8:121:113:0:1:174:174:1:0
