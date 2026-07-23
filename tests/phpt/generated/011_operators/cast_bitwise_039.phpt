--TEST--
casts comparisons and bitwise operators vector 039
--FILE--
<?php
$a = 127; $b = 22; $text = '354.75';
echo ($a & $b), ':', ($a | $b), ':', ($a ^ $b), ':', ($a < $b ? 1 : 0), ':', ($a >= $b ? 1 : 0), ':', (int)$text, ':', (string)354, ':', ((bool)$text ? 1 : 0), ':', ((bool)'0' ? 1 : 0);
--EXPECT--
22:127:105:0:1:354:354:1:0
