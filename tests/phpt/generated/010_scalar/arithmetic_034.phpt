--TEST--
integer arithmetic vector 034
--FILE--
<?php
$a = 16; $b = 22;
echo $a + $b, ':', $a - $b, ':', $a * $b, ':', $a % $b, ':', ($a + $b) * 2 - $b, ':', $a << 2;
--EXPECT--
38:-6:352:16:54:64
