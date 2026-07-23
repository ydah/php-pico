--TEST--
integer arithmetic vector 088
--FILE--
<?php
$a = 170; $b = 24;
echo $a + $b, ':', $a - $b, ':', $a * $b, ':', $a % $b, ':', ($a + $b) * 2 - $b, ':', $a << 0;
--EXPECT--
194:146:4080:2:364:170
