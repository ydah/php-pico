--TEST--
casts comparisons and bitwise operators vector 047
--FILE--
<?php
$a = 104; $b = 15; $text = '426.75';
echo ($a & $b), ':', ($a | $b), ':', ($a ^ $b), ':', ($a < $b ? 1 : 0), ':', ($a >= $b ? 1 : 0), ':', (int)$text, ':', (string)426, ':', ((bool)$text ? 1 : 0), ':', ((bool)'0' ? 1 : 0);
--EXPECT--
8:111:103:0:1:426:426:1:0
