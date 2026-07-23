--TEST--
integer arithmetic vector 080
--FILE--
<?php
$a = 34; $b = 22;
echo $a + $b, ':', $a - $b, ':', $a * $b, ':', $a % $b, ':', ($a + $b) * 2 - $b, ':', $a << 0;
--EXPECT--
56:12:748:12:90:34
