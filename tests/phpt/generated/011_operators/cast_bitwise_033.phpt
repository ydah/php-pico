--TEST--
casts comparisons and bitwise operators vector 033
--FILE--
<?php
$a = 49; $b = 43; $text = '300.75';
echo ($a & $b), ':', ($a | $b), ':', ($a ^ $b), ':', ($a < $b ? 1 : 0), ':', ($a >= $b ? 1 : 0), ':', (int)$text, ':', (string)300, ':', ((bool)$text ? 1 : 0), ':', ((bool)'0' ? 1 : 0);
--EXPECT--
33:59:26:0:1:300:300:1:0
