--TEST--
integer arithmetic vector 025
--FILE--
<?php
$a = 54; $b = 14;
echo $a + $b, ':', $a - $b, ':', $a * $b, ':', $a % $b, ':', ($a + $b) * 2 - $b, ':', $a << 1;
--EXPECT--
68:40:756:12:122:108
