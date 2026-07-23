--TEST--
casts comparisons and bitwise operators vector 007
--FILE--
<?php
$a = 92; $b = 50; $text = '66.75';
echo ($a & $b), ':', ($a | $b), ':', ($a ^ $b), ':', ($a < $b ? 1 : 0), ':', ($a >= $b ? 1 : 0), ':', (int)$text, ':', (string)66, ':', ((bool)$text ? 1 : 0), ':', ((bool)'0' ? 1 : 0);
--EXPECT--
16:126:110:0:1:66:66:1:0
