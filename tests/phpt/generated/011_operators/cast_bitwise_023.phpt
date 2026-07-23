--TEST--
casts comparisons and bitwise operators vector 023
--FILE--
<?php
$a = 46; $b = 36; $text = '210.75';
echo ($a & $b), ':', ($a | $b), ':', ($a ^ $b), ':', ($a < $b ? 1 : 0), ':', ($a >= $b ? 1 : 0), ':', (int)$text, ':', (string)210, ':', ((bool)$text ? 1 : 0), ':', ((bool)'0' ? 1 : 0);
--EXPECT--
36:46:10:0:1:210:210:1:0
