--TEST--
casts comparisons and bitwise operators vector 035
--FILE--
<?php
$a = 75; $b = 57; $text = '318.75';
echo ($a & $b), ':', ($a | $b), ':', ($a ^ $b), ':', ($a < $b ? 1 : 0), ':', ($a >= $b ? 1 : 0), ':', (int)$text, ':', (string)318, ':', ((bool)$text ? 1 : 0), ':', ((bool)'0' ? 1 : 0);
--EXPECT--
9:123:114:0:1:318:318:1:0
