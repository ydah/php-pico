--TEST--
integer arithmetic vector 012
--FILE--
<?php
$a = 24; $b = 5;
echo $a + $b, ':', $a - $b, ':', $a * $b, ':', $a % $b, ':', ($a + $b) * 2 - $b, ':', $a << 0;
--EXPECT--
29:19:120:4:53:24
