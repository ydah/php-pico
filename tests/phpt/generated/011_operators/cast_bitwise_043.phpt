--TEST--
casts comparisons and bitwise operators vector 043
--FILE--
<?php
$a = 52; $b = 50; $text = '390.75';
echo ($a & $b), ':', ($a | $b), ':', ($a ^ $b), ':', ($a < $b ? 1 : 0), ':', ($a >= $b ? 1 : 0), ':', (int)$text, ':', (string)390, ':', ((bool)$text ? 1 : 0), ':', ((bool)'0' ? 1 : 0);
--EXPECT--
48:54:6:0:1:390:390:1:0
