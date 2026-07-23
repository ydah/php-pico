--TEST--
casts comparisons and bitwise operators vector 026
--FILE--
<?php
$a = 85; $b = 57; $text = '237.75';
echo ($a & $b), ':', ($a | $b), ':', ($a ^ $b), ':', ($a < $b ? 1 : 0), ':', ($a >= $b ? 1 : 0), ':', (int)$text, ':', (string)237, ':', ((bool)$text ? 1 : 0), ':', ((bool)'0' ? 1 : 0);
--EXPECT--
17:125:108:0:1:237:237:1:0
