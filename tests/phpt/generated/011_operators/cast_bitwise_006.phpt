--TEST--
casts comparisons and bitwise operators vector 006
--FILE--
<?php
$a = 79; $b = 43; $text = '57.75';
echo ($a & $b), ':', ($a | $b), ':', ($a ^ $b), ':', ($a < $b ? 1 : 0), ':', ($a >= $b ? 1 : 0), ':', (int)$text, ':', (string)57, ':', ((bool)$text ? 1 : 0), ':', ((bool)'0' ? 1 : 0);
--EXPECT--
11:111:100:0:1:57:57:1:0
