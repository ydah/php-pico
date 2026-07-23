--TEST--
integer arithmetic vector 094
--FILE--
<?php
$a = 81; $b = 14;
echo $a + $b, ':', $a - $b, ':', $a * $b, ':', $a % $b, ':', ($a + $b) * 2 - $b, ':', $a << 2;
--EXPECT--
95:67:1134:11:176:324
