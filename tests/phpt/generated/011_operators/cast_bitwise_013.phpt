--TEST--
casts comparisons and bitwise operators vector 013
--FILE--
<?php
$a = 43; $b = 29; $text = '120.75';
echo ($a & $b), ':', ($a | $b), ':', ($a ^ $b), ':', ($a < $b ? 1 : 0), ':', ($a >= $b ? 1 : 0), ':', (int)$text, ':', (string)120, ':', ((bool)$text ? 1 : 0), ':', ((bool)'0' ? 1 : 0);
--EXPECT--
9:63:54:0:1:120:120:1:0
