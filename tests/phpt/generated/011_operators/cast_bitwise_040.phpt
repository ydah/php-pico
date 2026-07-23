--TEST--
casts comparisons and bitwise operators vector 040
--FILE--
<?php
$a = 13; $b = 29; $text = '363.75';
echo ($a & $b), ':', ($a | $b), ':', ($a ^ $b), ':', ($a < $b ? 1 : 0), ':', ($a >= $b ? 1 : 0), ':', (int)$text, ':', (string)363, ':', ((bool)$text ? 1 : 0), ':', ((bool)'0' ? 1 : 0);
--EXPECT--
13:29:16:1:0:363:363:1:0
