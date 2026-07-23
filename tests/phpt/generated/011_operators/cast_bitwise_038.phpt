--TEST--
casts comparisons and bitwise operators vector 038
--FILE--
<?php
$a = 114; $b = 15; $text = '345.75';
echo ($a & $b), ':', ($a | $b), ':', ($a ^ $b), ':', ($a < $b ? 1 : 0), ':', ($a >= $b ? 1 : 0), ':', (int)$text, ':', (string)345, ':', ((bool)$text ? 1 : 0), ':', ((bool)'0' ? 1 : 0);
--EXPECT--
2:127:125:0:1:345:345:1:0
