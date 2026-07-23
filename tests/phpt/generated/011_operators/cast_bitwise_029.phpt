--TEST--
casts comparisons and bitwise operators vector 029
--FILE--
<?php
$a = 124; $b = 15; $text = '264.75';
echo ($a & $b), ':', ($a | $b), ':', ($a ^ $b), ':', ($a < $b ? 1 : 0), ':', ($a >= $b ? 1 : 0), ':', (int)$text, ':', (string)264, ':', ((bool)$text ? 1 : 0), ':', ((bool)'0' ? 1 : 0);
--EXPECT--
12:127:115:0:1:264:264:1:0
