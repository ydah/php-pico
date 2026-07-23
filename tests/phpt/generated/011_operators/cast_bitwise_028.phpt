--TEST--
casts comparisons and bitwise operators vector 028
--FILE--
<?php
$a = 111; $b = 8; $text = '255.75';
echo ($a & $b), ':', ($a | $b), ':', ($a ^ $b), ':', ($a < $b ? 1 : 0), ':', ($a >= $b ? 1 : 0), ':', (int)$text, ':', (string)255, ':', ((bool)$text ? 1 : 0), ':', ((bool)'0' ? 1 : 0);
--EXPECT--
8:111:103:0:1:255:255:1:0
