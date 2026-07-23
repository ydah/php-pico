--TEST--
casts comparisons and bitwise operators vector 032
--FILE--
<?php
$a = 36; $b = 36; $text = '291.75';
echo ($a & $b), ':', ($a | $b), ':', ($a ^ $b), ':', ($a < $b ? 1 : 0), ':', ($a >= $b ? 1 : 0), ':', (int)$text, ':', (string)291, ':', ((bool)$text ? 1 : 0), ':', ((bool)'0' ? 1 : 0);
--EXPECT--
36:36:0:0:1:291:291:1:0
