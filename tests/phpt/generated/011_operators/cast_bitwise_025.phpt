--TEST--
casts comparisons and bitwise operators vector 025
--FILE--
<?php
$a = 72; $b = 50; $text = '228.75';
echo ($a & $b), ':', ($a | $b), ':', ($a ^ $b), ':', ($a < $b ? 1 : 0), ':', ($a >= $b ? 1 : 0), ':', (int)$text, ':', (string)228, ':', ((bool)$text ? 1 : 0), ':', ((bool)'0' ? 1 : 0);
--EXPECT--
0:122:122:0:1:228:228:1:0
