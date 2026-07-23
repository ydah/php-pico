--TEST--
casts comparisons and bitwise operators vector 049
--FILE--
<?php
$a = 3; $b = 29; $text = '444.75';
echo ($a & $b), ':', ($a | $b), ':', ($a ^ $b), ':', ($a < $b ? 1 : 0), ':', ($a >= $b ? 1 : 0), ':', (int)$text, ':', (string)444, ':', ((bool)$text ? 1 : 0), ':', ((bool)'0' ? 1 : 0);
--EXPECT--
1:31:30:1:0:444:444:1:0
