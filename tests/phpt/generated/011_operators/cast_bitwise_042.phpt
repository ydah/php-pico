--TEST--
casts comparisons and bitwise operators vector 042
--FILE--
<?php
$a = 39; $b = 43; $text = '381.75';
echo ($a & $b), ':', ($a | $b), ':', ($a ^ $b), ':', ($a < $b ? 1 : 0), ':', ($a >= $b ? 1 : 0), ':', (int)$text, ':', (string)381, ':', ((bool)$text ? 1 : 0), ':', ((bool)'0' ? 1 : 0);
--EXPECT--
35:47:12:1:0:381:381:1:0
