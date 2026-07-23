--TEST--
casts comparisons and bitwise operators vector 041
--FILE--
<?php
$a = 26; $b = 36; $text = '372.75';
echo ($a & $b), ':', ($a | $b), ':', ($a ^ $b), ':', ($a < $b ? 1 : 0), ':', ($a >= $b ? 1 : 0), ':', (int)$text, ':', (string)372, ':', ((bool)$text ? 1 : 0), ':', ((bool)'0' ? 1 : 0);
--EXPECT--
0:62:62:1:0:372:372:1:0
