--TEST--
integer arithmetic vector 067
--FILE--
<?php
$a = 195; $b = 13;
echo $a + $b, ':', $a - $b, ':', $a * $b, ':', $a % $b, ':', ($a + $b) * 2 - $b, ':', $a << 3;
--EXPECT--
208:182:2535:0:403:1560
