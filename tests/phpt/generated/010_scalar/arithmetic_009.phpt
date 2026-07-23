--TEST--
integer arithmetic vector 009
--FILE--
<?php
$a = 164; $b = 10;
echo $a + $b, ':', $a - $b, ':', $a * $b, ':', $a % $b, ':', ($a + $b) * 2 - $b, ':', $a << 1;
--EXPECT--
174:154:1640:4:338:328
