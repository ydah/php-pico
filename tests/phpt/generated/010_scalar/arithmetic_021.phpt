--TEST--
integer arithmetic vector 021
--FILE--
<?php
$a = 177; $b = 13;
echo $a + $b, ':', $a - $b, ':', $a * $b, ':', $a % $b, ':', ($a + $b) * 2 - $b, ':', $a << 1;
--EXPECT--
190:164:2301:8:367:354
