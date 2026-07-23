--TEST--
integer arithmetic vector 073
--FILE--
<?php
$a = 106; $b = 3;
echo $a + $b, ':', $a - $b, ':', $a * $b, ':', $a % $b, ':', ($a + $b) * 2 - $b, ':', $a << 1;
--EXPECT--
109:103:318:1:215:212
