--TEST--
casts comparisons and bitwise operators vector 011
--FILE--
<?php
$a = 17; $b = 15; $text = '102.75';
echo ($a & $b), ':', ($a | $b), ':', ($a ^ $b), ':', ($a < $b ? 1 : 0), ':', ($a >= $b ? 1 : 0), ':', (int)$text, ':', (string)102, ':', ((bool)$text ? 1 : 0), ':', ((bool)'0' ? 1 : 0);
--EXPECT--
1:31:30:0:1:102:102:1:0
