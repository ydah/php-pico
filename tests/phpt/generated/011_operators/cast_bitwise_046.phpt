--TEST--
casts comparisons and bitwise operators vector 046
--FILE--
<?php
$a = 91; $b = 8; $text = '417.75';
echo ($a & $b), ':', ($a | $b), ':', ($a ^ $b), ':', ($a < $b ? 1 : 0), ':', ($a >= $b ? 1 : 0), ':', (int)$text, ':', (string)417, ':', ((bool)$text ? 1 : 0), ':', ((bool)'0' ? 1 : 0);
--EXPECT--
8:91:83:0:1:417:417:1:0
