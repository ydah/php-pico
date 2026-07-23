--TEST--
casts comparisons and bitwise operators vector 014
--FILE--
<?php
$a = 56; $b = 36; $text = '129.75';
echo ($a & $b), ':', ($a | $b), ':', ($a ^ $b), ':', ($a < $b ? 1 : 0), ':', ($a >= $b ? 1 : 0), ':', (int)$text, ':', (string)129, ':', ((bool)$text ? 1 : 0), ':', ((bool)'0' ? 1 : 0);
--EXPECT--
32:60:28:0:1:129:129:1:0
