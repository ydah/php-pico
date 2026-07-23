--TEST--
casts comparisons and bitwise operators vector 016
--FILE--
<?php
$a = 82; $b = 50; $text = '147.75';
echo ($a & $b), ':', ($a | $b), ':', ($a ^ $b), ':', ($a < $b ? 1 : 0), ':', ($a >= $b ? 1 : 0), ':', (int)$text, ':', (string)147, ':', ((bool)$text ? 1 : 0), ':', ((bool)'0' ? 1 : 0);
--EXPECT--
18:114:96:0:1:147:147:1:0
