--TEST--
casts comparisons and bitwise operators vector 017
--FILE--
<?php
$a = 95; $b = 57; $text = '156.75';
echo ($a & $b), ':', ($a | $b), ':', ($a ^ $b), ':', ($a < $b ? 1 : 0), ':', ($a >= $b ? 1 : 0), ':', (int)$text, ':', (string)156, ':', ((bool)$text ? 1 : 0), ':', ((bool)'0' ? 1 : 0);
--EXPECT--
25:127:102:0:1:156:156:1:0
