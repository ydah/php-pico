--TEST--
casts comparisons and bitwise operators vector 034
--FILE--
<?php
$a = 62; $b = 50; $text = '309.75';
echo ($a & $b), ':', ($a | $b), ':', ($a ^ $b), ':', ($a < $b ? 1 : 0), ':', ($a >= $b ? 1 : 0), ':', (int)$text, ':', (string)309, ':', ((bool)$text ? 1 : 0), ':', ((bool)'0' ? 1 : 0);
--EXPECT--
50:62:12:0:1:309:309:1:0
