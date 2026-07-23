--TEST--
integer arithmetic vector 015
--FILE--
<?php
$a = 75; $b = 23;
echo $a + $b, ':', $a - $b, ':', $a * $b, ':', $a % $b, ':', ($a + $b) * 2 - $b, ':', $a << 3;
--EXPECT--
98:52:1725:6:173:600
